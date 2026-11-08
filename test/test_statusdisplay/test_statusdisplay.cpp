#include <unity.h>
#include "StatusDisplay.h"
#include <cstring>
using namespace StatusDisplay;
struct Panel : Output {
  bool fail = false, busy = false, pending = false;
  unsigned rows = 0, opens = 0, attempts = 0;
  uint8_t light = 255;
  Result initialize(uint32_t) override { ++opens; return fail ? Result::failed : pending ? Result::pending : Result::ready; }
  bool row(uint8_t, const char *) override { ++attempts; if(busy) return false; ++rows; return true; }
  void brightness(uint8_t value) override { light = value; }
};
void setUp() {}
void tearDown() {}
static void states() {
  Snapshot s;
  Page p;
  format(s, p);
  TEST_ASSERT_NOT_NULL(strstr(p.lines[0], "BOOT"));
  TEST_ASSERT_EQUAL_STRING("Power: not monitored", p.lines[1]);
  TEST_ASSERT_NOT_NULL(strstr(p.lines[2], "disconnected"));
  TEST_ASSERT_NOT_NULL(strstr(p.lines[4], "starting"));
  s.running = true; s.link = Link::connected; strcpy(s.address, "192.0.2.1");
  s.web = Web::listening; s.radio = Radio::ready;
  format(s, p);
  TEST_ASSERT_NOT_NULL(strstr(p.lines[3], "192.0.2.1"));
  TEST_ASSERT_NOT_NULL(strstr(p.lines[5], "http://192.0.2.1"));
  TEST_ASSERT_NOT_NULL(strstr(p.lines[6], "ready"));
  s.link = Link::connecting; s.web = Web::error; s.radio = Radio::unavailable;
  format(s, p);
  TEST_ASSERT_NOT_NULL(strstr(p.lines[2], "connecting"));
  TEST_ASSERT_NOT_NULL(strstr(p.lines[4], "error"));
  TEST_ASSERT_EQUAL_STRING("UI: unavailable", p.lines[5]);
  TEST_ASSERT_NOT_NULL(strstr(p.lines[6], "unavailable"));
  s.link = Link::accessPoint; s.web = Web::listening;
  format(s, p);
  TEST_ASSERT_NOT_NULL(strstr(p.lines[2], "access point"));
}
static void lifecycle() {
  Panel p; Session display(p); Snapshot s;
  display.tick(0, s, false);
  for(unsigned t = 20; t < 1000; t += 20) display.tick(t, s, false);
  TEST_ASSERT_EQUAL(9, p.rows);
  display.tick(30000, s, false); TEST_ASSERT_EQUAL(4, p.light);
  display.tick(60000, s, false); TEST_ASSERT_EQUAL(0, p.light);
  display.tick(60001, s, true); display.tick(60050, s, true);
  TEST_ASSERT_EQUAL(32, p.light);
  display.tick(120100, s, true); TEST_ASSERT_EQUAL(0, p.light); // held input cannot keep waking
}
static void failure() {
  Panel p; p.fail = true; Session display(p); Snapshot s;
  for(unsigned t = 0; t < 100000; t += 20) display.tick(t, s, t % 100 == 0);
  TEST_ASSERT_EQUAL(1, p.opens); TEST_ASSERT_EQUAL(0, p.rows); TEST_ASSERT_EQUAL(0, p.light);
}
static void busy_and_wrap() {
  Panel p; p.pending = true; Session display(p); Snapshot s;
  const uint32_t start = UINT32_MAX - 10000;
  display.tick(start, s, false); TEST_ASSERT_EQUAL(0, p.rows);
  p.pending = false; p.busy = true;
  display.tick(start + 1, s, false); TEST_ASSERT_EQUAL(0, p.rows);
  display.tick(start + 2, s, false); TEST_ASSERT_EQUAL(1, p.attempts);
  p.busy = false;
  display.tick(start + 21, s, false); TEST_ASSERT_EQUAL(1, p.rows);
  display.tick(start + 22, s, false); TEST_ASSERT_EQUAL(1, p.rows);
  display.tick(start + 60001, s, false); TEST_ASSERT_EQUAL(0, p.light);
  display.tick(start, s, false); TEST_ASSERT_EQUAL(0, p.light); // remains off over millis wrap
}
static void disabled_fallback_timeouts_keep_the_status_awake() {
  Panel p;Session session(p,20000);Snapshot s;session.setTimeouts(0,0);
  session.tick(0,s,false);session.tick(600000,s,false);TEST_ASSERT_EQUAL(32,p.light);
}
static void battery_readings_and_failures_are_explicit() {
  Snapshot s; Page p;
  s.battery = {BatteryGauge::State::ready, 74, 3920, -100};
  format(s, p);
  TEST_ASSERT_EQUAL_STRING("Battery: 74% (3.92V)", p.lines[1]);
  s.battery.state = BatteryGauge::State::readFailed;
  format(s, p);
  TEST_ASSERT_EQUAL_STRING("Battery: read failed", p.lines[1]);
  s.battery.state = BatteryGauge::State::noBattery;
  format(s, p);
  TEST_ASSERT_EQUAL_STRING("Battery: not detected", p.lines[1]);
  s.graphicsFailure = 2;
  format(s, p);
  TEST_ASSERT_EQUAL_STRING("LCD: PSRAM alloc failed", p.lines[1]);
}
static void health_distinguishes_unavailable_and_valid_zero_with_explicit_units() {
  Health h;char line[96];
  formatHealthLine(h,0,line,sizeof(line));TEST_ASSERT_EQUAL_STRING("Internal: unavailable",line);
  formatHealthLine(h,2,line,sizeof(line));TEST_ASSERT_EQUAL_STRING("PSRAM: unavailable",line);
  h.sampled=true;
  formatHealthLine(h,0,line,sizeof(line));TEST_ASSERT_EQUAL_STRING("Internal: 0 KiB (low 0)",line);
  formatHealthLine(h,1,line,sizeof(line));TEST_ASSERT_EQUAL_STRING("Largest block: 0 KiB",line);
  formatHealthLine(h,2,line,sizeof(line));TEST_ASSERT_EQUAL_STRING("PSRAM: not present",line);
  h.psramPresent=true;formatHealthLine(h,2,line,sizeof(line));TEST_ASSERT_EQUAL_STRING("PSRAM free: 0 KiB",line);
  formatHealthLine(h,3,line,sizeof(line));TEST_ASSERT_EQUAL_STRING("Loop: awaiting sample",line);
  h.loopSampled=true;formatHealthLine(h,3,line,sizeof(line));TEST_ASSERT_EQUAL_STRING("Loop: 0/s max 0.000 ms",line);
}
static void health_units_and_maximum_values_are_bounded() {
  Health h;h.sampled=h.psramPresent=h.loopSampled=true;
  h.internalFreeBytes=65536;h.internalMinimumBytes=32768;h.internalLargestBytes=16384;h.psramFreeBytes=6u*1024*1024;h.loopRate=400;h.loopMaxUs=12345;
  char line[96];formatHealthLine(h,0,line,sizeof(line));TEST_ASSERT_EQUAL_STRING("Internal: 64 KiB (low 32)",line);
  formatHealthLine(h,3,line,sizeof(line));TEST_ASSERT_EQUAL_STRING("Loop: 400/s max 12.345 ms",line);
  h.internalFreeBytes=h.internalMinimumBytes=h.internalLargestBytes=h.psramFreeBytes=h.loopRate=h.loopMaxUs=UINT32_MAX;
  formatHealthLine(h,3,line,sizeof(line));TEST_ASSERT_EQUAL_STRING("Loop: 4294967k/s max 4294.967 s",line);
  for(uint8_t row=0;row<4;row++) {formatHealthLine(h,row,line,sizeof(line));TEST_ASSERT_LESS_THAN(60,strlen(line));}
  char bounded[5]={'x','x','x','x','!'};formatHealthLine(h,0,bounded,4);TEST_ASSERT_EQUAL('!',bounded[4]);TEST_ASSERT_EQUAL(0,bounded[3]);
}
int main() { UNITY_BEGIN(); RUN_TEST(health_distinguishes_unavailable_and_valid_zero_with_explicit_units); RUN_TEST(health_units_and_maximum_values_are_bounded); RUN_TEST(battery_readings_and_failures_are_explicit); RUN_TEST(states); RUN_TEST(lifecycle); RUN_TEST(failure); RUN_TEST(busy_and_wrap); RUN_TEST(disabled_fallback_timeouts_keep_the_status_awake); return UNITY_END(); }
