#pragma once
#include <stdint.h>
// Optional charge-voltage limit for the BQ25896 charger (TI SLUSC76C) on boards
// that are plugged in all the time. Off by default: the charger is then left in
// its own default mode and nothing is written to it.
//
// A limit puts the charger in host mode: REG07 WATCHDOG is set to 00 (disabled)
// so the watchdog cannot expire and reset VREG to 4.208 V, then REG06 VREG[7:2]
// is set (3.840 V + 16 mV/step). Only those bits change; the rest of each
// register is read back and kept. The registers are re-checked every 30 s,
// because a charger power loss returns them to their defaults.
// Turning the limit off writes VREG and WATCHDOG back to their defaults, so the
// watchdog expires after 40 s and the charger returns to default mode.
namespace BatteryCharger {
constexpr uint8_t address = 0x6b;
constexpr uint8_t regVoltage = 0x06, regTimer = 0x07;
constexpr uint8_t vregMask = 0xfc, watchdogMask = 0x30, watchdogDefault = 0x10;
// Charge limits as the web UI offers them, by approximate charge level. The
// voltages follow Battery University BU-808, table 4 (4.20 V 100%, 4.13 V 90%,
// 4.06 V 81%, 4.00 V 73%, 3.92 V 65%, 3.85 V 60%), rounded to the charger's
// 16 mV steps. 3.840 V is the lowest the BQ25896 allows, so 60% is the floor.
enum Limit : int8_t { full = 0, percent90 = 1, percent80 = 2, percent70 = 3, percent60 = 4, limitCount = 5 };
constexpr int8_t standard = full;
// VREG[5:0] codes: 4.208 V (charger default), 4.128 V, 4.064 V, 3.968 V, 3.840 V.
constexpr uint8_t vregCode[limitCount] = {0x17, 0x12, 0x0e, 0x08, 0x00};
constexpr uint16_t millivolts(int8_t limit) {
  return limit >= 0 && limit < limitCount ? uint16_t(3840 + 16 * vregCode[limit]) : 0;
}
constexpr uint32_t recheckMs = 30000;
using ReadByte = bool (*)(uint8_t reg, uint8_t &value);
using WriteByte = bool (*)(uint8_t reg, uint8_t value);
enum class State : uint8_t { idle, applied, failed };
class Limiter {
  int8_t wanted = standard;
  bool due = true;
  State value = State::idle;
  uint32_t last = 0;
public:
  void want(int8_t limit) {
    if(limit < 0 || limit >= limitCount) return;
    if(limit != wanted) { wanted = limit; due = true; }
  }
  int8_t limit() const { return wanted; }
  State state() const { return value; }
  // At most a few register transfers, only when due. read/write may be null
  // when the bus is unavailable.
  void tick(uint32_t now, ReadByte read, WriteByte write);
};
// Register-pointer write + repeated-start read of one byte.
template<class Wire> bool readByte(Wire &wire, uint8_t reg, uint8_t &value) {
  wire.beginTransmission(address);
  if(wire.write(reg) != 1) { wire.endTransmission(true); return false; }
  if(wire.endTransmission(false) != 0) return false;
  if(wire.requestFrom(address, uint8_t(1), true) != 1 || wire.available() != 1) return false;
  const int v = wire.read();
  if(v < 0) return false;
  value = uint8_t(v);
  return true;
}
template<class Wire> bool writeByte(Wire &wire, uint8_t reg, uint8_t value) {
  wire.beginTransmission(address);
  if(wire.write(reg) != 1 || wire.write(value) != 1) { wire.endTransmission(true); return false; }
  return wire.endTransmission(true) == 0;
}
}
