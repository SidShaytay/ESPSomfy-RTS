#include "BatteryCharger.h"
namespace BatteryCharger {
namespace {
// Sets only the masked bits, then reads them back.
bool setBits(ReadByte read, WriteByte write, uint8_t reg, uint8_t mask, uint8_t bits) {
  uint8_t current = 0;
  if(!read(reg, current)) return false;
  if((current & mask) == bits) return true;
  if(!write(reg, uint8_t((current & ~mask) | bits))) return false;
  return read(reg, current) && (current & mask) == bits;
}
}
void Limiter::tick(uint32_t now, ReadByte read, WriteByte write) {
  // Standard: checked once at start and after a change, then left alone.
  if(!due && ((wanted == standard && value == State::idle) || uint32_t(now - last) < recheckMs)) return;
  due = false;
  last = now;
  if(!read || !write) { value = wanted == standard ? State::idle : State::failed; return; }
  uint8_t timer = 0;
  if(wanted == standard) {
    // Only undo a limit: watchdog disabled means this firmware set one earlier.
    if(!read(regTimer, timer)) { value = State::failed; return; }
    if((timer & watchdogMask) != 0) { value = State::idle; return; }
    const bool ok = setBits(read, write, regVoltage, vregMask, uint8_t(vregCode[standard] << 2)) &&
                    setBits(read, write, regTimer, watchdogMask, watchdogDefault);
    value = ok ? State::idle : State::failed;
    return;
  }
  // Watchdog off first, so the new VREG cannot be reset 40 s later.
  const bool ok = setBits(read, write, regTimer, watchdogMask, 0) &&
                  setBits(read, write, regVoltage, vregMask, uint8_t(vregCode[wanted] << 2));
  value = ok ? State::applied : State::failed;
}
}
