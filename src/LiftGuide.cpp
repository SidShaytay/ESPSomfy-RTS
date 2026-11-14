#include "LiftGuide.h"

bool LiftGuide::begin(LiftKind kind, uint32_t) {
  if(phase != LiftGuideStage::closed || (kind != LiftKind::direct && kind != LiftKind::segmented)) return false;
  *this = LiftGuide{};
  profile.kind = kind;
  phase = LiftGuideStage::reference;
  return true;
}
bool LiftGuide::queued(uint32_t value) {
  if(!value || ticket || !awaitingQueue) return false;
  ticket = value;
  awaitingQueue = false;
  return true;
}
void LiftGuide::refused() {
  if(phase == LiftGuideStage::closed || phase == LiftGuideStage::complete) return;
  phase = LiftGuideStage::failed;
  ticket = 0;
  awaitingQueue = false;
}
void LiftGuide::txEvent(tx_event_t type, uint32_t value, uint32_t at) {
  if(!ticket || value != ticket || phase == LiftGuideStage::failed) return;
  if(type == tx_event_t::dropped || type == tx_event_t::aborted) { refused(); return; }
  if(type == tx_event_t::started) {
    if(started) return;
    started = true;
    start = at;
    movingAt = halfwayAt = endAt = 0;
    if(phase == LiftGuideStage::startingDown)
      phase = profile.kind == LiftKind::segmented ? LiftGuideStage::downTakeup : LiftGuideStage::downHalf;
    else if(phase == LiftGuideStage::startingUp)
      phase = profile.kind == LiftKind::segmented ? LiftGuideStage::upTakeup : LiftGuideStage::upHalf;
  } else if(type == tx_event_t::finished) {
    if(!started) { refused(); return; }
    ticket = 0;
  }
}
bool LiftGuide::endpoint(uint32_t now) {
  const uint32_t total = now - start;
  if(ticket || !started || !total || total > liftCalibrationMaxMs || total <= halfwayAt ||
      (profile.kind == LiftKind::segmented && total < endAt)) return false;
  LiftDirection direction;
  direction.takeupMs = movingAt;
  direction.midpointMs = halfwayAt - movingAt;
  direction.settlingMs = profile.kind == LiftKind::segmented ? total - endAt : 0;
  LiftCalibration candidate = profile;
  (directionDown ? candidate.down : candidate.up) = direction;
  if(!LiftRun(candidate,total,directionDown ? 0 : 100,directionDown ? 1 : -1,LiftStart::endpoint).valid()) return false;
  profile = candidate;
  (directionDown ? down : up) = total;
  started = false;
  return true;
}
LiftGuideResult LiftGuide::act(LiftGuideAction action, uint32_t now, bool human) {
  using A = LiftGuideAction;
  using S = LiftGuideStage;
  using E = LiftGuideEffect;
  if(phase == S::closed) return {};
  if(action == A::cancel) { *this = LiftGuide{}; return {true,E::close}; }
  if(phase == S::complete || phase == S::failed || phase == S::saving) return {};
  if((action == A::confirmOpen || action == A::moving || action == A::halfway ||
      action == A::visibleEnd || action == A::confirmEndpoint) && !human) return {};
  if(action == A::stop) {
    const E effect = started ? E::stop : ticket || awaitingQueue ? E::cancelStart : E::none;
    refused();
    return {true,effect};
  }
  if(action == A::open && phase == S::reference && !ticket && !awaitingQueue) {
    started = false; awaitingQueue = true;
    return {true,E::driveUp};
  }
  if(action == A::confirmOpen && phase == S::reference && !ticket && !awaitingQueue) {
    phase = S::readyDown; started = false;
    return {true,E::anchorOpen};
  }
  if((action == A::startDown && phase == S::readyDown) || (action == A::startUp && phase == S::readyUp)) {
    directionDown = action == A::startDown;
    phase = directionDown ? S::startingDown : S::startingUp;
    started = false; awaitingQueue = true;
    return {true,directionDown ? E::driveDown : E::driveUp};
  }
  const uint32_t elapsed = now - start;
  if(action == A::moving && (phase == S::downTakeup || phase == S::upTakeup) && elapsed <= liftCalibrationMaxMs) {
    movingAt = elapsed; phase = directionDown ? S::downHalf : S::upHalf;
    return {true,E::none};
  }
  if(action == A::halfway && (phase == S::downHalf || phase == S::upHalf) &&
      elapsed > movingAt && elapsed <= liftCalibrationMaxMs) {
    halfwayAt = elapsed;
    phase = directionDown ? (profile.kind == LiftKind::segmented ? S::downEnd : S::downConfirm) :
                           (profile.kind == LiftKind::segmented ? S::upEnd : S::upConfirm);
    return {true,E::none};
  }
  if(action == A::visibleEnd && (phase == S::downEnd || phase == S::upEnd) &&
      elapsed > halfwayAt && elapsed <= liftCalibrationMaxMs) {
    endAt = elapsed; phase = directionDown ? S::downConfirm : S::upConfirm;
    return {true,E::none};
  }
  if(action == A::confirmEndpoint && (phase == S::downConfirm || phase == S::upConfirm) && endpoint(now)) {
    phase = directionDown ? S::readyUp : S::review;
    return {true,directionDown ? E::anchorClosed : E::anchorOpen};
  }
  if(action == A::save && phase == S::review && validLiftCalibration(profile,up,down)) {
    phase = S::saving;
    return {true,E::save};
  }
  return {};
}
void LiftGuide::saved(bool applied, bool durable) {
  if(phase == LiftGuideStage::saving)
    phase = applied && durable ? LiftGuideStage::complete : LiftGuideStage::failed;
}
