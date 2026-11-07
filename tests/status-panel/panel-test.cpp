#include "Arduino.h"
#include "SPI.h"
#include <algorithm>
#include <cstring>
uint32_t fakeNow = 0;
int pins[64] = {};
bool spiOK = true, attachOK = true, allocationOK = true, owned = false, busy = false;
SerialStub Serial;
SPIStub SPI;
bool tryDisplayBus(void (*fn)(void *), void *ctx) {
  if(busy) return false;
  owned = true; fn(ctx); owned = false; return true;
}
#include "../../src/StatusPanel.cpp"
void StatusPanel::pollPower(bool) {}
StatusDisplay::Snapshot StatusPanel::snapshot(bool running) {
  StatusDisplay::Snapshot s; s.running = running; strcpy(s.build, "Plus LCD test"); return s;
}
int main() {
  using namespace StatusDisplay;
  pins[12] = pins[41] = pins[0] = HIGH;
  StatusPanel::Panel panel;
  busy = true;
  assert(panel.initialize(0) == Result::pending);
  assert(SPI.transactions == 0);
  busy = false;
  Result result = Result::pending;
  for(fakeNow = 1; fakeNow < 2000 && result == Result::pending; ++fakeNow) result = panel.initialize(fakeNow);
  assert(result == Result::ready);
  assert(std::find(SPI.commands.begin(), SPI.commands.end(), 0x11) != SPI.commands.end());
  assert(std::find(SPI.commands.begin(), SPI.commands.end(), 0x29) != SPI.commands.end());
  assert(pins[21] == 0 && !SPI.active && !owned);
  unsigned before = SPI.bytes;
  assert(panel.row(0, "BOOT"));
  assert(SPI.bytes - before == 11531); // 18 scanlines plus address/command bytes
  uint8_t stripe[2560] = {};
  before = SPI.bytes; assert(panel.band(166,4,stripe,sizeof(stripe)));
  assert(SPI.bytes-before==2571);
  assert(!panel.band(168,4,stripe,sizeof(stripe)));
  assert(!panel.band(0,4,stripe,1));
  before = SPI.transactions; busy = true;
  assert(!panel.row(1, "offline")); assert(SPI.transactions == before);
  busy = false; assert(!panel.row(9, "out of bounds"));
  panel.brightness(32); assert(pins[21] == 32);
  panel.brightness(0); assert(pins[21] == 0);
  attachOK = false;
  StatusPanel::Panel failed;
  assert(failed.initialize(0) == Result::failed);
  attachOK = true; allocationOK = false;
  StatusPanel::Panel noMemory;
  assert(noMemory.initialize(0) == Result::failed);
  allocationOK = true; spiOK = false;
  StatusPanel::Panel brokenBus;
  assert(brokenBus.initialize(0) == Result::failed);
  brokenBus.brightness(32); assert(pins[21] == 0);
  assert(!brokenBus.row(0, "failed"));
  spiOK = true; busy = true;
  StatusPanel::Panel timeout;
  assert(timeout.initialize(0) == Result::pending);
  assert(timeout.initialize(5001) == Result::failed);
  puts("Status panel: ownership, selects, bounds, busy bus, initialization and failures passed");
}
