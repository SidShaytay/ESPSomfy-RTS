#include <unity.h>
#include <algorithm>
#include <vector>
#include "ShadeBehaviour.h"

#define ASSERT_POS(expected, actual) TEST_ASSERT_FLOAT_WITHIN(0.05f, (expected), (actual))
#define ASSERT_CMD(expected, actual) TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(expected), static_cast<uint8_t>(actual))

// Physical transport and notifications are fake; every state transition,
// position update and automatic Stop uses the production ShadeBehaviour.
struct Sent {
  somfy_commands command;
  uint32_t ticket;
  uint32_t queuedAt;
  uint8_t repeats;
};
struct Link : MotorLink {
  somfy_frame_t &frame;
  const RemoteSettings &remote;
  const uint32_t &now;
  std::vector<Sent> sent;
  uint32_t nextTicket = 1;
  Link(somfy_frame_t &frame, const RemoteSettings &remote, const uint32_t &now)
    : frame(frame), remote(remote), now(now) {}
  send_result_t send(somfy_commands command, uint8_t repeats, uint8_t step,
      uint16_t, bool) override {
    somfy_frame_t candidate = frame;
    candidate.cmd = remote.flipCommands ? flipCommand(command) : command;
    candidate.remoteAddress = 0x123456;
    candidate.rollingCode++;
    candidate.repeats = repeats;
    candidate.stepSize = step;
    candidate.bitLength = remote.bitLength;
    candidate.valid = true;
    candidate.processed = false;
    somfy_tx_job_t job;
    encodeTxJob(job, candidate, repeats);
    frame = candidate;
    const uint32_t ticket = nextTicket++;
    sent.push_back({frame.cmd, ticket, now, repeats});
    return {true, ticket};
  }
};
struct Events : ShadeEvents {
  unsigned saves = 0;
  void onChanged(uint16_t) override {}
  void onCommand(somfy_commands, const char *, uint32_t) override {}
  void onSaveWanted() override { ++saves; }
  void onSensorFlags() override {}
};
struct Rig {
  ShadeSettings settings;
  RemoteSettings remote;
  somfy_frame_t frame;
  uint32_t now = 1000;
  Link link{frame, remote, now};
  Events events;
  uint8_t id = 1;
  char name[21] = "Calibrated";
  ShadeBehaviour behaviour{settings, remote, frame, link, events, id, name};
  Rig() {
    remote.bitLength = 56;
    settings.upTime = 20000;
    settings.downTime = 10000;
    settings.calibration.kind = LiftKind::segmented;
    settings.calibration.down = {1000, 3000, 1000, 200};
    settings.calibration.up = {2000, 10000, 2000, 500};
    behaviour.restore(0, 0, 70, -1);
  }
  const ShadeBehaviour::State &state() const { return behaviour.state(); }
  void run(uint32_t duration) {
    while(duration) {
      const auto step = std::min(10u, duration);
      duration -= step;
      now += step;
      behaviour.tick(now);
    }
  }
  void event(tx_event_t type, uint32_t ticket) {
    somfy_tx_event_t outcome;
    outcome.type = type;
    outcome.ticket = ticket;
    outcome.at = now;
    behaviour.onTxEvent(outcome);
  }
  void completePress(uint32_t ticket) {
    event(tx_event_t::started, ticket);
    run(274); // Complete 56-bit start press, not an instantaneous job.
    event(tx_event_t::finished, ticket);
  }
  uint32_t individual(float target) {
    TEST_ASSERT_TRUE(behaviour.moveToTarget(target, -1, now));
    const auto ticket = link.sent.back().ticket;
    const auto start = now;
    completePress(ticket);
    return start;
  }
  uint32_t group(float target) {
    // The real group adapter adopts one shared ticket in every member. It
    // must not queue another member-addressed directional press here.
    const auto start = now;
    behaviour.adoptGroupTarget(target, 81, now);
    completePress(81);
    return start;
  }
  void until(uint32_t absolute) {
    TEST_ASSERT_TRUE(absolute >= now);
    run(absolute - now);
  }
};
void setUp() {}
void tearDown() {}

static void test_individual_position_waits_for_on_air_then_saved_endpoint_takeup() {
  Rig r;
  TEST_ASSERT_TRUE(r.behaviour.moveToTarget(50, -1, r.now));
  const auto ticket = r.link.sent.back().ticket;
  r.run(3000);
  ASSERT_POS(0, r.state().currentPos);
  const auto onAir = r.now;
  r.completePress(ticket);
  r.until(onAir + 700);
  ASSERT_POS(0, r.state().currentPos);
  r.until(onAir + 2500);
  ASSERT_POS(25, r.state().currentPos);
}
static void test_individual_intermediate_stop_uses_nonlinear_visible_deadline() {
  Rig r;
  const auto start = r.individual(50);
  r.until(start + 3990);
  TEST_ASSERT_EQUAL_UINT(1, r.link.sent.size());
  r.until(start + 4000);
  TEST_ASSERT_EQUAL_UINT(2, r.link.sent.size());
  ASSERT_CMD(somfy_commands::My, r.link.sent.back().command);
  TEST_ASSERT_EQUAL_UINT32(start + 4000, r.link.sent.back().queuedAt);
}
static void test_group_member_intermediate_stop_uses_same_saved_profile() {
  Rig r;
  const auto start = r.group(50);
  r.until(start + 3990);
  TEST_ASSERT_EQUAL_UINT(0, r.link.sent.size());
  r.until(start + 4000);
  TEST_ASSERT_EQUAL_UINT(1, r.link.sent.size());
  ASSERT_CMD(somfy_commands::My, r.link.sent.back().command);
  TEST_ASSERT_EQUAL_UINT32(start + 4000, r.link.sent.back().queuedAt);
}
static void test_individual_and_group_estimates_share_mechanical_and_nonlinear_phases() {
  Rig individual, group;
  const auto start = individual.individual(100);
  TEST_ASSERT_EQUAL_UINT32(start, group.group(100));
  const uint32_t elapsed[] = {700, 1500, 2500, 4000, 6500, 9000, 10000};
  const float visible[] = {0, 8.333333f, 25, 50, 75, 100, 100};
  for(unsigned i = 0; i < sizeof(elapsed) / sizeof(elapsed[0]); ++i) {
    individual.until(start + elapsed[i]);
    group.until(start + elapsed[i]);
    ASSERT_POS(visible[i], individual.state().currentPos);
    ASSERT_POS(visible[i], group.state().currentPos);
  }
  TEST_ASSERT_EQUAL_UINT(1, individual.link.sent.size());
  TEST_ASSERT_EQUAL_UINT(0, group.link.sent.size());
}
static void check_endpoint_settling(bool grouped) {
  Rig r;
  const auto start = grouped ? r.group(100) : r.individual(100);
  r.until(start + 9000);
  ASSERT_POS(100, r.state().currentPos);
  // This is a model deadline, not a claim of human-confirmed motor position.
  TEST_ASSERT_FALSE(r.behaviour.idleForAdministration());
  r.until(start + 9990);
  TEST_ASSERT_FALSE(r.behaviour.idleForAdministration());
  r.until(start + 10000);
  TEST_ASSERT_TRUE(r.behaviour.idleForAdministration());
  TEST_ASSERT_EQUAL_UINT(grouped ? 0 : 1, r.link.sent.size());
}
static void test_individual_endpoint_remains_unsettled_after_visible_lift_arrives() { check_endpoint_settling(false); }
static void test_group_endpoint_remains_unsettled_after_visible_lift_arrives() { check_endpoint_settling(true); }
static void test_individual_same_direction_retarget_during_takeup_preserves_original_run() {
  Rig r;
  const auto start = r.individual(75);
  r.until(start + 700);
  TEST_ASSERT_TRUE(r.behaviour.moveToTarget(50, -1, r.now));
  TEST_ASSERT_EQUAL_UINT(1, r.link.sent.size());
  r.until(start + 3990);
  TEST_ASSERT_EQUAL_UINT(1, r.link.sent.size());
  r.until(start + 4000);
  TEST_ASSERT_EQUAL_UINT(2, r.link.sent.size());
  ASSERT_CMD(somfy_commands::My, r.link.sent.back().command);
}
static void test_group_same_direction_retarget_during_takeup_preserves_original_run() {
  Rig r;
  const auto start = r.group(75);
  r.until(start + 700);
  TEST_ASSERT_TRUE(r.behaviour.moveToGroupTarget(50, r.now));
  TEST_ASSERT_EQUAL_UINT(0, r.link.sent.size());
  r.until(start + 3990);
  TEST_ASSERT_EQUAL_UINT(0, r.link.sent.size());
  r.until(start + 4000);
  TEST_ASSERT_EQUAL_UINT(1, r.link.sent.size());
  ASSERT_CMD(somfy_commands::My, r.link.sent.back().command);
}
static void test_group_reversal_uses_shorter_reversal_takeup_after_actual_stop_outcome() {
  Rig r;
  const auto start = r.group(100);
  r.until(start + 2500);
  ASSERT_POS(25, r.state().currentPos);
  TEST_ASSERT_TRUE(r.behaviour.moveToGroupTarget(0, r.now));
  TEST_ASSERT_EQUAL_UINT(1, r.link.sent.size());
  ASSERT_CMD(somfy_commands::My, r.link.sent.back().command);
  const auto stop = r.link.sent.back().ticket;
  r.completePress(stop);
  TEST_ASSERT_EQUAL_UINT(2, r.link.sent.size());
  ASSERT_CMD(somfy_commands::Up, r.link.sent.back().command);
  const auto reversedAt = r.now;
  r.completePress(r.link.sent.back().ticket);
  r.until(reversedAt + 400);
  ASSERT_POS(25, r.state().currentPos);
  r.until(reversedAt + 500);
  ASSERT_POS(25, r.state().currentPos);
  r.until(reversedAt + 1100);
  ASSERT_POS(20, r.state().currentPos);
}
static void test_direct_nonlinear_profile_has_no_segmented_takeup() {
  Rig r;
  r.settings.calibration.kind = LiftKind::direct;
  r.settings.calibration.down = {0, 3000, 0, 0};
  r.settings.calibration.up = {};
  const auto start = r.individual(50);
  r.until(start + 3000);
  ASSERT_POS(50, r.state().currentPos);
  TEST_ASSERT_EQUAL_UINT(2, r.link.sent.size());
  ASSERT_CMD(somfy_commands::My, r.link.sent.back().command);
}

static void test_ordinary_stop_during_settling_does_not_leave_a_phantom_run() {
  Rig r;
  TEST_ASSERT_TRUE(r.behaviour.sendCommand(somfy_commands::Down, 1, 0, r.now));
  const auto start = r.now; r.completePress(r.link.sent.back().ticket);
  r.until(start + 9500);
  TEST_ASSERT_FALSE(r.behaviour.isIdle());
  TEST_ASSERT_TRUE(r.behaviour.sendCommand(somfy_commands::My, 1, 0, r.now));
  r.completePress(r.link.sent.back().ticket);
  TEST_ASSERT_TRUE(r.behaviour.isIdle());
  const auto count = r.link.sent.size();
  TEST_ASSERT_FALSE(r.behaviour.moveToTarget(50, -1, r.now));
  TEST_ASSERT_EQUAL_UINT(count, r.link.sent.size());
  r.behaviour.confirmEndpoint(true);
  TEST_ASSERT_TRUE(r.behaviour.moveToTarget(50, -1, r.now));
  TEST_ASSERT_EQUAL_UINT(count + 1, r.link.sent.size());
  ASSERT_CMD(somfy_commands::Up, r.link.sent.back().command);
}
static void test_ordinary_reversal_keeps_old_motion_until_stop_and_new_start_outcomes() {
  Rig r;
  TEST_ASSERT_TRUE(r.behaviour.sendCommand(somfy_commands::Down, 1, 0, r.now));
  const auto start = r.now; r.completePress(r.link.sent.back().ticket);
  r.until(start + 2500);
  TEST_ASSERT_TRUE(r.behaviour.sendCommand(somfy_commands::Up, 1, 0, r.now));
  ASSERT_CMD(somfy_commands::My, r.link.sent.back().command);
  const auto stop = r.link.sent.back().ticket;
  r.run(300);
  ASSERT_POS(30, r.state().currentPos);
  r.completePress(stop);
  ASSERT_CMD(somfy_commands::Up, r.link.sent.back().command);
  const auto stopped = r.state().currentPos;
  r.run(700);
  ASSERT_POS(stopped, r.state().currentPos);
}
static void test_repeating_endpoint_during_settling_preserves_the_unfinished_run() {
  for(bool grouped : {false, true}) {
    Rig r;
    const auto start = grouped ? r.group(100) : r.individual(100);
    r.until(start + 9500);
    const auto count = r.link.sent.size();
    TEST_ASSERT_TRUE(grouped ? r.behaviour.moveToGroupTarget(100,r.now) : r.behaviour.moveToTarget(100,-1,r.now));
    TEST_ASSERT_EQUAL_UINT(count, r.link.sent.size());
    r.until(start + 10000);
    TEST_ASSERT_TRUE(r.behaviour.idleForAdministration());
  }
}

static void test_calibrated_command_preserves_explicit_repeat_count_and_start_receipt() {
  Rig r; uint32_t ticket = 0;
  TEST_ASSERT_TRUE(r.behaviour.sendCommand(somfy_commands::Down, 7, 0, r.now, &ticket));
  TEST_ASSERT_EQUAL_UINT32(r.link.sent.back().ticket, ticket);
  TEST_ASSERT_EQUAL_UINT8(7, r.link.sent.back().repeats);
  TEST_ASSERT_EQUAL_UINT8(8, r.frame.repeats); // Initial frame plus requested repetitions.
}
static void test_completed_idle_endpoint_refresh_can_seek_before_next_tick() {
  Rig r;
  TEST_ASSERT_TRUE(r.behaviour.moveToGroupTarget(0, r.now));
  r.completePress(r.link.sent.back().ticket);
  TEST_ASSERT_TRUE(r.behaviour.isIdle());
  TEST_ASSERT_TRUE(r.behaviour.moveToTarget(50, -1, r.now));
  ASSERT_CMD(somfy_commands::Down, r.link.sent.back().command);
}

static void test_operator_endpoint_anchor_is_separate_from_estimated_positions() {
  Rig r;
  r.behaviour.confirmEndpoint(true);
  TEST_ASSERT_TRUE(r.state().endpointConfirmed);
  ASSERT_POS(100, r.state().currentPos);
  TEST_ASSERT_TRUE(r.behaviour.idleForAdministration());
  TEST_ASSERT_TRUE(r.behaviour.moveToTarget(50,-1,r.now));
  TEST_ASSERT_FALSE(r.state().endpointConfirmed);
  r.behaviour.confirmEndpoint(false);
  TEST_ASSERT_TRUE(r.state().endpointConfirmed);
  r.behaviour.setPosition(0,-1);
  TEST_ASSERT_FALSE(r.state().endpointConfirmed);
  r.behaviour.confirmEndpoint(true);
  r.behaviour.restore(100,0,-1,-1);
  TEST_ASSERT_FALSE(r.state().endpointConfirmed);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_operator_endpoint_anchor_is_separate_from_estimated_positions);
  RUN_TEST(test_calibrated_command_preserves_explicit_repeat_count_and_start_receipt);
  RUN_TEST(test_completed_idle_endpoint_refresh_can_seek_before_next_tick);
  RUN_TEST(test_ordinary_stop_during_settling_does_not_leave_a_phantom_run);
  RUN_TEST(test_ordinary_reversal_keeps_old_motion_until_stop_and_new_start_outcomes);
  RUN_TEST(test_repeating_endpoint_during_settling_preserves_the_unfinished_run);
  RUN_TEST(test_individual_position_waits_for_on_air_then_saved_endpoint_takeup);
  RUN_TEST(test_individual_intermediate_stop_uses_nonlinear_visible_deadline);
  RUN_TEST(test_group_member_intermediate_stop_uses_same_saved_profile);
  RUN_TEST(test_individual_and_group_estimates_share_mechanical_and_nonlinear_phases);
  RUN_TEST(test_individual_endpoint_remains_unsettled_after_visible_lift_arrives);
  RUN_TEST(test_group_endpoint_remains_unsettled_after_visible_lift_arrives);
  RUN_TEST(test_individual_same_direction_retarget_during_takeup_preserves_original_run);
  RUN_TEST(test_group_same_direction_retarget_during_takeup_preserves_original_run);
  RUN_TEST(test_group_reversal_uses_shorter_reversal_takeup_after_actual_stop_outcome);
  RUN_TEST(test_direct_nonlinear_profile_has_no_segmented_takeup);
  return UNITY_END();
}
