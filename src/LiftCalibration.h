#pragma once
#include <stdint.h>

enum class LiftKind : uint8_t { legacy = 0, direct = 1, segmented = 2 };
enum class LiftStart : uint8_t { endpoint, interior, reversal };
struct LiftDirection {
  uint32_t takeupMs = 0;
  uint32_t midpointMs = 0; // Visible travel to halfway; zero means linear.
  uint32_t settlingMs = 0;
  uint32_t reversalMs = 0;
};
// Existing up/down times remain full-stroke motor times, including take-up
// and settling. Midpoint is measured within visible travel only.
struct LiftCalibration {
  LiftKind kind = LiftKind::legacy;
  LiftDirection up, down;
};
constexpr uint32_t liftCalibrationMaxMs = 600000;
bool validLiftCalibration(const LiftCalibration &, uint32_t upTime, uint32_t downTime);
bool sameLiftCalibration(const LiftCalibration &, const LiftCalibration &);

// One uninterrupted motor run. Retargeting changes the requested deadline,
// not this run's origin, clock or remaining mechanical take-up.
class LiftRun {
  LiftDirection profile;
  uint32_t visibleMs = 0, leadingMs = 0;
  float initial = 0, originMs = 0;
  int8_t direction = 0;
  bool usable = false, legacy = true;
  float curveTime(float progress) const;
  float curvePosition(float milliseconds) const;
public:
  LiftRun() = default;
  LiftRun(const LiftCalibration &, uint32_t totalMs, float startPos,
          int8_t direction, LiftStart);
  bool valid() const { return usable; }
  float position(uint32_t elapsedMs) const;
  uint32_t msTo(float target) const;
  uint32_t fullCompletionMs() const;
  bool mechanicalPhase(uint32_t elapsed) const {
    return usable && !legacy && (elapsed < leadingMs ||
      (elapsed < fullCompletionMs() && position(elapsed) == (direction > 0 ? 100 : 0)));
  }
};
