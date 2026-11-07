#include "esp_log.h"
#include "Somfy.h"
#include "ModelEditor.h"
#include "GroupMotion.h"
#include "BitLength.h"

static const char *TAG = "Somfy";

extern SomfyShadeController somfy;
static bool stopTargetMembers(SomfyGroup &group);

void SomfyGroup::clear() {
  this->targetActive = false;
  this->replanPending = false;
  this->setGroupId(255);
  this->setRemoteAddress(0);
  this->repeats = 0;
  this->roomId = 0;
  this->name[0] = 0x00;
  memset(&this->linkedShades, 0x00, sizeof(this->linkedShades));
}
// False when the group is full.  The caller saves the change.
bool SomfyGroup::linkShade(uint8_t shadeId) { return addToList(this->linkedShades, SOMFY_MAX_GROUPED_SHADES, shadeId); }
// False when the shade wasn't in the group.  The caller saves the change.
bool SomfyGroup::unlinkShade(uint8_t shadeId) { return removeFromList(this->linkedShades, SOMFY_MAX_GROUPED_SHADES, shadeId); }
// Linked shade ids are kept packed so a search can stop at the first 0.
void SomfyGroup::compressLinkedShadeIds() { packList(this->linkedShades, SOMFY_MAX_GROUPED_SHADES); }
bool SomfyGroup::hasShadeId(uint8_t shadeId) {
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPED_SHADES; i++) {
    if(this->linkedShades[i] == 0) break;
    if(this->linkedShades[i] == shadeId) return true;
  }
  return false;
}
void SomfyGroup::updateFlags() { 
  uint8_t oldFlags = this->flags;
  this->flags = 0;
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPED_SHADES; i++) {
    if(this->linkedShades[i] != 0) {
      SomfyShade *shade = somfy.getShadeById(this->linkedShades[i]);
      if(shade) this->flags |= shade->getFlags();
    }
    else break;
  }
  if(oldFlags != this->flags) this->emitState();
}
int8_t SomfyGroup::p_direction(int8_t dir) {
  int8_t old = this->direction;
  if(old != dir) {
    this->direction = dir;
    this->changed |= GROUP_CHANGED_DIRECTION;
  }
  return old;
}
bool SomfyGroup::sendCommand(somfy_commands cmd) { return this->sendCommand(cmd, this->repeats); }
bool SomfyGroup::sendCommand(somfy_commands cmd, uint8_t repeat, uint8_t stepSize) {
  ESP_LOGI(TAG, "[Group %u] sendCommand cmd=%s repeat=%u", this->getGroupId(), translateSomfyCommand(cmd).c_str(), repeat);
  // This sendCommand function will always be called externally. sendCommand at the remote level
  // is expected to be called internally when the motor needs commanded.
  const bool stop = cmd == somfy_commands::My || cmd == somfy_commands::Stop;
  bool unfinished = false; // a member start is queued or not yet fully on air
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPED_SHADES && this->linkedShades[i]; i++) {
    SomfyShade *shade = somfy.getShadeById(this->linkedShades[i]);
    if(shade && (shade->hasPendingGroupStart() || shade->startQueuedOnly())) unfinished = true;
  }
  if(commandSupersedesPositioning(cmd)) this->cancelReplan();
  if(this->bitLength == 0) this->bitLength = somfy.transceiver.config.type;
  if(stop && (this->targetActive || unfinished)) {
    // A shared My would recall the favorite on a member which has already
    // stopped or never started. Drop a shared start still queued, cancel
    // queued member starts, and stop only members actually moving, after an
    // unfinished start's outcome is known.
    somfy.transceiver.cancelTx(this->getRemoteAddress());
    const bool accepted = stopTargetMembers(*this);
    this->updateTargetDirection();
    return accepted;
  }
  // Up/Down replace an unfinished start; other commands wait for its outcome.
  if(unfinished && commandSupersedesPositioning(cmd) && cmd != somfy_commands::Up && cmd != somfy_commands::Down) return false;
  const send_result_t sent = this->sendCommandAfter(cmd, repeat, stepSize, 0);
  if(!sent.sent) return false;
  if(commandSupersedesPositioning(cmd)) {
    somfy.transceiver.cancelTx(this->getRemoteAddress(), sent.ticket);
    for(uint8_t i = 0; i < SOMFY_MAX_GROUPED_SHADES && this->linkedShades[i]; i++) {
      SomfyShade *shade = somfy.getShadeById(this->linkedShades[i]);
      if(shade) {
        somfy.cancelGroupStartsForShade(shade->getShadeId(), sent.ticket);
        somfy.transceiver.cancelTx(shade->getRemoteAddress());
        shade->cancelPositioning();
      }
    }
    this->targetActive = false;
  }

  switch(cmd) {
    case somfy_commands::My:
      this->p_direction(0);
      break;
    case somfy_commands::Up:
      this->p_direction(-1);
      break;
    case somfy_commands::Down:
      this->p_direction(1);
      break;
    default:
      break;
  }
  
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPED_SHADES; i++) {
    if(this->linkedShades[i] != 0) {
      SomfyShade *shade = somfy.getShadeById(this->linkedShades[i]);
      if(shade) {
        ESP_LOGI(TAG, "[Group %u] processInternalCommand on shade %u cmd=%s", this->getGroupId(), shade->getShadeId(), translateSomfyCommand(cmd).c_str());
        shade->processInternalCommand(shade->transformCommand(this->transformCommand(cmd)), repeat);
        if(sent.ticket && startsMovement(this->transformCommand(cmd))) {
          shade->markTxPending(sent.ticket);
          shade->observeGroupStart(sent.ticket);
        }
        shade->emitCommand(cmd, "group", this->getRemoteAddress());
      }
    }
  }
  this->updateFlags();
  this->emitState();
  return true;
}
bool SomfyGroup::save() { somfy.commit(); return true; }

namespace {
class GroupMoveAdapter : public GroupMotionOps, public GroupStopOps {
  SomfyGroup &group;
  public:
    explicit GroupMoveAdapter(SomfyGroup &group) : group(group) {}
    size_t memberCount() override {
      size_t n = 0;
      while(n < SOMFY_MAX_GROUPED_SHADES && group.linkedShades[n]) n++;
      return n;
    }
    bool memberIdle(size_t index) override {
      SomfyShade *shade = somfy.getShadeById(group.linkedShades[index]);
      return !shade || (shade->isIdle() && !shade->hasGroupTargetWork());
    }
    bool stopMember(size_t index) override {
      SomfyShade *shade = somfy.getShadeById(group.linkedShades[index]);
      return shade && shade->stopGroupTarget();
    }
    bool member(size_t index, GroupMemberMotion &out) override {
      SomfyShade *shade = somfy.getShadeById(group.linkedShades[index]);
      if(!shade) return false;
      shade->refreshTravel();
      out.current = shade->state().currentPos;
      out.direction = shade->state().direction;
      out.stopPending = shade->hasStopPending();
      out.invertTarget = shade->shadeType == shade_types::awning;
      out.flipCommands = shade->flipCommands;
      // A press still queued has not moved the motor, which is idle.
      out.idle = (shade->isIdle() && !shade->hasGroupTargetWork()) || shade->startQueuedOnly();
      // This operation controls lift percentages. Tilt-first and toggle motor
      // modes need a different stop/timing contract; refuse before any member
      // moves rather than simulating a percentage those motors cannot honor.
      out.targetSupported = shade->supportsGroupTarget() &&
        group.proto == radio_proto::RTS && shade->proto == radio_proto::RTS;
      // Different motor modes require their existing individual planner.
      // In particular a group press cannot express tilt-before-lift timing.
      const uint8_t groupBits = group.bitLength ? group.bitLength : radioBitLength;
      const uint8_t shadeBits = shade->bitLength ? shade->bitLength : radioBitLength;
      out.supported = group.proto == radio_proto::RTS && group.getRemoteAddress() != 0 &&
        groupBits == shadeBits && supportedBitLength(groupBits) &&
        shade->proto == radio_proto::RTS && shade->tiltType == tilt_types::none &&
        (shade->shadeType == shade_types::roller || shade->shadeType == shade_types::shutter ||
         shade->shadeType == shade_types::ldrapery || shade->shadeType == shade_types::rdrapery ||
         shade->shadeType == shade_types::cdrapery) && shade->upTime > 0 && shade->downTime > 0 &&
        !shade->sunSensor && !(shade->state().stateFlags & static_cast<uint8_t>(somfy_flags_t::Windy));
      return true;
    }
    bool startGroup(int wireDirection, float target) override {
      const somfy_commands wire = wireDirection < 0 ? somfy_commands::Up : somfy_commands::Down;
      const send_result_t sent = group.sendCommandAfter(group.transformCommand(wire), group.repeats, 0, 0);
      if(!sent.sent) return false;
      // Only accepted replacement commands supersede the old movement. Keep
      // the new shared job while removing every stale queued stop/start.
      somfy.transceiver.cancelTx(group.getRemoteAddress(), sent.ticket);
      for(size_t i = 0; i < memberCount(); i++) {
        SomfyShade *shade = somfy.getShadeById(group.linkedShades[i]);
        somfy.cancelGroupStartsForShade(shade->getShadeId(), sent.ticket);
        somfy.transceiver.cancelTx(shade->getRemoteAddress());
        shade->cancelPositioning();
      }
      for(size_t i = 0; i < memberCount(); i++) {
        SomfyShade *shade = somfy.getShadeById(group.linkedShades[i]);
        GroupMemberMotion m; member(i,m);
        shade->adoptGroupTarget(groupStoredTarget(m,target), sent.ticket);
        shade->emitCommand(wire, "group", group.getRemoteAddress());
      }
      group.targetActive = true;
      group.updateTargetDirection();
      return true;
    }
    bool moveMember(size_t index, float storedTarget) override {
      SomfyShade *shade = somfy.getShadeById(group.linkedShades[index]);
      return shade->moveToGroupTarget(storedTarget);
    }
    void keepMember(size_t index) override {
      SomfyShade *shade = somfy.getShadeById(group.linkedShades[index]);
      if(shade) shade->cancelQueuedStart();
    }
    bool deferUntilStopped(float target) override {
      group.replanTarget = target;
      group.replanPending = true;
      for(size_t i = 0; i < memberCount(); i++) {
        SomfyShade *shade = somfy.getShadeById(group.linkedShades[i]);
        if(shade) shade->clearGroupPlanFailure();
      }
      return true;
    }
    void cancelDeferred() override { group.replanPending = false; }
    bool stopMovingMember(size_t index) override { return stopMember(index); }
};
}
static bool stopTargetMembers(SomfyGroup &group) {
  GroupMoveAdapter adapter(group);
  return runGroupStop(adapter);
}
bool SomfyGroup::moveToTarget(float target) {
  if(!(target >= 0 && target <= 100)) return false;
  GroupMoveAdapter adapter(*this);
  bool unfinished = false, sameTarget = true;
  for(size_t i = 0; i < adapter.memberCount(); i++) {
    GroupMemberMotion m;
    if(!adapter.member(i, m) || !m.targetSupported) return false;
    SomfyShade *shade = somfy.getShadeById(this->linkedShades[i]);
    if(shade->hasPendingGroupStart()) unfinished = true;
    if(shade->state().target != groupStoredTarget(m, target)) sameTarget = false;
  }
  if(unfinished && sameTarget) {
    // The unfinished press already heads every member to this target, so a
    // repeat (or a return to it) needs nothing more.
    this->replanPending = false;
    this->targetActive = true;
    return true;
  }
  if(unfinished) {
    // Sliders send their final target before the start finishes. Keep that
    // press and plan only the latest target from where the members are once
    // its outcome is known: no new start, rolling code or early My now.
    this->replanTarget = target;
    this->replanPending = true;
    for(size_t i = 0; i < adapter.memberCount(); i++) somfy.getShadeById(this->linkedShades[i])->clearGroupPlanFailure();
    this->targetActive = true;
    this->updateTargetDirection();
    return true;
  }
  this->replanPending = false;
  const bool accepted = runGroupTarget(adapter, target);
  // Also track the individual fallback: group movement ends only after the
  // last member stops, not when the first member reaches its target.
  this->targetActive = true;
  this->updateTargetDirection();
  return accepted;
}
void SomfyGroup::serviceTarget() {
  if(!this->replanPending) return;
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPED_SHADES && this->linkedShades[i]; i++) {
    SomfyShade *shade = somfy.getShadeById(this->linkedShades[i]);
    if(!shade) continue;
    if(shade->groupPlanFailed()) {
      // A start or stop did not fully go on air, so where the motor is (and
      // whether it moves) is unknown. Do not follow it with another press.
      ESP_LOGW(TAG, "[Group %u] Waiting group target dropped: a member start or stop did not complete", this->getGroupId());
      this->replanPending = false;
      return;
    }
    if(shade->hasPendingGroupStart() || shade->hasStopPending()) return; // wait for the outcome
  }
  this->replanPending = false;
  this->targetActive = true;
  GroupMoveAdapter adapter(*this);
  if(!runGroupTarget(adapter, this->replanTarget))
    ESP_LOGW(TAG, "[Group %u] Waiting group target refused", this->getGroupId());
  this->updateTargetDirection();
}
void SomfyGroup::updateTargetDirection() {
  if(!this->targetActive) return;
  int8_t direction = 0;
  bool moving = false;
  bool mixed = false;
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPED_SHADES && this->linkedShades[i]; i++) {
    SomfyShade *shade = somfy.getShadeById(this->linkedShades[i]);
    if(!shade || (shade->isIdle() && !shade->hasGroupTargetWork())) continue;
    // While a stop waits for airtime the estimate may pass the requested
    // target; it still travels in its actual direction until that stop starts.
    int8_t dir = shade->state().direction;
    if(shade->flipCommands != this->flipCommands) dir = -dir;
    if(moving && direction != dir) mixed = true;
    direction = dir;
    moving = true;
  }
  this->p_direction(moving && !mixed ? direction : 0);
  if(!moving) this->targetActive = false;
}
