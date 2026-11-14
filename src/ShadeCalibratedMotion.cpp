#include "ShadeBehaviour.h"

bool ShadeBehaviour::calibratedLift() const {
  return settings.calibration.kind != LiftKind::legacy && settings.tiltType == tilt_types::none;
}
float ShadeBehaviour::liftPosition(float origin, int8_t direction, uint32_t now,
                                    uint32_t since, int32_t total) {
  if(!calibratedLift()) return travelPosition(origin, direction, now - since, total);
  if(!liftDirection || liftDirection != direction) {
    const LiftStart start = origin == 0 || origin == 100 ? LiftStart::endpoint :
      lastLiftDirection && lastLiftDirection != direction ? LiftStart::reversal : LiftStart::interior;
    liftRun = LiftRun(settings.calibration, uint32_t(total), origin, direction, start);
    liftAt = since;
    liftDirection = lastLiftDirection = direction;
  }
  return liftRun.position(now - liftAt);
}
bool ShadeBehaviour::liftReached(float target, uint32_t now) const {
  if(calibratedLift() && liftDirection)
    return now - liftAt >= liftRun.msTo(target);
  return s.direction > 0 ? s.currentPos >= target : s.currentPos <= target;
}
bool ShadeBehaviour::liftSettling(uint32_t now) const {
  return calibratedLift() && liftDirection && s.target == limitTarget(liftDirection) &&
    now - liftAt < liftRun.fullCompletionMs();
}

bool ShadeBehaviour::startLiftTarget(float target, uint8_t repeat, uint32_t now, uint32_t *ticket) {
  if(ticket) *ticket = 0;
  if(!(target >= 0 && target <= 100)) return false;
  if(target == s.currentPos && target > 0 && target < 100) return true;
  const int8_t direction = target == 0 ? -1 : target == 100 ? 1 : target > s.currentPos ? 1 : -1;
  const auto receipt = transmit(direction > 0 ? somfy_commands::Down : somfy_commands::Up,
      repeat, 0, 0, false, now);
  if(!receipt.sent) return false;
  if(ticket) *ticket = receipt.ticket;
  p_target(target);
  settingPos = target != s.currentPos;
  groupPositioning = true;
  beginGroupStart(receipt.ticket, target == s.currentPos ? 0 : direction, now);
  return true;
}
