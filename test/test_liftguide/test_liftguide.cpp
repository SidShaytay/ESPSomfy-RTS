#include <unity.h>
#include <cstdint>
#include "LiftCalibration.h"
#include "TxBuffer.h"
#include "LiftGuide.h"

#define ASSERT_STAGE(expected, guide) TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), static_cast<uint8_t>((guide).stage()))
#define ASSERT_EFFECT(expected, result) TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), static_cast<uint8_t>((result).effect))

void setUp() {}
void tearDown() {}

// These are synthetic human observations. They test the session contract,
// never claim that a real controller or motor has reached a physical endpoint.
static void accepted(LiftGuide &guide, LiftGuideAction action, uint32_t at,
    LiftGuideEffect effect = LiftGuideEffect::none) {
  const auto result = guide.act(action, at);
  TEST_ASSERT_TRUE(result.accepted);
  ASSERT_EFFECT(effect, result);
}
static void refused(LiftGuide &guide, LiftGuideAction action, uint32_t at,
    bool human = true) {
  const auto result = guide.act(action, at, human);
  TEST_ASSERT_FALSE(result.accepted);
  ASSERT_EFFECT(LiftGuideEffect::none, result);
}
static void ready(LiftGuide &guide, LiftKind kind, uint32_t start = 1000) {
  TEST_ASSERT_TRUE(guide.begin(kind, start - 900));
  ASSERT_STAGE(LiftGuideStage::reference, guide);
  accepted(guide, LiftGuideAction::confirmOpen, start - 800, LiftGuideEffect::anchorOpen);
  ASSERT_STAGE(LiftGuideStage::readyDown, guide);
}
static void startDown(LiftGuide &guide, LiftKind kind, uint32_t start = 1000,
    bool finish = true) {
  ready(guide, kind, start);
  accepted(guide, LiftGuideAction::startDown, start - 700, LiftGuideEffect::driveDown);
  ASSERT_STAGE(LiftGuideStage::startingDown, guide);
  TEST_ASSERT_TRUE(guide.queued(41));
  guide.txEvent(tx_event_t::started, 41, start);
  ASSERT_STAGE(kind == LiftKind::segmented ? LiftGuideStage::downTakeup : LiftGuideStage::downHalf, guide);
  if(finish) guide.txEvent(tx_event_t::finished, 41, start + 274);
}
static void segmentedDown(LiftGuide &guide, uint32_t start = 1000) {
  startDown(guide, LiftKind::segmented, start);
  accepted(guide, LiftGuideAction::moving, start + 1000);
  ASSERT_STAGE(LiftGuideStage::downHalf, guide);
  accepted(guide, LiftGuideAction::halfway, start + 4000);
  ASSERT_STAGE(LiftGuideStage::downEnd, guide);
  accepted(guide, LiftGuideAction::visibleEnd, start + 9000);
  ASSERT_STAGE(LiftGuideStage::downConfirm, guide);
  accepted(guide, LiftGuideAction::confirmEndpoint, start + 10000, LiftGuideEffect::anchorClosed);
  ASSERT_STAGE(LiftGuideStage::readyUp, guide);
}
static void segmentedBoth(LiftGuide &guide) {
  segmentedDown(guide);
  accepted(guide, LiftGuideAction::startUp, 12000, LiftGuideEffect::driveUp);
  ASSERT_STAGE(LiftGuideStage::startingUp, guide);
  TEST_ASSERT_TRUE(guide.queued(42));
  guide.txEvent(tx_event_t::started, 42, 20000);
  ASSERT_STAGE(LiftGuideStage::upTakeup, guide);
  guide.txEvent(tx_event_t::finished, 42, 20274);
  accepted(guide, LiftGuideAction::moving, 22000);
  ASSERT_STAGE(LiftGuideStage::upHalf, guide);
  accepted(guide, LiftGuideAction::halfway, 32000);
  ASSERT_STAGE(LiftGuideStage::upEnd, guide);
  accepted(guide, LiftGuideAction::visibleEnd, 38000);
  ASSERT_STAGE(LiftGuideStage::upConfirm, guide);
  accepted(guide, LiftGuideAction::confirmEndpoint, 40000, LiftGuideEffect::anchorOpen);
  ASSERT_STAGE(LiftGuideStage::review, guide);
}
static void test_only_supported_profile_kinds_begin_a_new_guide() {
  LiftGuide guide;
  ASSERT_STAGE(LiftGuideStage::closed, guide);
  TEST_ASSERT_FALSE(guide.begin(static_cast<LiftKind>(255), 100));
  TEST_ASSERT_FALSE(guide.begin(LiftKind::legacy, 100));
  ASSERT_STAGE(LiftGuideStage::closed, guide);
  TEST_ASSERT_TRUE(guide.begin(LiftKind::direct, 100));
  ASSERT_STAGE(LiftGuideStage::reference, guide);
  // A second begin must not silently replace an active session.
  TEST_ASSERT_FALSE(guide.begin(LiftKind::segmented, 200));
}
static void test_reference_requires_human_confirmation_before_measuring() {
  LiftGuide guide;
  TEST_ASSERT_TRUE(guide.begin(LiftKind::segmented, 100));
  refused(guide, LiftGuideAction::startDown, 200);
  refused(guide, LiftGuideAction::confirmOpen, 200, false);
  ASSERT_STAGE(LiftGuideStage::reference, guide);
  accepted(guide, LiftGuideAction::open, 300, LiftGuideEffect::driveUp);
}
static void test_segmented_observations_capture_visible_midpoint_and_settling_per_direction() {
  LiftGuide guide;
  segmentedBoth(guide);
  const auto &profile = guide.calibration();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LiftKind::segmented), static_cast<uint8_t>(profile.kind));
  TEST_ASSERT_EQUAL_UINT32(10000, guide.downTime());
  TEST_ASSERT_EQUAL_UINT32(20000, guide.upTime());
  TEST_ASSERT_EQUAL_UINT32(1000, profile.down.takeupMs);
  TEST_ASSERT_EQUAL_UINT32(3000, profile.down.midpointMs);
  TEST_ASSERT_EQUAL_UINT32(1000, profile.down.settlingMs);
  TEST_ASSERT_EQUAL_UINT32(2000, profile.up.takeupMs);
  TEST_ASSERT_EQUAL_UINT32(10000, profile.up.midpointMs);
  TEST_ASSERT_EQUAL_UINT32(2000, profile.up.settlingMs);
  TEST_ASSERT_EQUAL_UINT32(0, profile.down.reversalMs);
  TEST_ASSERT_EQUAL_UINT32(0, profile.up.reversalMs);
  TEST_ASSERT_TRUE(validLiftCalibration(profile, guide.upTime(), guide.downTime()));
}
static void test_direct_guide_skips_mechanical_markers_and_retains_nonlinear_midpoints() {
  LiftGuide guide;
  startDown(guide, LiftKind::direct);
  refused(guide, LiftGuideAction::moving, 2000);
  accepted(guide, LiftGuideAction::halfway, 4000);
  ASSERT_STAGE(LiftGuideStage::downConfirm, guide);
  refused(guide, LiftGuideAction::visibleEnd, 10000);
  accepted(guide, LiftGuideAction::confirmEndpoint, 11000, LiftGuideEffect::anchorClosed);
  accepted(guide, LiftGuideAction::startUp, 12000, LiftGuideEffect::driveUp);
  TEST_ASSERT_TRUE(guide.queued(42));
  guide.txEvent(tx_event_t::started, 42, 20000);
  ASSERT_STAGE(LiftGuideStage::upHalf, guide);
  guide.txEvent(tx_event_t::finished, 42, 20274);
  accepted(guide, LiftGuideAction::halfway, 32000);
  ASSERT_STAGE(LiftGuideStage::upConfirm, guide);
  accepted(guide, LiftGuideAction::confirmEndpoint, 40000, LiftGuideEffect::anchorOpen);
  ASSERT_STAGE(LiftGuideStage::review, guide);
  const auto &profile = guide.calibration();
  TEST_ASSERT_EQUAL_UINT32(10000, guide.downTime());
  TEST_ASSERT_EQUAL_UINT32(20000, guide.upTime());
  TEST_ASSERT_EQUAL_UINT32(3000, profile.down.midpointMs);
  TEST_ASSERT_EQUAL_UINT32(12000, profile.up.midpointMs);
  TEST_ASSERT_EQUAL_UINT32(0, profile.down.takeupMs + profile.down.settlingMs + profile.down.reversalMs);
  TEST_ASSERT_EQUAL_UINT32(0, profile.up.takeupMs + profile.up.settlingMs + profile.up.reversalMs);
  TEST_ASSERT_TRUE(validLiftCalibration(profile, guide.upTime(), guide.downTime()));
}
static void test_each_physical_marker_rejects_automation_without_advancing_stage() {
  LiftGuide guide;
  startDown(guide, LiftKind::segmented);
  refused(guide, LiftGuideAction::moving, 2000, false);
  ASSERT_STAGE(LiftGuideStage::downTakeup, guide);
  accepted(guide, LiftGuideAction::moving, 2000);
  refused(guide, LiftGuideAction::halfway, 5000, false);
  ASSERT_STAGE(LiftGuideStage::downHalf, guide);
  accepted(guide, LiftGuideAction::halfway, 5000);
  refused(guide, LiftGuideAction::visibleEnd, 10000, false);
  ASSERT_STAGE(LiftGuideStage::downEnd, guide);
  accepted(guide, LiftGuideAction::visibleEnd, 10000);
  refused(guide, LiftGuideAction::confirmEndpoint, 11000, false);
  ASSERT_STAGE(LiftGuideStage::downConfirm, guide);
  accepted(guide, LiftGuideAction::confirmEndpoint, 11000, LiftGuideEffect::anchorClosed);
}
static void test_measurement_origin_is_actual_tx_start_not_request_or_queue_admission() {
  LiftGuide guide;
  startDown(guide, LiftKind::direct, 5000);
  accepted(guide, LiftGuideAction::halfway, 8000);
  accepted(guide, LiftGuideAction::confirmEndpoint, 15000, LiftGuideEffect::anchorClosed);
  TEST_ASSERT_EQUAL_UINT32(10000, guide.downTime());
  TEST_ASSERT_EQUAL_UINT32(3000, guide.calibration().down.midpointMs);
}
static void test_endpoint_confirmation_waits_for_complete_directional_press() {
  LiftGuide guide;
  startDown(guide, LiftKind::direct, 1000, false);
  accepted(guide, LiftGuideAction::halfway, 1100);
  refused(guide, LiftGuideAction::confirmEndpoint, 1200);
  ASSERT_STAGE(LiftGuideStage::downConfirm, guide);
  guide.txEvent(tx_event_t::finished, 41, 1274);
  accepted(guide, LiftGuideAction::confirmEndpoint, 2000, LiftGuideEffect::anchorClosed);
}
static void test_missing_or_nonmonotonic_markers_do_not_create_a_profile() {
  LiftGuide guide;
  startDown(guide, LiftKind::segmented);
  refused(guide, LiftGuideAction::halfway, 2000);
  refused(guide, LiftGuideAction::visibleEnd, 2000);
  refused(guide, LiftGuideAction::confirmEndpoint, 2000);
  refused(guide, LiftGuideAction::moving, 999);
  accepted(guide, LiftGuideAction::moving, 2000);
  refused(guide, LiftGuideAction::halfway, 2000);
  refused(guide, LiftGuideAction::halfway, 1999);
  accepted(guide, LiftGuideAction::halfway, 5000);
  refused(guide, LiftGuideAction::visibleEnd, 4999);
  accepted(guide, LiftGuideAction::visibleEnd, 10000);
  refused(guide, LiftGuideAction::confirmEndpoint, 9999);
  refused(guide, LiftGuideAction::save, 11000);
}
static void test_marker_timestamps_survive_uint32_clock_wrap() {
  LiftGuide guide;
  const uint32_t start = 0xfffff000u;
  segmentedDown(guide, start);
  TEST_ASSERT_EQUAL_UINT32(10000, guide.downTime());
  TEST_ASSERT_EQUAL_UINT32(1000, guide.calibration().down.takeupMs);
  TEST_ASSERT_EQUAL_UINT32(3000, guide.calibration().down.midpointMs);
  TEST_ASSERT_EQUAL_UINT32(1000, guide.calibration().down.settlingMs);
}
static void test_stroke_longer_than_maximum_is_rejected_before_endpoint_anchor_or_save() {
  LiftGuide guide;
  startDown(guide, LiftKind::direct);
  accepted(guide, LiftGuideAction::halfway, 301000);
  refused(guide, LiftGuideAction::confirmEndpoint, 601001);
  refused(guide, LiftGuideAction::save, 601002);
}
static void test_refused_dropped_and_aborted_starts_fail_without_saving() {
  for(unsigned failure = 0; failure < 3; ++failure) {
    LiftGuide guide;
    ready(guide, LiftKind::segmented);
    accepted(guide, LiftGuideAction::startDown, 300, LiftGuideEffect::driveDown);
    if(failure == 0) guide.refused();
    else {
      TEST_ASSERT_TRUE(guide.queued(41));
      if(failure == 2) guide.txEvent(tx_event_t::started, 41, 1000);
      guide.txEvent(failure == 1 ? tx_event_t::dropped : tx_event_t::aborted, 41, 1100);
    }
    ASSERT_STAGE(LiftGuideStage::failed, guide);
    refused(guide, LiftGuideAction::save, 2000);
    refused(guide, LiftGuideAction::startDown, 2000);
  }
}
static void test_stopping_queued_start_cancels_without_motor_my() {
  LiftGuide guide;
  ready(guide, LiftKind::segmented);
  accepted(guide, LiftGuideAction::startDown, 300, LiftGuideEffect::driveDown);
  TEST_ASSERT_TRUE(guide.queued(41));
  accepted(guide, LiftGuideAction::stop, 400, LiftGuideEffect::cancelStart);
  ASSERT_STAGE(LiftGuideStage::failed, guide);
  guide.txEvent(tx_event_t::started, 41, 1000);
  guide.txEvent(tx_event_t::finished, 41, 1274);
  ASSERT_STAGE(LiftGuideStage::failed, guide);
  refused(guide, LiftGuideAction::save, 2000);
}
static void test_stopping_after_start_emits_stop_and_invalidates_draft() {
  LiftGuide guide;
  startDown(guide, LiftKind::segmented);
  accepted(guide, LiftGuideAction::stop, 2000, LiftGuideEffect::stop);
  ASSERT_STAGE(LiftGuideStage::failed, guide);
  refused(guide, LiftGuideAction::moving, 2100);
  refused(guide, LiftGuideAction::save, 2200);
}
static void test_a_directional_press_requires_one_nonzero_ticket_before_it_can_start() {
  LiftGuide guide;
  ready(guide, LiftKind::segmented);
  accepted(guide, LiftGuideAction::startDown, 300, LiftGuideEffect::driveDown);
  TEST_ASSERT_FALSE(guide.queued(0));
  TEST_ASSERT_TRUE(guide.queued(41));
  TEST_ASSERT_FALSE(guide.queued(42));
  guide.txEvent(tx_event_t::started, 42, 1000);
  ASSERT_STAGE(LiftGuideStage::startingDown, guide);
  guide.txEvent(tx_event_t::started, 41, 1000);
  ASSERT_STAGE(LiftGuideStage::downTakeup, guide);
}
static void test_unrelated_ticket_outcomes_cannot_advance_or_fail_current_measurement() {
  LiftGuide guide;
  startDown(guide, LiftKind::segmented);
  guide.txEvent(tx_event_t::dropped, 99, 1800);
  guide.txEvent(tx_event_t::aborted, 99, 1900);
  ASSERT_STAGE(LiftGuideStage::downTakeup, guide);
  accepted(guide, LiftGuideAction::moving, 2000);
  ASSERT_STAGE(LiftGuideStage::downHalf, guide);
}
static void test_only_review_can_save_and_only_durable_applied_save_completes() {
  LiftGuide guide;
  ready(guide, LiftKind::segmented);
  refused(guide, LiftGuideAction::save, 250);
  // Build a distinct complete draft; readyDown alone never authorizes save.
  LiftGuide complete;
  segmentedBoth(complete);
  accepted(complete, LiftGuideAction::save, 41000, LiftGuideEffect::save);
  ASSERT_STAGE(LiftGuideStage::saving, complete);
  refused(complete, LiftGuideAction::save, 41001);
  refused(complete, LiftGuideAction::startDown, 41002);
  complete.saved(true, true);
  ASSERT_STAGE(LiftGuideStage::complete, complete);
}
static void test_applied_but_undurable_save_requires_attention_without_rf_retry() {
  LiftGuide guide;
  segmentedBoth(guide);
  accepted(guide, LiftGuideAction::save, 41000, LiftGuideEffect::save);
  guide.saved(true, false);
  ASSERT_STAGE(LiftGuideStage::failed, guide);
  refused(guide, LiftGuideAction::save, 42000);
  refused(guide, LiftGuideAction::startDown, 42000);
  refused(guide, LiftGuideAction::startUp, 42000);
}
static void test_save_completion_without_a_saving_operation_cannot_invent_completion() {
  LiftGuide guide;
  TEST_ASSERT_TRUE(guide.begin(LiftKind::direct, 100));
  guide.saved(true, true);
  ASSERT_STAGE(LiftGuideStage::reference, guide);
}
static void test_cancel_after_review_closes_without_saving_or_movement() {
  LiftGuide guide;
  segmentedBoth(guide);
  accepted(guide, LiftGuideAction::cancel, 41000, LiftGuideEffect::close);
  ASSERT_STAGE(LiftGuideStage::closed, guide);
  refused(guide, LiftGuideAction::save, 42000);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_only_supported_profile_kinds_begin_a_new_guide);
  RUN_TEST(test_reference_requires_human_confirmation_before_measuring);
  RUN_TEST(test_segmented_observations_capture_visible_midpoint_and_settling_per_direction);
  RUN_TEST(test_direct_guide_skips_mechanical_markers_and_retains_nonlinear_midpoints);
  RUN_TEST(test_each_physical_marker_rejects_automation_without_advancing_stage);
  RUN_TEST(test_measurement_origin_is_actual_tx_start_not_request_or_queue_admission);
  RUN_TEST(test_endpoint_confirmation_waits_for_complete_directional_press);
  RUN_TEST(test_missing_or_nonmonotonic_markers_do_not_create_a_profile);
  RUN_TEST(test_marker_timestamps_survive_uint32_clock_wrap);
  RUN_TEST(test_stroke_longer_than_maximum_is_rejected_before_endpoint_anchor_or_save);
  RUN_TEST(test_refused_dropped_and_aborted_starts_fail_without_saving);
  RUN_TEST(test_stopping_queued_start_cancels_without_motor_my);
  RUN_TEST(test_stopping_after_start_emits_stop_and_invalidates_draft);
  RUN_TEST(test_a_directional_press_requires_one_nonzero_ticket_before_it_can_start);
  RUN_TEST(test_unrelated_ticket_outcomes_cannot_advance_or_fail_current_measurement);
  RUN_TEST(test_only_review_can_save_and_only_durable_applied_save_completes);
  RUN_TEST(test_applied_but_undurable_save_requires_attention_without_rf_retry);
  RUN_TEST(test_save_completion_without_a_saving_operation_cannot_invent_completion);
  RUN_TEST(test_cancel_after_review_closes_without_saving_or_movement);
  return UNITY_END();
}
