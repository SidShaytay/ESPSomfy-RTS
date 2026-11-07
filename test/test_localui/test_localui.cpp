#include <unity.h>
#include "LocalUi.h"
#include "LocalUiMemory.h"
#include "StatusDisplay.h"
#include <string.h>
using namespace LocalUi;
void setUp() {}
void tearDown() {}
static Model model() {
  Model m;
  m.ready = true;
  m.controlsEnabled = true;
  for(int i = 0; i < 3; i++) {
    m.targets[i].valid = true;
    m.targets[i].identity = i + 1;
  }
  return m;
}
static void click(Controller &c, uint32_t t, const Model &m, bool visible = true) {
  c.tick(t, m, 0, true, visible);
  c.tick(t + 41, m, 0, true, visible);
  c.tick(t + 80, m, 0, false, visible);
  c.tick(t + 121, m, 0, false, visible);
}
static void navigation_does_not_emit() {
  Controller c;
  auto m = model();
  Command cmd;
  c.tick(0, m, 0, false, true);
  TEST_ASSERT_FALSE(c.take(cmd));
  c.tick(100, m, 1, false, true);
  TEST_ASSERT_EQUAL((int)Screen::choose, (int)c.view().screen);
  click(c, 200, m);
  TEST_ASSERT_EQUAL((int)Screen::control, (int)c.view().screen);
  TEST_ASSERT_EQUAL((int)Action::stop, (int)c.view().action);
  TEST_ASSERT_FALSE(c.take(cmd));
  c.tick(400, m, -1, false, true);
  TEST_ASSERT_FALSE(c.take(cmd));
  click(c, 500, m);
  TEST_ASSERT_TRUE(c.take(cmd));
  TEST_ASSERT_EQUAL((int)Action::up, (int)cmd.action);
  TEST_ASSERT_FALSE(c.take(cmd));
  TEST_ASSERT_EQUAL((int)Action::stop, (int)c.view().action);
}
static void wake_consumes_input_and_hold_is_back() {
  Controller c;
  auto m = model();
  Command cmd;
  c.tick(0, m, 0, false, true);
  c.tick(61000, m, 0, false, true);
  TEST_ASSERT_EQUAL(0, c.brightness());
  click(c, 62000, m);
  TEST_ASSERT_EQUAL((int)Screen::status, (int)c.view().screen);
  TEST_ASSERT_FALSE(c.take(cmd));
  c.tick(62200, m, 1, false, true);
  click(c, 62300, m);
  c.tick(62500, m, 0, true, true);
  c.tick(62541, m, 0, true, true);
  c.tick(63400, m, 0, true, true);
  c.tick(63500, m, 0, false, true);
  c.tick(63541, m, 0, false, true);
  TEST_ASSERT_EQUAL((int)Screen::choose, (int)c.view().screen);
  TEST_ASSERT_FALSE(c.take(cmd));
}
static void stop_identity_and_presentation_guards() {
  Controller c;
  auto m = model();
  Command cmd;
  c.tick(0, m, 1, false, true);
  click(c, 100, m);
  click(c, 300, m);
  TEST_ASSERT_FALSE(c.take(cmd)); // idle STOP cannot mean favourite
  c.tick(500, m, -1, false, true);
  click(c, 600, m, false);
  TEST_ASSERT_FALSE(c.take(cmd));
  m.targets[0].identity = 999;
  click(c, 800, m);
  TEST_ASSERT_FALSE(c.take(cmd));
}
static void decoder_rejects_bounce_and_partial_steps() {
  Encoder e;
  TEST_ASSERT_EQUAL(0, e.sample(3));
  TEST_ASSERT_EQUAL(0, e.sample(2));
  TEST_ASSERT_EQUAL(0, e.sample(3));
  TEST_ASSERT_EQUAL(0, e.sample(2));
  TEST_ASSERT_EQUAL(0, e.sample(0));
  TEST_ASSERT_EQUAL(0, e.sample(1));
  TEST_ASSERT_EQUAL(1, e.sample(3));
  TEST_ASSERT_EQUAL(0, e.sample(0));
  TEST_ASSERT_EQUAL(0, e.sample(3)); // invalid jump
}
static void preview_and_boot_hold_never_emit() {
  Controller c;
  auto m = model();
  m.controlsEnabled = false;
  Command cmd;
  c.tick(0, m, 0, true, true);
  c.tick(1000, m, 0, true, true);
  c.tick(1100, m, 0, false, true);
  c.tick(1141, m, 0, false, true);
  TEST_ASSERT_EQUAL((int)Screen::status, (int)c.view().screen);
  uint32_t random = 71;
  for(unsigned i = 0; i < 20000; i++) {
    random = random * 1664525 + 1013904223;
    c.tick(1200 + i * 17, m, int8_t((random >> 8) % 3) - 1, (random >> 16) % 2, true);
    TEST_ASSERT_FALSE(c.take(cmd));
    TEST_ASSERT_LESS_THAN(5, c.view().selected);
  }
}
static unsigned allocations = 0;
static void *noMemory(size_t n) {
  ++allocations;
  TEST_ASSERT_EQUAL(108800, n);
  return nullptr;
}
static void memory_fallback_has_an_explicit_cause() {
  GraphicsMemory missing;
  missing.begin(false, noMemory);
  TEST_ASSERT_EQUAL(0, allocations);
  TEST_ASSERT_EQUAL((int)GraphicsFailure::missing, (int)missing.failure);
  GraphicsMemory failed;
  failed.begin(true, noMemory);
  TEST_ASSERT_EQUAL(1, allocations);
  TEST_ASSERT_NULL(failed.pixels);
  StatusDisplay::Snapshot s;
  s.graphicsFailure = uint8_t(failed.failure);
  StatusDisplay::Page p;
  StatusDisplay::format(s, p);
  TEST_ASSERT_NOT_NULL(strstr(p.lines[1], "PSRAM alloc failed"));
  s.graphicsFailure = uint8_t(missing.failure);
  StatusDisplay::format(s, p);
  TEST_ASSERT_NOT_NULL(strstr(p.lines[1], "PSRAM not found"));
}
static void estimated_motion_keeps_stop_awake() {
  Controller c;
  auto m = model();
  m.targets[0].moving = true;
  c.tick(0, m, 0, false, true);
  c.tick(70000, m, 0, false, true);
  TEST_ASSERT_EQUAL(32, c.brightness());
  m.targets[0].moving = false;
  c.tick(130001, m, 0, false, true);
  TEST_ASSERT_EQUAL(0, c.brightness());
}
static void brightness_and_twenty_second_idle_timeout() {
  Controller c;
  auto m = model();
  uint8_t saved = 0;
  Command command;
  c.tick(0, m, 0, false, true);
  c.tick(1, m, 1, false, true);
  for(unsigned i = 0; i < 4; i++)
    c.tick(10 + i, m, 1, false, true);
  TEST_ASSERT_EQUAL(4, c.view().selected);
  click(c, 100, m);
  TEST_ASSERT_EQUAL((int)Screen::brightness, (int)c.view().screen);
  c.tick(300, m, -1, false, true);
  TEST_ASSERT_EQUAL(16, c.brightness());
  TEST_ASSERT_FALSE(c.takeBrightness(saved));
  click(c, 400, m);
  TEST_ASSERT_TRUE(c.takeBrightness(saved));
  TEST_ASSERT_EQUAL(16, saved);
  TEST_ASSERT_FALSE(c.take(command));
  c.tick(10500, m, 0, false, true);
  TEST_ASSERT_EQUAL(4, c.brightness());
  c.tick(20500, m, 0, false, true);
  TEST_ASSERT_EQUAL(0, c.brightness());
  c.setBrightness(0);
  TEST_ASSERT_EQUAL(32, c.preference());
}
static void changed_group_members_and_mixed_gestures_are_blocked() {
  Controller c;
  auto m = model();
  Command cmd;
  m.targets[2].members[0] = 1;
  m.targets[2].members[1] = 2;
  c.tick(0, m, 1, false, true);
  c.tick(10, m, 1, false, true);
  c.tick(20, m, 1, false, true);
  click(c, 100, m);
  c.tick(300, m, -1, false, true);
  m.targets[2].members[1] = 99;
  click(c, 400, m);
  TEST_ASSERT_FALSE(c.take(cmd));
  Controller single;
  single.tick(0, m, 1, false, true);
  click(single, 100, m);
  single.tick(300, m, -1, false, true);
  single.tick(400, m, 0, true, true);
  single.tick(441, m, 0, true, true);
  single.tick(500, m, 0, false, true);
  single.tick(541, m, 1, false, true);
  TEST_ASSERT_FALSE(single.take(cmd));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(navigation_does_not_emit);
  RUN_TEST(wake_consumes_input_and_hold_is_back);
  RUN_TEST(stop_identity_and_presentation_guards);
  RUN_TEST(decoder_rejects_bounce_and_partial_steps);
  RUN_TEST(preview_and_boot_hold_never_emit);
  RUN_TEST(memory_fallback_has_an_explicit_cause);
  RUN_TEST(estimated_motion_keeps_stop_awake);
  RUN_TEST(brightness_and_twenty_second_idle_timeout);
  RUN_TEST(changed_group_members_and_mixed_gestures_are_blocked);
  return UNITY_END();
}
