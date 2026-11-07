#include "LocalUiRuntime.h"
#include "LocalUiAdapter.h"
#include "LocalUiCapture.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
bool hasPsram = true, allocateOK = true;
void *allocated = nullptr;
int testPins[64] = {};
uint32_t testNow = 0;
static unsigned bands = 0, dispatches = 0;
static bool busBusy = false;
LocalUi::Model LocalUi::snapshotModel(bool running) {
  Model m;
  m.ready = running;
  for(auto &t : m.targets) {
    t.valid = true;
    t.identity = 123;
    t.open[0] = 45;
    strcpy(t.name, "Bedroom");
  }
  return m; // controls remain locked in the actual preview runtime
}
void LocalUi::capturePoll(const uint16_t *, bool, uint8_t, uint32_t) {}
LocalUi::DisplayPreferences LocalUi::loadPreferences() { return {}; }
bool LocalUi::savePreferences(const DisplayPreferences &) { return true; }
bool LocalUi::dispatch(const Command &) {
  ++dispatches;
  return false;
}
static bool send(uint8_t y, uint8_t rows, const uint8_t *data, size_t bytes) {
  assert(data && y < 170 && y + rows <= 170 && rows > 0 && rows <= 4 && bytes == rows * 640u);
  if(busBusy)
    return false;
  ++bands;
  return true;
}
static void light(uint8_t value) { assert(value == 0 || value == 4 || value == 32); }
int main() {
  using namespace LocalUi;
  StatusDisplay::Snapshot s;
  s.running = true;
  s.link = StatusDisplay::Link::connected;
  s.web = StatusDisplay::Web::listening;
  s.radio = StatusDisplay::Radio::ready;
  testPins[4] = testPins[5] = testPins[6] = 1;
  Runtime ui;
  ui.begin();
  assert(ui.failure() == GraphicsFailure::none && allocated);
  busBusy = true;
  for(testNow = 2; testNow < 20; testNow += 2)
    assert(ui.tick(testNow, s, false, send, light));
  assert(bands == 0);
  busBusy = false;
  for(; testNow < 200; testNow += 2)
    assert(ui.tick(testNow, s, false, send, light));
  assert(bands == 43); // one complete 170-row frame, last band is two rows
  for(; testNow < 400; testNow += 2)
    assert(ui.tick(testNow, s, false, send, light));
  assert(bands == 43); // unchanged frames do not transmit
  // Navigate through the actual quadrature/runtime/controller/renderer pipeline.
  auto tick = [&](int ab, bool pressed) {
    testPins[4] = ab >> 1;
    testPins[5] = ab & 1;
    testNow += 50;
    ui.tick(testNow, s, pressed, send, light);
  };
  int ab = 3;
  auto idle = [&] {
    for(int i = 0; i < 60; i++)
      tick(ab, false);
  };
  auto rotate = [&](int direction) {
    const int middle = direction > 0 ? (ab == 3 ? 1 : 2) : (ab == 3 ? 2 : 1);
    ab = ab == 3 ? 0 : 3;
    tick(middle, false);
    tick(ab, false);
  };
  auto click = [&] {
    tick(ab, true);
    tick(ab, true);
    tick(ab, false);
    tick(ab, false);
  };
  rotate(1);
  idle();
  click();
  idle();
  rotate(-1);
  idle();
  click();
  assert(dispatches == 0);
  free(allocated);
  allocated = nullptr;
  hasPsram = false;
  Runtime missing;
  missing.begin();
  assert(missing.failure() == GraphicsFailure::missing);
  assert(!missing.tick(testNow, s, false, send, light));
  hasPsram = true;
  allocateOK = false;
  Runtime failed;
  failed.begin();
  assert(failed.failure() == GraphicsFailure::allocation);
  assert(!failed.tick(testNow, s, false, send, light));
  puts("UI runtime: PSRAM-only allocation, bounded/busy transfers, unchanged-frame skip, locked preview and "
       "fallback passed.");
}
