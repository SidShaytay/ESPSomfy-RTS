#include "BatteryGauge.h"
namespace BatteryGauge {
void Monitor::tick(uint32_t now, ReadWord read) {
  if(!sampling) {
    if(started && uint32_t(now - lastPoll) < 10000) return;
    started = sampling = true;
    lastPoll = now;
    index = 0;
  } else if(uint32_t(now - lastWord) < 10) return;
  lastWord = now;
  // TI SLUUBD4A: OperationStatus, BatteryStatus, SOC, Voltage, Current.
  constexpr uint8_t registers[] = {0x3a, 0x0a, 0x2c, 0x08, 0x0c};
  if(!read || !read(registers[index], words[index])) {
    value = {State::readFailed};
    sampling = false;
    return;
  }
  if(++index != 5) return;
  sampling = false;
  if(!(words[1] & 0x08)) value = {State::noBattery}; // BATTPRES
  else if(!(words[0] & 0x20) || (words[0] & 0x400)) value = {State::initializing}; // INITCOMP / CFGUPDATE
  else if(words[2] > 100 || words[3] == 0 || words[3] > 6000) value = {State::invalid};
  else value = {State::ready, uint8_t(words[2]), words[3],
                int16_t(words[4] >= 0x8000 ? int32_t(words[4]) - 65536 : words[4])};
}
}
