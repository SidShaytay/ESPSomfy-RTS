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
  TEST_ASSERT_NOT_NULL(strstr(p.lines[1], "unavailable"));
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
int main() { UNITY_BEGIN(); RUN_TEST(states); RUN_TEST(lifecycle); RUN_TEST(failure); RUN_TEST(busy_and_wrap); return UNITY_END(); }
