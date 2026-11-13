#include "LiftCalibration.h"
#include "ShadeMotion.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace {
bool empty(const LiftDirection &d) {
  return !d.takeupMs && !d.midpointMs && !d.settlingMs && !d.reversalMs;
}
bool validProfile(const LiftDirection &d, uint32_t total) {
  if(!total || total > liftCalibrationMaxMs || d.reversalMs > liftCalibrationMaxMs) return false;
  const uint64_t flat = uint64_t(d.takeupMs) + d.settlingMs;
  return flat < total && (!d.midpointMs || d.midpointMs < total - flat);
}
bool same(const LiftDirection &a, const LiftDirection &b) {
  return a.takeupMs == b.takeupMs && a.midpointMs == b.midpointMs &&
    a.settlingMs == b.settlingMs && a.reversalMs == b.reversalMs;
}
uint32_t roundedDeadline(float value) {
  if(value <= 0) return 0;
  if(value >= float(UINT32_MAX)) return UINT32_MAX;
  return uint32_t(ceilf(value));
}
}
bool validLiftCalibration(const LiftCalibration &c, uint32_t up, uint32_t down) {
  if(c.kind == LiftKind::legacy) return empty(c.up) && empty(c.down);
  if(c.kind == LiftKind::direct && (c.up.takeupMs || c.down.takeupMs || c.up.settlingMs || c.down.settlingMs || c.up.reversalMs || c.down.reversalMs)) return false;
  return (c.kind == LiftKind::direct || c.kind == LiftKind::segmented) &&
    validProfile(c.up, up) && validProfile(c.down, down);
}
bool sameLiftCalibration(const LiftCalibration &a, const LiftCalibration &b) {
  return a.kind == b.kind && same(a.up,b.up) && same(a.down,b.down);
}
LiftRun::LiftRun(const LiftCalibration &c, uint32_t total, float start,
                 int8_t dir, LiftStart kind) : initial(start), direction(dir) {
  if(!std::isfinite(start) || start < 0 || start > 100 || (dir != -1 && dir != 1)) return;
  legacy = c.kind == LiftKind::legacy;
  profile = dir > 0 ? c.down : c.up;
  if(legacy && (!empty(c.up) || !empty(c.down))) return;
  if(c.kind == LiftKind::direct && (profile.takeupMs || profile.settlingMs || profile.reversalMs)) return;
  if(!legacy && ((c.kind != LiftKind::direct && c.kind != LiftKind::segmented) || !validProfile(profile,total))) return;
  if(legacy) profile = {};
  visibleMs = total - profile.takeupMs - profile.settlingMs;
  const float progress = dir > 0 ? start : 100 - start;
  if(kind == LiftStart::endpoint && progress == 0) leadingMs = profile.takeupMs;
  else if(kind == LiftStart::reversal && progress > 0 && progress < 100) leadingMs = profile.reversalMs;
  // Match travelPosition's division-first, millisecond-quantized legacy origin.
  originMs = legacy ? (dir > 0 ? floorf((start / 100) * total) : total - floorf((start / 100) * total)) : curveTime(progress);
  usable = true;
}
float LiftRun::curveTime(float progress) const {
  const float middle = profile.midpointMs ? float(profile.midpointMs) : float(visibleMs) / 2;
  return progress <= 50 ? middle * progress / 50 : middle + (visibleMs - middle) * (progress - 50) / 50;
}
float LiftRun::curvePosition(float ms) const {
  if(!visibleMs) return 100;
  const float middle = profile.midpointMs ? float(profile.midpointMs) : float(visibleMs) / 2;
  return ms <= middle ? 50 * ms / middle : 50 + 50 * (ms - middle) / (visibleMs - middle);
}
float LiftRun::position(uint32_t elapsed) const {
  if(!usable) return initial;
  if(legacy) return travelPosition(initial,direction,elapsed,static_cast<int32_t>(visibleMs));
  if(elapsed < leadingMs) return initial;
  const float ms = std::min(float(visibleMs), originMs + float(elapsed - leadingMs));
  const float progress = curvePosition(ms);
  return std::max(0.0f,std::min(100.0f,direction > 0 ? progress : 100 - progress));
}
uint32_t LiftRun::msTo(float target) const {
  if(!usable || !std::isfinite(target) || target < 0 || target > 100) return 0;
  const float progress = direction > 0 ? target : 100 - target;
  const float from = direction > 0 ? initial : 100 - initial;
  if(progress <= from) return 0;
  return roundedDeadline(float(leadingMs) + curveTime(progress) - originMs +
    (progress == 100 ? profile.settlingMs : 0));
}
uint32_t LiftRun::fullCompletionMs() const {
  return !usable ? 0 : roundedDeadline(float(leadingMs) + visibleMs - originMs + profile.settlingMs);
}
