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
  TEST_ASSERT_EQUAL(0, e.sample(1));
  TEST_ASSERT_EQUAL(0, e.sample(3)); // bounce
  TEST_ASSERT_EQUAL(0, e.sample(1));
  TEST_ASSERT_EQUAL(1, e.sample(0)); // clockwise detent
  TEST_ASSERT_EQUAL(0, e.sample(2));
  TEST_ASSERT_EQUAL(1, e.sample(3)); // second detent
  TEST_ASSERT_EQUAL(0, e.sample(2));
  TEST_ASSERT_EQUAL(-1, e.sample(0)); // anticlockwise
  TEST_ASSERT_EQUAL(0, e.sample(1));
  TEST_ASSERT_EQUAL(-1, e.sample(3));
  TEST_ASSERT_EQUAL(0, e.sample(0));
  TEST_ASSERT_EQUAL(0, e.sample(3)); // invalid jumps
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
  DisplayPreferences saved;
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
  TEST_ASSERT_FALSE(c.takePreferences(saved));
  click(c, 400, m);
  click(c, 600, m);
  click(c, 800, m);
  TEST_ASSERT_TRUE(c.takePreferences(saved));
  TEST_ASSERT_EQUAL(16, saved.brightness);
  TEST_ASSERT_FALSE(c.take(command));
  c.tick(11000, m, 0, false, true);
  TEST_ASSERT_EQUAL(4, c.brightness());
  c.tick(21000, m, 0, false, true);
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
static void separate_back_button_and_disabled_timeouts() {
  Controller c;
  auto m = model();
  Command cmd;
  c.tick(0, m, 1, false, true);
  click(c, 100, m);
  c.tick(400, m, 0, false, true, true);
  c.tick(441, m, 0, false, true, true);
  TEST_ASSERT_EQUAL((int)Screen::choose, (int)c.view().screen);
  TEST_ASSERT_FALSE(c.take(cmd));
  c.tick(500, m, 0, false, true, false);
  c.tick(541, m, 0, false, true, false);
  click(c, 600, m);
  TEST_ASSERT_EQUAL((int)Screen::control, (int)c.view().screen); // next knob press still works
  Controller always;
  always.setPreferences({32, 0, 0});
  always.tick(0, m, 0, false, true);
  always.tick(600000, m, 0, false, true);
  TEST_ASSERT_EQUAL(32, always.brightness());
  Controller dimOnly;
  dimOnly.setPreferences({32, 5, 0});
  dimOnly.tick(0, m, 0, false, true);
  dimOnly.tick(6000, m, 0, false, true);
  TEST_ASSERT_EQUAL(4, dimOnly.brightness());
  Controller sleepOnly;
  sleepOnly.setPreferences({32, 0, 5});
  sleepOnly.tick(0, m, 0, false, true);
  sleepOnly.tick(6000, m, 0, false, true);
  TEST_ASSERT_EQUAL(0, sleepOnly.brightness());
  sleepOnly.tick(7000, m, 0, false, true, true);
  sleepOnly.tick(7041, m, 0, false, true, true);
  TEST_ASSERT_EQUAL(32, sleepOnly.brightness());
  TEST_ASSERT_EQUAL((int)Screen::status, (int)sleepOnly.view().screen);
}
static void no_dim_no_sleep_are_selectable_and_saved() {
  Controller c;
  auto m = model();
  DisplayPreferences saved;
  Command cmd;
  c.tick(0, m, 1, false, true);
  for(int i = 0; i < 4; i++)
    c.tick(10 + i, m, 1, false, true);
  click(c, 100, m);
  click(c, 300, m); // display settings, then dim row
  c.tick(500, m, -1, false, true);
  c.tick(510, m, -1, false, true);
  TEST_ASSERT_EQUAL(0, c.view().dimSeconds);
  click(c, 600, m);
  for(int i = 0; i < 4; i++)
    c.tick(800 + i, m, -1, false, true);
  TEST_ASSERT_EQUAL(0, c.view().offSeconds);
  c.tick(1000, m, 0, false, true, true);
  c.tick(1041, m, 0, false, true, true);
  TEST_ASSERT_TRUE(c.takePreferences(saved));
  TEST_ASSERT_EQUAL(0, saved.dimSeconds);
  TEST_ASSERT_EQUAL(0, saved.offSeconds);
  TEST_ASSERT_FALSE(c.take(cmd));
}
static void moving_stop_is_not_blocked_by_a_repaint() {
  Controller c;
  auto m = model();
  m.targets[0].moving = true;
  Command cmd;
  c.tick(0, m, 1, false, true);
  click(c, 100, m);
  click(c, 300, m, false);
  TEST_ASSERT_TRUE(c.take(cmd));
  TEST_ASSERT_EQUAL((int)Action::stop, (int)cmd.action);
}
static void details_wheel_scrolls_without_leaving_or_sending() {
  Controller c;
  auto m = model();
  Command cmd;
  c.tick(0, m, 0, false, true);
  c.tick(100, m, 0, true, true);
  c.tick(141, m, 0, true, true);
  c.tick(942, m, 0, true, true);
  c.tick(1000, m, 0, false, true);
  c.tick(1041, m, 0, false, true);
  TEST_ASSERT_EQUAL((int)Screen::details, (int)c.view().screen);
  c.tick(1100, m, 1, false, true);
  TEST_ASSERT_EQUAL((int)Screen::details, (int)c.view().screen);
  TEST_ASSERT_EQUAL(1, c.view().detailsOffset);
  for(int i = 0; i < 20; ++i) c.tick(1200 + i, m, 1, false, true);
  TEST_ASSERT_EQUAL(detailsRowCount - detailsVisibleRows, c.view().detailsOffset);
  for(int i = 0; i < 20; ++i) c.tick(1300 + i, m, -1, false, true);
  TEST_ASSERT_EQUAL(0, c.view().detailsOffset);
  TEST_ASSERT_FALSE(c.take(cmd));
  click(c, 1400, m);
  TEST_ASSERT_EQUAL((int)Screen::status, (int)c.view().screen);
}
static void secured_controls_unlock_without_replaying_and_stop_lease_is_narrow() {
  Controller c; auto m=model(); m.authorized=false; m.authMode=AuthMode::pin; m.authGeneration=4;
  m.targets[0].moving=true;
  c.tick(0,m,1,false,true); click(c,100,m); click(c,300,m);
  TEST_ASSERT_EQUAL(int(Screen::unlock),int(c.view().screen));
  Command command; TEST_ASSERT_FALSE(c.take(command));
  c.tick(500,m,1,false,true); // choose !
  uint32_t time=600;
  for(int i=0;i<4;i++,time+=200) click(c,time,m);
  for(int i=0;i<96;i++) c.tick(time++,m,1,false,true); // Done
  click(c,time+100,m);
  Credentials given; TEST_ASSERT_TRUE(c.takeCredentials(given));
  TEST_ASSERT_EQUAL_STRING("!!!!",given.secret);
  c.unlockResult(time+230,AuthResult::unlocked);
  TEST_ASSERT_EQUAL(int(Screen::control),int(c.view().screen));
  TEST_ASSERT_FALSE(c.take(command)); // unlocking does not replay previous action
  m.targets[0].stopLease=true;
  click(c,time+500,m);
  TEST_ASSERT_TRUE(c.take(command));
  TEST_ASSERT_EQUAL(int(Action::stop),int(command.action));
  TEST_ASSERT_EQUAL(4,command.authGeneration);
  c.tick(time+700,m,-1,false,true); // UP is not covered by the lease
  click(c,time+800,m);
  TEST_ASSERT_EQUAL(int(Screen::unlock),int(c.view().screen));
  TEST_ASSERT_FALSE(c.take(command));
  c.tick(time+1100,m,0,false,true,true); c.tick(time+1141,m,0,false,true,true);
  TEST_ASSERT_EQUAL(int(Screen::control),int(c.view().screen));
  TEST_ASSERT_FALSE(c.takeCredentials(given));
}
static void secret_entry_wipes_on_sleep_and_motion_independent_timeout() {
  for(unsigned state=0;state<2;state++) {
    const bool moving=state!=0;
    Controller c; auto m=model(); m.authorized=false; m.authMode=AuthMode::pin;
    m.targets[0].moving=moving;
    c.tick(0,m,1,false,true); click(c,100,m); click(c,300,m);
    c.tick(500,m,1,false,true); click(c,600,m);
    TEST_ASSERT_EQUAL(1,c.view().credentialLength);
    c.tick(moving?31000:21000,m,0,false,true);
    TEST_ASSERT_NOT_EQUAL(int(Screen::unlock),int(c.view().screen));
    TEST_ASSERT_EQUAL(0,c.view().credentialLength);
    Credentials out; TEST_ASSERT_FALSE(c.takeCredentials(out));
    Command command; TEST_ASSERT_FALSE(c.take(command));
  }
}
static void same_mode_credential_change_wipes_unlock_draft_immediately() {
  Controller c; auto m=model(); m.authorized=false; m.authMode=AuthMode::pin; m.authGeneration=2;
  m.targets[0].moving=true;
  c.tick(0,m,1,false,true); click(c,100,m); click(c,300,m);
  c.tick(500,m,1,false,true); click(c,600,m);
  TEST_ASSERT_EQUAL(1,c.view().credentialLength);
  ++m.authGeneration; // credential changed, same mode
  ++m.authConfigurationGeneration;
  c.tick(800,m,0,false,true);
  TEST_ASSERT_NOT_EQUAL(int(Screen::unlock),int(c.view().screen));
  TEST_ASSERT_EQUAL(0,c.view().credentialLength);
  Credentials out; TEST_ASSERT_FALSE(c.takeCredentials(out));
  Command command; TEST_ASSERT_FALSE(c.take(command));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(details_wheel_scrolls_without_leaving_or_sending);
  RUN_TEST(navigation_does_not_emit);
  RUN_TEST(wake_consumes_input_and_hold_is_back);
  RUN_TEST(stop_identity_and_presentation_guards);
  RUN_TEST(decoder_rejects_bounce_and_partial_steps);
  RUN_TEST(preview_and_boot_hold_never_emit);
  RUN_TEST(memory_fallback_has_an_explicit_cause);
  RUN_TEST(estimated_motion_keeps_stop_awake);
  RUN_TEST(brightness_and_twenty_second_idle_timeout);
  RUN_TEST(changed_group_members_and_mixed_gestures_are_blocked);
  RUN_TEST(separate_back_button_and_disabled_timeouts);
  RUN_TEST(no_dim_no_sleep_are_selectable_and_saved);
  RUN_TEST(moving_stop_is_not_blocked_by_a_repaint);
  RUN_TEST(secured_controls_unlock_without_replaying_and_stop_lease_is_narrow);
  RUN_TEST(secret_entry_wipes_on_sleep_and_motion_independent_timeout);
  RUN_TEST(same_mode_credential_change_wipes_unlock_draft_immediately);
  return UNITY_END();
}
