#pragma once
#include "LiftCalibration.h"
struct LiftCalibrationRequest {
  static constexpr uint8_t begin = 100, poll = 101;
  uint32_t token = 0;
  uint8_t id = 0, action = poll;
  LiftKind kind = LiftKind::legacy;
  bool human = false;
};
