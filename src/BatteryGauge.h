#pragma once
#include <stdint.h>
namespace BatteryGauge {
enum class State : uint8_t { notMonitored, initializing, ready, noBattery, readFailed, invalid };
struct Reading {
  State state = State::notMonitored;
  uint8_t percent = 0;
  uint16_t millivolts = 0;
  int16_t milliamps = 0;
};
using ReadWord = bool (*)(uint8_t reg, uint16_t &value);
// BQ27220 read-only sampling. One word per tick, no charger/configuration commands.
class Monitor {
  Reading value{State::initializing};
  uint16_t words[5] = {};
  uint32_t lastPoll = 0, lastWord = 0;
  uint8_t index = 0;
  bool started = false, sampling = false;
public:
  void tick(uint32_t now, ReadWord read);
  const Reading &reading() const { return value; }
};
// Register-pointer write + repeated-start read. No register data is written.
// Kept independent of Arduino so short reads/byte order can be tested on the host.
template<class Wire> bool readWord(Wire &wire, uint8_t reg, uint16_t &value) {
  wire.beginTransmission(0x55);
  if(wire.write(reg) != 1) {
    wire.endTransmission(true);
    return false;
  }
  if(wire.endTransmission(false) != 0) return false;
  if(wire.requestFrom(uint8_t(0x55), uint8_t(2), true) != 2 || wire.available() != 2)
    return false;
  const int low = wire.read(), high = wire.read();
  if(low < 0 || high < 0) return false;
  value = uint16_t(low) | (uint16_t(high) << 8);
  return true;
}
}
