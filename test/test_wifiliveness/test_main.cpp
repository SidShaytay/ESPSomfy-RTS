#include <unity.h>
#include "WifiLiveness.h"

void setUp() {}
void tearDown() {}

using Action = WifiLiveness::Action;
static const uint32_t GW_A = 0x0101A8C0;  // 192.168.1.1
static const uint32_t GW_B = 0x0102A8C0;  // 192.168.2.1

// Runs loop() once a second from `from` to `to`, answering each probe with
// `ok`.  Returns the time of the first reconnect, or 0 if there was none.
static uint64_t run(WifiLiveness &l, uint64_t from, uint64_t to, bool ok, bool deferred = false) {
  for(uint64_t t = from; t <= to; t += 1000) {
    const Action a = l.poll(t, false, deferred);
    if(a == Action::reconnect) return t;
    if(a == Action::probe) l.probed(ok, t);
  }
  return 0;
}

static void test_nothing_happens_while_the_link_is_down() {
  WifiLiveness l;
  TEST_ASSERT_EQUAL(Action::none, l.poll(1000000, false, false));
  l.linkUp(GW_A, 0);
  l.linkDown();
  TEST_ASSERT_EQUAL(0, run(l, 0, 3600000, false));
}
static void test_no_gateway_means_no_probing() {
  WifiLiveness l;
  l.linkUp(0, 0);
  TEST_ASSERT_FALSE(l.isUp());
  TEST_ASSERT_EQUAL(Action::none, l.poll(WIFI_LIVENESS_PROBE_MS, false, false));
}
static void test_it_probes_every_interval() {
  WifiLiveness l;
  l.linkUp(GW_A, 1000);
  TEST_ASSERT_EQUAL(Action::none, l.poll(1000 + WIFI_LIVENESS_PROBE_MS - 1, false, false));
  TEST_ASSERT_EQUAL(Action::probe, l.poll(1000 + WIFI_LIVENESS_PROBE_MS, false, false));
  TEST_ASSERT_EQUAL(Action::none, l.poll(1000 + WIFI_LIVENESS_PROBE_MS + 1, false, false));
  TEST_ASSERT_EQUAL(Action::probe, l.poll(1000 + 2 * WIFI_LIVENESS_PROBE_MS, false, false));
}
static void test_a_ping_still_out_holds_the_next_probe() {
  WifiLiveness l;
  l.linkUp(GW_A, 0);
  TEST_ASSERT_EQUAL(Action::none, l.poll(WIFI_LIVENESS_PROBE_MS, true, false));
  TEST_ASSERT_EQUAL(Action::probe, l.poll(WIFI_LIVENESS_PROBE_MS + 500, false, false));
}
static void test_a_healthy_link_never_reconnects() {
  WifiLiveness l;
  l.linkUp(GW_A, 0);
  TEST_ASSERT_EQUAL(0, run(l, 0, 24ULL * 3600000, true));
  TEST_ASSERT_TRUE(l.gatewayAnswered());
}
static void test_a_gateway_that_never_answers_never_forces_a_reconnect() {
  WifiLiveness l;
  l.linkUp(GW_A, 0);
  TEST_ASSERT_EQUAL(0, run(l, 0, 24ULL * 3600000, false));
  TEST_ASSERT_FALSE(l.gatewayAnswered());
}

// The 2026-11-15 incident: the board rejoins after an AP restart, the gateway
// answers for about two minutes, then the link carries nothing while the
// driver still says associated.  It must rejoin within a bounded time.
static void test_regression_silent_link_after_ap_restart_rejoins() {
  WifiLiveness l;
  l.linkUp(GW_A, 0);
  TEST_ASSERT_EQUAL(0, run(l, 0, 120000, true));
  const uint64_t silentFrom = 120001;
  const uint64_t at = run(l, silentFrom, silentFrom + 3600000, false);
  TEST_ASSERT_NOT_EQUAL(0, at);
  // No answer since the last good probe at 120 s, so it fires one dead window
  // after that, give or take a probe interval.
  TEST_ASSERT_UINT64_WITHIN(WIFI_LIVENESS_PROBE_MS, 120000 + WIFI_LIVENESS_DEAD_MS, at);
  TEST_ASSERT_GREATER_OR_EQUAL(WIFI_LIVENESS_MIN_FAILURES, l.failedProbes());
  // It asks once, then waits for the rejoin.
  TEST_ASSERT_FALSE(l.isUp());
  TEST_ASSERT_EQUAL(Action::none, l.poll(at + 1000, false, false));
}
static void test_a_short_outage_does_not_reconnect() {
  WifiLiveness l;
  l.linkUp(GW_A, 0);
  TEST_ASSERT_EQUAL(0, run(l, 0, 60000, true));
  TEST_ASSERT_EQUAL(0, run(l, 60001, 60000 + WIFI_LIVENESS_DEAD_MS - 2 * WIFI_LIVENESS_PROBE_MS, false));
  TEST_ASSERT_EQUAL(0, run(l, 60000 + WIFI_LIVENESS_DEAD_MS - 2 * WIFI_LIVENESS_PROBE_MS + 1000, 3600000, true));
}
static void test_silence_alone_needs_failed_probes_too() {
  // A loop stalled for longer than the dead window has sent no probes, so the
  // gap alone is not evidence of a dead link.
  WifiLiveness l;
  l.linkUp(GW_A, 0);
  TEST_ASSERT_EQUAL(Action::probe, l.poll(WIFI_LIVENESS_PROBE_MS, false, false));
  l.probed(true, WIFI_LIVENESS_PROBE_MS);
  TEST_ASSERT_EQUAL(Action::probe, l.poll(10 * WIFI_LIVENESS_DEAD_MS, false, false));
}
static void test_an_update_defers_the_reconnect() {
  WifiLiveness l;
  l.linkUp(GW_A, 0);
  run(l, 0, 30000, true);
  TEST_ASSERT_EQUAL(0, run(l, 30001, 3600000, false, true));
  TEST_ASSERT_TRUE(l.isUp());
  TEST_ASSERT_EQUAL(Action::reconnect, l.poll(3600001, false, false));
}
static void test_each_fruitless_rejoin_doubles_the_wait_up_to_15_minutes() {
  WifiLiveness l;
  l.linkUp(GW_A, 0);
  run(l, 0, 30000, true);
  uint64_t t = 30001;
  const uint64_t expected[] = {90000, 180000, 360000, 720000, 900000, 900000};
  for(uint64_t window : expected) {
    TEST_ASSERT_EQUAL_UINT64(window, l.deadWindow());
    const uint64_t at = run(l, t, t + 3600000, false);
    TEST_ASSERT_NOT_EQUAL(0, at);
    // The rejoin succeeds but the gateway stays silent.
    l.linkUp(GW_A, at + 5000);
    t = at + 5000;
  }
}
static void test_an_answer_resets_the_wait() {
  WifiLiveness l;
  l.linkUp(GW_A, 0);
  run(l, 0, 30000, true);
  const uint64_t at = run(l, 30001, 3600000, false);
  l.linkUp(GW_A, at + 5000);
  TEST_ASSERT_EQUAL_UINT64(2 * WIFI_LIVENESS_DEAD_MS, l.deadWindow());
  run(l, at + 5000, at + 60000, true);
  TEST_ASSERT_EQUAL_UINT64(WIFI_LIVENESS_DEAD_MS, l.deadWindow());
}
static void test_a_new_gateway_must_answer_before_it_counts() {
  WifiLiveness l;
  l.linkUp(GW_A, 0);
  run(l, 0, 30000, true);
  l.linkDown();
  l.linkUp(GW_B, 40000);
  TEST_ASSERT_FALSE(l.gatewayAnswered());
  TEST_ASSERT_EQUAL(0, run(l, 40000, 3600000, false));
}
static void test_a_late_result_after_the_link_drops_is_ignored() {
  WifiLiveness l;
  l.linkUp(GW_A, 0);
  l.linkDown();
  l.probed(true, 1000);
  TEST_ASSERT_FALSE(l.gatewayAnswered());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_nothing_happens_while_the_link_is_down);
  RUN_TEST(test_no_gateway_means_no_probing);
  RUN_TEST(test_it_probes_every_interval);
  RUN_TEST(test_a_ping_still_out_holds_the_next_probe);
  RUN_TEST(test_a_healthy_link_never_reconnects);
  RUN_TEST(test_a_gateway_that_never_answers_never_forces_a_reconnect);
  RUN_TEST(test_regression_silent_link_after_ap_restart_rejoins);
  RUN_TEST(test_a_short_outage_does_not_reconnect);
  RUN_TEST(test_silence_alone_needs_failed_probes_too);
  RUN_TEST(test_an_update_defers_the_reconnect);
  RUN_TEST(test_each_fruitless_rejoin_doubles_the_wait_up_to_15_minutes);
  RUN_TEST(test_an_answer_resets_the_wait);
  RUN_TEST(test_a_new_gateway_must_answer_before_it_counts);
  RUN_TEST(test_a_late_result_after_the_link_drops_is_ignored);
  return UNITY_END();
}
