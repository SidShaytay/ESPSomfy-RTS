#include "Somfy.h"
#include "GroupMotion.h"
#include "BitLength.h"
#include "CounterCommandGuard.h"
#include "esp_log.h"
static const char *TAG = "Somfy";
extern SomfyShadeController somfy;

namespace {
class GroupMoveAdapter : public GroupMotionOps, public GroupStopOps {
  SomfyGroup &group;
  bool wireEndpoints = false;
  public:
    explicit GroupMoveAdapter(SomfyGroup &group, bool wire = false) : group(group), wireEndpoints(wire) {}
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
      shade->refreshTravel(millis());
      out.current = shade->state().currentPos;
      out.direction = shade->state().direction;
      out.stopPending = shade->hasStopPending();
      out.settling = shade->isSettling(millis());
      out.invertTarget = wireEndpoints ? shade->flipCommands != group.flipCommands : shade->shadeType == shade_types::awning;
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
      somfy.transceiver.cancelQueuedForRemote(group.getRemoteAddress(), sent.ticket);
      for(size_t i = 0; i < memberCount(); i++) {
        SomfyShade *shade = somfy.getShadeById(group.linkedShades[i]);
        somfy.cancelGroupStartsForShade(shade->getShadeId(), sent.ticket);
        somfy.transceiver.cancelQueuedForRemote(shade->getRemoteAddress());
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
      group.replanWireEndpoints = wireEndpoints;
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
bool stopTargetMembers(SomfyGroup &group) {
  GroupMoveAdapter adapter(group);
  return runGroupStop(adapter);
}
bool SomfyGroup::moveToTarget(float target, bool wireEndpoints) {
  if(!somfy.modelLoad.usable() || !(target >= 0 && target <= 100) ||
      !counterCommandAllowed(getRemoteAddress(), somfy_commands::UpDown)) return false;
  bool ended = false;
  for(uint8_t id : linkedShades) {
    if(!id) { ended = true; continue; }
    if(ended) return false;
  }
  GroupMoveAdapter adapter(*this, wireEndpoints);
  bool unfinished = false, sameTarget = true, settling = false, stopping = false;
  for(size_t i = 0; i < adapter.memberCount(); i++) {
    GroupMemberMotion m;
    if(!adapter.member(i, m) || !m.targetSupported || somfy.getShadeById(linkedShades[i])->hasPendingProgramming()) return false;
    SomfyShade *shade = somfy.getShadeById(this->linkedShades[i]);
    shade->awaitQueuedStart();
    if(shade->hasPendingGroupStart()) unfinished = true;
    if(shade->state().target != groupStoredTarget(m, target)) sameTarget = false;
    settling = settling || shade->isSettling(millis());
    stopping = stopping || shade->hasStopPending();
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
    this->replanWireEndpoints = wireEndpoints;
    this->replanPending = true;
    for(size_t i = 0; i < adapter.memberCount(); i++) somfy.getShadeById(this->linkedShades[i])->clearGroupPlanFailure();
    this->targetActive = true;
    this->updateTargetDirection();
    return true;
  }
  // Repeating an endpoint while a member is still taking up the final gaps
  // retains that run, including members that have already finished.
  if((target == 0 || target == 100) && sameTarget && settling && !stopping) return true;
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
  GroupMoveAdapter adapter(*this, this->replanWireEndpoints);
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
