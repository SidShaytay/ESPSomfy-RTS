#pragma once
#include <stdint.h>
#include "BatteryGauge.h"
#include "BatteryCharger.h"
// Loop-only view of the board's battery for the web UI, and its charge limit.
namespace BatteryStatus {
struct View {
  BatteryGauge::Reading reading;
  bool limitSupported = false;
  int8_t limit = BatteryCharger::standard;
  BatteryCharger::State limitState = BatteryCharger::State::idle;
};
View view();
// Saves the limit (its own NVS key, not part of the settings backup) and
// applies it on the next power poll. False when unsupported or not saved.
bool setLimit(int8_t limit);
}
