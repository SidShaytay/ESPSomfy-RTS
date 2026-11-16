#include <unity.h>
#include "WifiLiveness.h"

void setUp() {}
void tearDown() {}

using Action = WifiLiveness::Action;
static const uint64_t QUIET = WIFI_LIVENESS_QUIET_MS;
static const uint64_t PROBE = WIFI_LIVENESS_PROBE_MS;
// From the last frame to the reconnect: the quiet window, then one interval
// for each ARP request.
static const uint64_t GIVE_UP = QUIET + WIFI_LIVENESS_PROBES * PROBE;

// Runs loop() every `step` ms from `from` to `to`.  While `talking`, a frame
// arrives on every pass.  Counts the probes; returns the time of the first
// reconnect, or 0 if there was none.
static uint64_t run(WifiLiveness &l, uint64_t from, uint64_t to, bool talking, int *probes = nullptr,
    bool deferred = false, uint64_t step = 1000) {
  for(uint64_t t = from; t <= to; t += step) {
    if(talking) l.heard(t);
    const Action a = l.poll(t, deferred);
    if(a == Action::reconnect) return t;
    if(a == Action::probe && probes) (*probes)++;
  }
  return 0;
}

static void test_nothing_happens_before_the_link_is_up() {
  WifiLiveness l;
  TEST_ASSERT_EQUAL(Action::none, l.poll(10 * GIVE_UP, false));
}
static void test_a_busy_link_never_sends_anything() {
  WifiLiveness l;
  l.linkUp(0);
  int probes = 0;
  TEST_ASSERT_EQUAL(0, run(l, 0, 24ULL * 3600000, true, &probes));
  TEST_ASSERT_EQUAL(0, probes);
}
static void test_frames_a_minute_apart_never_probe() {
  WifiLiveness l;
  l.linkUp(0);
  for(uint64_t t = 0; t < 24ULL * 3600000; t += 1000) {
    if(t % 60000 == 0) l.heard(t);
    TEST_ASSERT_EQUAL(Action::none, l.poll(t, false));
  }
}
static void test_a_quiet_link_asks_the_gateway_before_giving_up() {
  WifiLiveness l;
  l.linkUp(0);
  TEST_ASSERT_EQUAL(Action::none, l.poll(QUIET - 1, false));
  TEST_ASSERT_EQUAL(Action::probe, l.poll(QUIET, false));
  TEST_ASSERT_EQUAL(Action::none, l.poll(QUIET + PROBE - 1, false));
  TEST_ASSERT_EQUAL(Action::probe, l.poll(QUIET + PROBE, false));
  TEST_ASSERT_EQUAL(Action::probe, l.poll(QUIET + 2 * PROBE, false));
  TEST_ASSERT_EQUAL(Action::none, l.poll(GIVE_UP - 1, false));
  TEST_ASSERT_EQUAL(Action::reconnect, l.poll(GIVE_UP, false));
}
static void test_an_arp_reply_ends_the_check() {
  WifiLiveness l;
  l.linkUp(0);
  TEST_ASSERT_EQUAL(Action::probe, l.poll(QUIET, false));
  l.heard(QUIET + 20);  // The gateway answered.
  int probes = 0;
  TEST_ASSERT_EQUAL(0, run(l, QUIET + 1000, QUIET + 1000 + QUIET - 1000, false, &probes));
  TEST_ASSERT_EQUAL(0, probes);
}
static void test_a_stalled_loop_still_waits_between_requests() {
  // The hook stamps frames on its own, so a stall can't fake silence, but
  // after one the requests must still be spaced out for replies to arrive.
  WifiLiveness l;
  l.linkUp(0);
  TEST_ASSERT_EQUAL(Action::probe, l.poll(10 * GIVE_UP, false));
  TEST_ASSERT_EQUAL(Action::none, l.poll(10 * GIVE_UP + 1, false));
  TEST_ASSERT_EQUAL(Action::none, l.poll(10 * GIVE_UP + 2, false));
}
static void test_an_old_stamp_is_ignored() {
  WifiLiveness l;
  l.linkUp(50000);
  l.heard(10000);  // From before this join.
  TEST_ASSERT_EQUAL(Action::probe, l.poll(50000 + QUIET, false));
}

// The 2026-11-15 incident: the AP restarts, the board rejoins on channel 3
// and talks for about two minutes, then the AP is on channel 9 and nothing
// reaches the board while the driver still says associated.  It must rejoin.
static void test_regression_silent_link_after_ap_restart_rejoins() {
  WifiLiveness l;
  l.linkUp(0);
  TEST_ASSERT_EQUAL(0, run(l, 0, 120000, true));
  int probes = 0;
  const uint64_t at = run(l, 121000, 120000 + 3600000, false, &probes);
  TEST_ASSERT_EQUAL_UINT64(120000 + GIVE_UP, at);
  TEST_ASSERT_EQUAL(WIFI_LIVENESS_PROBES, probes);
  // It asks once, then waits for the rejoin.
  TEST_ASSERT_FALSE(l.isUp());
  TEST_ASSERT_EQUAL(Action::none, l.poll(at + 1000, false));
}
static void test_an_update_defers_the_reconnect() {
  WifiLiveness l;
  l.linkUp(0);
  TEST_ASSERT_EQUAL(0, run(l, 0, 3600000, false, nullptr, true));
  TEST_ASSERT_TRUE(l.isUp());
  TEST_ASSERT_EQUAL(Action::reconnect, l.poll(3600001, false));
}
static void test_each_silent_rejoin_doubles_the_wait_up_to_15_minutes() {
  WifiLiveness l;
  uint64_t t = 0;
  const uint64_t expected[] = {120000, 240000, 480000, 900000, 900000};
  for(uint64_t window : expected) {
    l.linkUp(t);
    TEST_ASSERT_EQUAL_UINT64(window, l.quietWindow());
    const uint64_t at = run(l, t, t + 3600000, false);
    TEST_ASSERT_EQUAL_UINT64(t + window + WIFI_LIVENESS_PROBES * PROBE, at);
    t = at + 5000;
  }
}
static void test_traffic_soon_after_a_rejoin_keeps_the_longer_wait() {
  // DHCP and the first chatter after a join don't prove the link yet.
  WifiLiveness l;
  l.linkUp(0);
  const uint64_t at = run(l, 0, 3600000, false);
  l.linkUp(at + 5000);
  l.heard(at + 5000 + QUIET - 1);
  TEST_ASSERT_EQUAL_UINT64(2 * QUIET, l.quietWindow());
}
static void test_traffic_a_full_window_after_a_rejoin_resets_the_wait() {
  WifiLiveness l;
  l.linkUp(0);
  const uint64_t at = run(l, 0, 3600000, false);
  l.linkUp(at + 5000);
  run(l, at + 5000, at + 5000 + QUIET, true);
  TEST_ASSERT_EQUAL_UINT64(QUIET, l.quietWindow());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_nothing_happens_before_the_link_is_up);
  RUN_TEST(test_a_busy_link_never_sends_anything);
  RUN_TEST(test_frames_a_minute_apart_never_probe);
  RUN_TEST(test_a_quiet_link_asks_the_gateway_before_giving_up);
  RUN_TEST(test_an_arp_reply_ends_the_check);
  RUN_TEST(test_a_stalled_loop_still_waits_between_requests);
  RUN_TEST(test_an_old_stamp_is_ignored);
  RUN_TEST(test_regression_silent_link_after_ap_restart_rejoins);
  RUN_TEST(test_an_update_defers_the_reconnect);
  RUN_TEST(test_each_silent_rejoin_doubles_the_wait_up_to_15_minutes);
  RUN_TEST(test_traffic_soon_after_a_rejoin_keeps_the_longer_wait);
  RUN_TEST(test_traffic_a_full_window_after_a_rejoin_resets_the_wait);
  return UNITY_END();
}
