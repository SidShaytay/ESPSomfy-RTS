#include <unity.h>
#include <cmath>
#include <cstdint>
#include <limits>
#include "LiftCalibration.h"
#include "ShadeMotion.h"

#define ASSERT_POS(expected, actual) TEST_ASSERT_FLOAT_WITHIN(0.01f, (expected), (actual))

void setUp() {}
void tearDown() {}

static LiftCalibration segmented() {
  LiftCalibration calibration;
  calibration.kind = LiftKind::segmented;
  // Directional durations include leading take-up and final settling.
  // Down: total 10s, visible 8s, midpoint at 3s of visible travel.
  calibration.down = {1000, 3000, 1000, 200};
  // Up: total 20s, visible 16s, midpoint at 10s of visible travel.
  calibration.up = {2000, 10000, 2000, 500};
  return calibration;
}

static void test_legacy_configuration_preserves_zero_time_and_requires_empty_new_metadata() {
  LiftCalibration calibration;
  TEST_ASSERT_TRUE(validLiftCalibration(calibration, 0, 0));
  TEST_ASSERT_TRUE(validLiftCalibration(calibration, 10000, 20000));
  calibration.down.takeupMs = 1;
  TEST_ASSERT_FALSE(validLiftCalibration(calibration, 10000, 20000));
  calibration.down = {};
  calibration.up.reversalMs = 1;
  TEST_ASSERT_FALSE(validLiftCalibration(calibration, 10000, 20000));
}
static void test_direct_requires_bounded_positive_times_and_permits_curve_without_slack() {
  LiftCalibration calibration;
  calibration.kind = LiftKind::direct;
  TEST_ASSERT_TRUE(validLiftCalibration(calibration, 1, 600000));
  TEST_ASSERT_FALSE(validLiftCalibration(calibration, 0, 10000));
  TEST_ASSERT_FALSE(validLiftCalibration(calibration, 10000, 0));
  TEST_ASSERT_FALSE(validLiftCalibration(calibration, 600001, 10000));
  TEST_ASSERT_FALSE(validLiftCalibration(calibration, 10000, 600001));
  for(const unsigned field : {0u, 2u, 3u}) {
    calibration.down = {};
    if(field == 0) calibration.down.takeupMs = 1;
    if(field == 2) calibration.down.settlingMs = 1;
    if(field == 3) calibration.down.reversalMs = 1;
    TEST_ASSERT_FALSE(validLiftCalibration(calibration, 10000, 10000));
  }
  calibration.down = {0, 3000, 0, 0};
  TEST_ASSERT_TRUE(validLiftCalibration(calibration, 10000, 10000));
  calibration.down.midpointMs = 10000;
  TEST_ASSERT_FALSE(validLiftCalibration(calibration, 10000, 10000));
}
static void test_segmented_profile_validation_rejects_no_visible_interval_and_overflow() {
  auto calibration = segmented();
  TEST_ASSERT_TRUE(validLiftCalibration(calibration, 20000, 10000));
  calibration.down.midpointMs = 0;
  TEST_ASSERT_TRUE(validLiftCalibration(calibration, 20000, 10000));
  calibration.down = {5000, 0, 5000, 0};
  TEST_ASSERT_FALSE(validLiftCalibration(calibration, 20000, 10000));
  calibration.down = {9000, 0, 2000, 0};
  TEST_ASSERT_FALSE(validLiftCalibration(calibration, 20000, 10000));
  calibration.down = {std::numeric_limits<uint32_t>::max(), 0, 2, 0};
  TEST_ASSERT_FALSE(validLiftCalibration(calibration, 20000, 10000));
  calibration.down = {1000, 8000, 1000, 200};
  TEST_ASSERT_FALSE(validLiftCalibration(calibration, 20000, 10000));
  calibration.down = {1000, 8001, 1000, 200};
  TEST_ASSERT_FALSE(validLiftCalibration(calibration, 20000, 10000));
  calibration.down = {1000, 3000, 1000, 600001};
  TEST_ASSERT_FALSE(validLiftCalibration(calibration, 20000, 10000));
  calibration = segmented();
  calibration.kind = static_cast<LiftKind>(255);
  TEST_ASSERT_FALSE(validLiftCalibration(calibration, 20000, 10000));
}
static void test_run_rejects_invalid_direct_slack_and_invalid_position_inputs() {
  LiftCalibration calibration;
  calibration.kind = LiftKind::direct;
  calibration.down.takeupMs = 1;
  TEST_ASSERT_FALSE(LiftRun(calibration, 10000, 0, 1, LiftStart::endpoint).valid());
  calibration.down = {};
  TEST_ASSERT_FALSE(LiftRun(calibration, 0, 0, 1, LiftStart::endpoint).valid());
  TEST_ASSERT_FALSE(LiftRun(calibration, 10000, -1, 1, LiftStart::endpoint).valid());
  TEST_ASSERT_FALSE(LiftRun(calibration, 10000, 101, 1, LiftStart::endpoint).valid());
  TEST_ASSERT_FALSE(LiftRun(calibration, 10000, std::numeric_limits<float>::quiet_NaN(), 1, LiftStart::endpoint).valid());
  TEST_ASSERT_FALSE(LiftRun(calibration, 10000, 0, 0, LiftStart::endpoint).valid());
}
static void test_legacy_up_deadline_matches_the_existing_quantized_clock() {
  LiftCalibration calibration;
  for(const float start : {33.3f, 40.014f}) {
    const LiftRun run(calibration, 9999, start, -1, LiftStart::interior);
    const uint32_t endpoint = uint32_t(std::floor((start / 100) * 9999));
    TEST_ASSERT_EQUAL_UINT32(endpoint, run.msTo(0));
    TEST_ASSERT_EQUAL_UINT32(endpoint, run.fullCompletionMs());
    ASSERT_POS(0, run.position(endpoint));
  }
}
static void test_legacy_position_math_matches_current_behavior_exactly() {
  LiftCalibration calibration;
  for(const uint32_t total : {0u, 1u, 9999u, 10000u, 600000u}) {
    for(const float start : {0.0f, 12.345f, 33.3f, 50.5f, 99.99f, 100.0f}) {
      for(const int8_t direction : {-1, 0, 1}) {
        for(const auto startKind : {LiftStart::endpoint, LiftStart::interior, LiftStart::reversal}) {
          const LiftRun run(calibration, total, start, direction, startKind);
          for(const uint32_t elapsed : {0u, 1u, 137u, 5000u, 30000u, 600001u})
            TEST_ASSERT_EQUAL_FLOAT(travelPosition(start, direction, elapsed, total), run.position(elapsed));
        }
      }
    }
  }
}
static void test_direct_lift_has_no_initial_or_final_flat_phase() {
  LiftCalibration calibration;
  calibration.kind = LiftKind::direct;
  const LiftRun down(calibration, 10000, 0, 1, LiftStart::endpoint);
  ASSERT_POS(1, down.position(100));
  ASSERT_POS(50, down.position(5000));
  ASSERT_POS(99, down.position(9900));
  ASSERT_POS(100, down.position(10000));
  TEST_ASSERT_EQUAL_UINT32(5000, down.msTo(50));
  TEST_ASSERT_EQUAL_UINT32(10000, down.msTo(100));
  TEST_ASSERT_EQUAL_UINT32(10000, down.fullCompletionMs());
  const LiftRun up(calibration, 20000, 100, -1, LiftStart::reversal);
  ASSERT_POS(99.5f, up.position(100));
  TEST_ASSERT_EQUAL_UINT32(10000, up.msTo(50));
}
static void test_direct_can_fit_a_nonlinear_visible_curve_without_mechanical_slack() {
  LiftCalibration calibration;
  calibration.kind = LiftKind::direct;
  calibration.down.midpointMs = 3000;
  const LiftRun run(calibration, 10000, 0, 1, LiftStart::endpoint);
  ASSERT_POS(25, run.position(1500));
  ASSERT_POS(50, run.position(3000));
  ASSERT_POS(75, run.position(6500));
  TEST_ASSERT_EQUAL_UINT32(3000, run.msTo(50));
  TEST_ASSERT_EQUAL_UINT32(10000, run.fullCompletionMs());
}
static void test_segmented_endpoint_run_has_leading_and_trailing_flat_phases() {
  const auto calibration = segmented();
  const LiftRun down(calibration, 10000, 0, 1, LiftStart::endpoint);
  ASSERT_POS(0, down.position(0));
  ASSERT_POS(0, down.position(999));
  ASSERT_POS(0, down.position(1000));
  ASSERT_POS(25, down.position(2500));
  ASSERT_POS(50, down.position(4000));
  ASSERT_POS(75, down.position(6500));
  ASSERT_POS(100, down.position(9000));
  ASSERT_POS(100, down.position(9500));
  ASSERT_POS(100, down.position(10000));
  ASSERT_POS(100, down.position(20000));
  // Visible arrival and completion differ: do not declare a physical endpoint
  // settled just because the visible lift coordinate has reached 100.
  TEST_ASSERT_EQUAL_UINT32(4000, down.msTo(50));
  TEST_ASSERT_EQUAL_UINT32(10000, down.msTo(100));
  TEST_ASSERT_EQUAL_UINT32(10000, down.fullCompletionMs());
}
static void test_direction_uses_its_own_profile_and_midpoint_means_progress_from_that_origin() {
  const auto calibration = segmented();
  const LiftRun up(calibration, 20000, 100, -1, LiftStart::endpoint);
  ASSERT_POS(100, up.position(2000));
  ASSERT_POS(75, up.position(7000));
  ASSERT_POS(50, up.position(12000));
  ASSERT_POS(25, up.position(15000));
  ASSERT_POS(0, up.position(18000));
  ASSERT_POS(0, up.position(20000));
  TEST_ASSERT_EQUAL_UINT32(12000, up.msTo(50));
  TEST_ASSERT_EQUAL_UINT32(20000, up.fullCompletionMs());
}
static void test_zero_midpoint_selects_linear_visible_travel() {
  auto calibration = segmented();
  calibration.down.midpointMs = 0;
  const LiftRun run(calibration, 10000, 0, 1, LiftStart::endpoint);
  ASSERT_POS(25, run.position(3000));
  ASSERT_POS(50, run.position(5000));
  ASSERT_POS(75, run.position(7000));
  TEST_ASSERT_EQUAL_UINT32(5000, run.msTo(50));
}
static void test_interior_resume_does_not_charge_full_endpoint_takeup() {
  const auto calibration = segmented();
  const LiftRun down(calibration, 10000, 25, 1, LiftStart::interior);
  ASSERT_POS(25, down.position(0));
  ASSERT_POS(50, down.position(1500));
  ASSERT_POS(75, down.position(4000));
  ASSERT_POS(100, down.position(6500));
  TEST_ASSERT_EQUAL_UINT32(1500, down.msTo(50));
  TEST_ASSERT_EQUAL_UINT32(7500, down.fullCompletionMs());
  TEST_ASSERT_EQUAL_UINT32(7500, down.msTo(100));
}
static void test_reversal_uses_reversal_takeup_instead_of_full_endpoint_takeup() {
  const auto calibration = segmented();
  const LiftRun down(calibration, 10000, 25, 1, LiftStart::reversal);
  ASSERT_POS(25, down.position(0));
  ASSERT_POS(25, down.position(200));
  ASSERT_POS(50, down.position(1700));
  TEST_ASSERT_EQUAL_UINT32(1700, down.msTo(50));
  TEST_ASSERT_EQUAL_UINT32(7700, down.fullCompletionMs());
  const LiftRun up(calibration, 20000, 75, -1, LiftStart::reversal);
  ASSERT_POS(75, up.position(500));
  ASSERT_POS(50, up.position(5500));
  TEST_ASSERT_EQUAL_UINT32(5500, up.msTo(50));
  TEST_ASSERT_EQUAL_UINT32(13500, up.fullCompletionMs());
}
static void test_same_direction_retarget_keeps_original_clock_and_pending_takeup() {
  const auto calibration = segmented();
  const LiftRun run(calibration, 10000, 0, 1, LiftStart::endpoint);
  // During the flat first second the visible coordinate cannot say how much
  // take-up has already elapsed. Reuse this run when a slider changes target.
  ASSERT_POS(0, run.position(700));
  TEST_ASSERT_EQUAL_UINT32(1800, run.msTo(25) - 700);
  TEST_ASSERT_EQUAL_UINT32(3300, run.msTo(50) - 700);
  ASSERT_POS(25, run.position(2500));
  // The later target changes only the remaining deadline, not the start.
  TEST_ASSERT_EQUAL_UINT32(1500, run.msTo(50) - 2500);
  TEST_ASSERT_EQUAL_UINT32(4000, run.msTo(75) - 2500);
  ASSERT_POS(50, run.position(4000));
}
static void test_position_and_target_time_are_monotonic_and_inverse_between_flat_phases() {
  const auto calibration = segmented();
  for(const int8_t direction : {-1, 1}) {
    const auto total = direction > 0 ? 10000u : 20000u;
    const LiftRun run(calibration, total, direction > 0 ? 0 : 100, direction, LiftStart::endpoint);
    float previous = run.position(0);
    for(uint32_t elapsed = 1; elapsed <= total + 1000; elapsed += 31) {
      const float current = run.position(elapsed);
      TEST_ASSERT_TRUE(current >= 0 && current <= 100);
      TEST_ASSERT_TRUE(direction > 0 ? current >= previous : current <= previous);
      previous = current;
    }
    uint32_t previousTime = 0;
    for(unsigned progress = 1; progress < 100; ++progress) {
      const float target = direction > 0 ? progress : 100 - progress;
      const auto time = run.msTo(target);
      TEST_ASSERT_TRUE(time >= previousTime);
      ASSERT_POS(target, run.position(time));
      previousTime = time;
    }
  }
}
static void test_already_at_target_or_target_behind_run_needs_no_extra_time() {
  const auto calibration = segmented();
  const LiftRun run(calibration, 10000, 25, 1, LiftStart::interior);
  TEST_ASSERT_EQUAL_UINT32(0, run.msTo(25));
  TEST_ASSERT_EQUAL_UINT32(0, run.msTo(10));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_legacy_configuration_preserves_zero_time_and_requires_empty_new_metadata);
  RUN_TEST(test_direct_requires_bounded_positive_times_and_permits_curve_without_slack);
  RUN_TEST(test_segmented_profile_validation_rejects_no_visible_interval_and_overflow);
  RUN_TEST(test_run_rejects_invalid_direct_slack_and_invalid_position_inputs);
  RUN_TEST(test_legacy_up_deadline_matches_the_existing_quantized_clock);
  RUN_TEST(test_legacy_position_math_matches_current_behavior_exactly);
  RUN_TEST(test_direct_lift_has_no_initial_or_final_flat_phase);
  RUN_TEST(test_direct_can_fit_a_nonlinear_visible_curve_without_mechanical_slack);
  RUN_TEST(test_segmented_endpoint_run_has_leading_and_trailing_flat_phases);
  RUN_TEST(test_direction_uses_its_own_profile_and_midpoint_means_progress_from_that_origin);
  RUN_TEST(test_zero_midpoint_selects_linear_visible_travel);
  RUN_TEST(test_interior_resume_does_not_charge_full_endpoint_takeup);
  RUN_TEST(test_reversal_uses_reversal_takeup_instead_of_full_endpoint_takeup);
  RUN_TEST(test_same_direction_retarget_keeps_original_clock_and_pending_takeup);
  RUN_TEST(test_position_and_target_time_are_monotonic_and_inverse_between_flat_phases);
  RUN_TEST(test_already_at_target_or_target_behind_run_needs_no_extra_time);
  return UNITY_END();
}
