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
  // A queued shared start cannot safely be cancelled and followed by My:
  // the motor may still be idle, when My recalls its favorite. Refuse until
  // its radio outcome is known; clients can issue a new request afterwards.
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPED_SHADES && this->linkedShades[i]; i++) {
    SomfyShade *shade = somfy.getShadeById(this->linkedShades[i]);
    if(commandSupersedesPositioning(cmd) && shade && shade->hasPendingGroupStart()) return false;
  }
  if(this->bitLength == 0) this->bitLength = somfy.transceiver.config.type;
  if((cmd == somfy_commands::My || cmd == somfy_commands::Stop) && this->targetActive) {
    // Members may have stopped at different times. A shared My now would
    // recall the favorite on one which has already stopped.
    const bool accepted = stopTargetMembers(*this);
    this->updateTargetDirection();
    return accepted;
  }
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
      out.current = shade->state().currentPos;
      out.invertTarget = shade->shadeType == shade_types::awning;
      out.flipCommands = shade->flipCommands;
      out.idle = shade->isIdle() && !shade->hasGroupTargetWork();
      const bool percentageType = shade->shadeType == shade_types::roller || shade->shadeType == shade_types::blind ||
        shade->shadeType == shade_types::awning || shade->shadeType == shade_types::shutter ||
        shade->shadeType == shade_types::ldrapery || shade->shadeType == shade_types::rdrapery || shade->shadeType == shade_types::cdrapery;
      // This operation controls lift percentages. Tilt-first and toggle motor
      // modes need a different stop/timing contract; refuse before any member
      // moves rather than simulating a percentage those motors cannot honor.
      out.targetSupported = !shade->hasPendingGroupStart() && percentageType && shade->tiltType == tilt_types::none &&
        group.proto == radio_proto::RTS && shade->proto == radio_proto::RTS &&
        shade->upTime > 0 && shade->downTime > 0;
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
};
}
static bool stopTargetMembers(SomfyGroup &group) {
  GroupMoveAdapter adapter(group);
  return runGroupStop(adapter);
}
bool SomfyGroup::moveToTarget(float target) {
  GroupMoveAdapter adapter(*this);
  const bool accepted = runGroupTarget(adapter, target);
  // Also track the individual fallback: group movement ends only after the
  // last member stops, not when the first member reaches its target.
  this->targetActive = true;
  this->updateTargetDirection();
  return accepted;
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
