#pragma once
#include <ArduinoJson.h>
#include "ShadeSettings.h"
// Project the complete profile before applying any part of a settings edit.
inline bool projectLiftCalibration(JsonObjectConst obj, const ShadeSettings &current,
                                   LiftCalibration &out) {
  out = current.calibration;
  uint32_t up = current.upTime, down = current.downTime;
  struct Field { const char *key; uint32_t *value; };
  const Field fields[] = {
    {"upTime", &up}, {"downTime", &down},
    {"upTakeupMs", &out.up.takeupMs},
    {"upMidpointMs", &out.up.midpointMs},
    {"upSettlingMs", &out.up.settlingMs},
    {"upReversalMs", &out.up.reversalMs},
    {"downTakeupMs", &out.down.takeupMs},
    {"downMidpointMs", &out.down.midpointMs},
    {"downSettlingMs", &out.down.settlingMs},
    {"downReversalMs", &out.down.reversalMs},
  };
  const auto kind = obj["calibrationKind"];
  if(!kind.isNull()) {
    if(!kind.is<uint8_t>() || kind.as<uint8_t>() > 2) return false;
    out.kind = static_cast<LiftKind>(kind.as<uint8_t>());
  }
  for(const auto &field : fields) {
    const auto value = obj[field.key];
    if(value.isNull()) continue;
    if(!value.is<uint32_t>()) return false;
    *field.value = value.as<uint32_t>();
  }
  return validLiftCalibration(out, up, down);
}
