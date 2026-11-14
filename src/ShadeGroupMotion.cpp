#include "esp_log.h"
static const char *TAG = "Somfy";
#include "ShadeBehaviour.h"
#include "GroupMotion.h"

void ShadeBehaviour::cancelPositioning() {
  // Cancelling a queued stop leaves a moving motor running to its limit.
  // Forget its ticket so a late dropped outcome cannot revive an old plan.
  if(this->tx.stopTicket && !this->groupStopOnAir) {
    this->runToLimit(this->tx.stopDir, this->tx.stopTilt, this->tx.stopAt);
  }
  else if(this->tx.hold.held()) {
    this->p_target(this->s.currentPos);
    this->p_tiltTarget(this->s.currentTiltPos);
    this->p_direction(0);
    this->p_tiltDirection(0);
  }
  else {
    if(this->s.direction) this->p_target(limitTarget(this->s.direction));
    if(this->s.tiltDirection) this->p_tiltTarget(limitTarget(this->s.tiltDirection));
  }
  this->settingPos = this->settingTiltPos = this->settingMyPos = false;
  // Programming has its own accepted ticket and rollback snapshot. Dropping
  // a queued movement must not erase the eventual programming outcome.
  const auto pendingMy = this->tx;
  this->tx = ShadeTxState{};
  this->tx.myTicket = pendingMy.myTicket;
  this->tx.myOldPos = pendingMy.myOldPos;
  this->tx.myOldTiltPos = pendingMy.myOldTiltPos;
  this->groupStartTicket = 0;
  this->resetGroupFlags();
}
void ShadeBehaviour::resetGroupFlags() {
  this->groupPositioning = this->groupDeferred = this->groupStopOnAir = this->groupRetargetPending = this->groupStopAfterStart = false;
}
void ShadeBehaviour::beginGroupStart(uint32_t ticket, int8_t dir, uint32_t now) {
  this->groupStartTicket = ticket;
  this->groupStartDir = dir;
  this->groupStartSince = now;
  this->groupStopAfterStart = false;
  this->groupFailed = false;
}
void ShadeBehaviour::abandonGroupStart(uint32_t at) {
  // A partial directional press may not have reached the motor at all.
  // Do not later send My to a potentially idle motor and recall a favorite.
  const int8_t dir = this->groupStartDir;
  this->groupStartTicket = 0;
  this->groupFailed = true;
  this->tx.hold.clear();
  this->tx.stopTicket = 0;
  this->resetGroupFlags();
  this->settingPos = this->settingTiltPos = this->settingMyPos = false;
  if(dir) this->runToLimit(dir, false, at);
  this->p_direction(dir);
  ESP_LOGW(TAG, "[Shade %u] Group start incomplete; physical position unknown, estimating travel to limit", this->shadeId);
  this->emitState();
}
void ShadeBehaviour::refreshTravel(uint32_t now) {
  // Requests can be drained before the next movement tick. Refresh the
  // estimate without ticking the old plan (which might send an obsolete
  // Stop). Held, pending and stopping estimates have their own clocks.
  if(this->s.tiltDirection || this->tx.hold.held() || this->groupStartTicket || this->tx.stopTicket) return;
  // An event can arrive before the first tick of a new movement has set its
  // direction; derive it as the tick would.
  if(!this->s.direction) {
    if(this->groupPositioning && this->groupStartDir) this->p_direction(this->groupStartDir);
    else if(this->s.currentPos != this->s.target) this->p_direction(this->s.currentPos > this->s.target ? -1 : 1);
    else return;
  }
  const int8_t dir = this->s.direction;
  const uint32_t travel = dir > 0 ? this->settings.downTime : this->settings.upTime;
  this->p_currentPos(liftPosition(this->startPos, dir, now, this->moveStart, travel));
  if(this->s.currentPos == limitTarget(dir) && !this->liftSettling(now)) {
    // The timed model has reached its self-stopping limit. A My here could
    // recall a favorite; begin any new movement from the idle estimate.
    this->p_target(this->s.currentPos);
    this->p_direction(0);
    this->settingPos = false;
  }
}
bool ShadeBehaviour::stopGroupTarget(uint32_t now) {
  if(this->hasPendingGroupStart()) {
    // Stopping before the start has fully gone on air could send My to a
    // motor that never started. Stop once its outcome is known instead.
    this->groupStopAfterStart = true;
    this->groupRetargetPending = false;
    return true;
  }
  if(this->startQueuedOnly()) {
    // The motor never started; the caller cancels the queued press.
    this->cancelPositioning();
    return true;
  }
  if(this->tx.stopTicket) { this->groupDeferred = false; return true; }
  this->refreshTravel(now);
  if(this->isIdle()) return true;
  const int8_t dir = this->s.direction ? this->s.direction : this->s.target > this->s.currentPos ? 1 : -1;
  if(!this->queueStop(dir, false, now)) return false;
  this->groupPositioning = true;
  this->groupDeferred = false;
  this->p_direction(dir);
  return true;
}
bool ShadeBehaviour::moveToGroupTarget(float target, uint32_t now) {
  if(!(target >= 0 && target <= 100)) return false;
  if(this->hasPendingGroupStart()) {
    // Sliders can send their final target before the start finishes. Keep
    // that press and apply only the latest target after its outcome is
    // known. Do not alter the original direction or send an early My.
    this->groupNextTarget = target;
    this->groupRetargetPending = target != this->s.target;
    this->groupStopAfterStart = false;
    return true;
  }
  if(this->groupPositioning && this->tx.stopTicket) {
    this->groupDeferred = true;
    this->groupNextTarget = target;
    return true;
  }
  if(calibratedLift() && target == this->s.target && this->liftSettling(now)) return true;
  // A start not yet on air has not moved the motor: replace it rather than
  // stop it with My. The caller cancels the queued press.
  if(this->startQueuedOnly()) this->cancelPositioning();
  this->refreshTravel(now);
  if(!this->isIdle()) {
    if(!this->s.tiltDirection && !this->tx.stopTicket &&
        ((this->s.direction > 0 && target > this->s.currentPos) ||
         (this->s.direction < 0 && target < this->s.currentPos))) {
      // The motor already moves the right way, from a shared press or its
      // own. Only its stop deadline changes; independent stop/restart
      // sequences would lose synchronization and consume rolling codes.
      this->p_target(target);
      this->settingPos = true;
      this->groupPositioning = true;
      this->groupStartDir = this->s.direction;
      this->emitState();
      return true;
    }
    // Stop before reversing or returning to a target already passed. Sending
    // a fresh directional button while moving can have motor-specific stop semantics.
    int8_t dir = this->s.direction;
    if(!dir) dir = this->s.target > this->s.currentPos ? 1 : -1;
    if(!this->tx.stopTicket && !this->queueStop(dir, false, now)) return false;
    this->groupPositioning = true;
    this->groupDeferred = true;
    this->groupNextTarget = target;
    this->p_direction(dir);
    return true;
  }
  bool sent;
  if(calibratedLift()) return this->startLiftTarget(target, this->remote.repeats, now);
  if(target == 0 || target == 100)
    sent = this->sendCommand(target == 0 ? somfy_commands::Up : somfy_commands::Down, this->remote.repeats, 0, now);
  else sent = this->moveToTarget(target, -1, now);
  if(sent) {
    this->groupPositioning = true;
    this->beginGroupStart(this->tx.hold.ticket, target > this->s.currentPos ? 1 : target < this->s.currentPos ? -1 : 0, now);
  }
  return sent;
}
void ShadeBehaviour::observeGroupStart(uint32_t ticket, uint32_t now) {
  // Ordinary directional group commands need the same override barrier:
  // a priority individual My must not overtake their queued shared start.
  this->beginGroupStart(ticket, this->s.target > this->s.currentPos ? 1 : this->s.target < this->s.currentPos ? -1 : 0, now);
}
void ShadeBehaviour::adoptGroupTarget(float target, uint32_t ticket, uint32_t now) {
  invalidateFavouriteObservation();
  this->cancelPositioning();
  this->liftDirection = 0;
  this->startPos = this->s.currentPos;
  this->moveStart = now;
  this->p_target(target);
  this->settingPos = true;
  this->groupPositioning = true;
  // Force a hold even when replacing an already moving estimate. Native
  // starts are only planned for idle members; endpoint refreshes may be idle
  // at their target and therefore have no simulated travel to hold.
  this->tx.hold.mark(ticket, false, now);
  this->beginGroupStart(ticket, target > this->s.currentPos ? 1 : target < this->s.currentPos ? -1 : 0, now);
  this->p_direction(target > this->s.currentPos ? 1 : target < this->s.currentPos ? -1 : 0);
  this->emitState();
}

void ShadeBehaviour::cancelGroupStop(float target, uint32_t now) {
  const int8_t dir = tx.stopDir;
  const uint32_t travel = dir > 0 ? settings.downTime : settings.upTime;
  p_currentPos(liftPosition(groupStopPos, dir, now, tx.stopAt, travel));
  startPos = s.currentPos;
  moveStart = now;
  tx.stopTicket = 0;
  groupStopOnAir = groupDeferred = false;
  groupPositioning = settingPos = true;
  groupStartDir = dir;
  p_direction(dir);
  p_target(target);
  emitState();
}
