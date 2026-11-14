#include <unity.h>
#include "LiftGuide.h"

#define ASSERT_STAGE(expected, guide) TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), static_cast<uint8_t>((guide).stage()))
#define ASSERT_EFFECT(expected, result) TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), static_cast<uint8_t>((result).effect))

static_assert(static_cast<unsigned>(LiftGuideAction::cancel) == 10, "Existing wire actions stay numbered");
static_assert(static_cast<unsigned>(LiftGuideAction::measureReversals) == 11, "Reversal actions append after Cancel");
static_assert(static_cast<unsigned>(LiftGuideAction::startReverseDown) == 15, "Reversal actions stay ordered");

void setUp() {}
void tearDown() {}

// Synthetic operator observations specify the draft state machine. They are
// not evidence that physical motors have moved or reached their endpoints.
static void accept(LiftGuide &guide, LiftGuideAction action, uint32_t now,
    LiftGuideEffect effect = LiftGuideEffect::none, bool human = true) {
  const auto result = guide.act(action, now, human);
  TEST_ASSERT_TRUE(result.accepted);
  ASSERT_EFFECT(effect, result);
}
static void reject(LiftGuide &guide, LiftGuideAction action, uint32_t now, bool human = true) {
  const auto result = guide.act(action, now, human);
  TEST_ASSERT_FALSE(result.accepted);
  ASSERT_EFFECT(LiftGuideEffect::none, result);
}
static void review(LiftGuide &guide, LiftKind kind = LiftKind::segmented, uint32_t base = 0) {
  TEST_ASSERT_TRUE(guide.begin(kind, base + 100));
  accept(guide, LiftGuideAction::confirmOpen, base + 200, LiftGuideEffect::anchorOpen);
  accept(guide, LiftGuideAction::startDown, base + 300, LiftGuideEffect::driveDown);
  TEST_ASSERT_TRUE(guide.queued(1));
  guide.txEvent(tx_event_t::started, 1, base + 1000);
  guide.txEvent(tx_event_t::finished, 1, base + 1274);
  if(kind == LiftKind::segmented) accept(guide, LiftGuideAction::moving, base + 2000);
  accept(guide, LiftGuideAction::halfway, base + 5000);
  if(kind == LiftKind::segmented) accept(guide, LiftGuideAction::visibleEnd, base + 10000);
  accept(guide, LiftGuideAction::confirmEndpoint, base + 11000, LiftGuideEffect::anchorClosed);
  accept(guide, LiftGuideAction::startUp, base + 12000, LiftGuideEffect::driveUp);
  TEST_ASSERT_TRUE(guide.queued(2));
  guide.txEvent(tx_event_t::started, 2, base + 20000);
  guide.txEvent(tx_event_t::finished, 2, base + 20274);
  if(kind == LiftKind::segmented) accept(guide, LiftGuideAction::moving, base + 22000);
  accept(guide, LiftGuideAction::halfway, base + 32000);
  if(kind == LiftKind::segmented) accept(guide, LiftGuideAction::visibleEnd, base + 38000);
  accept(guide, LiftGuideAction::confirmEndpoint, base + 40000, LiftGuideEffect::anchorOpen);
  ASSERT_STAGE(LiftGuideStage::review, guide);
}
static void positionAtHalf(LiftGuide &guide, uint32_t base = 0) {
  review(guide, LiftKind::segmented, base);
  accept(guide, LiftGuideAction::measureReversals, base + 41000, LiftGuideEffect::driveDown);
  ASSERT_STAGE(LiftGuideStage::reversePosition, guide);
  TEST_ASSERT_TRUE(guide.queued(3));
  guide.txEvent(tx_event_t::started, 3, base + 50000);
  guide.txEvent(tx_event_t::finished, 3, base + 50274);
  accept(guide, LiftGuideAction::stopAtHalf, base + 55000, LiftGuideEffect::stop);
  ASSERT_STAGE(LiftGuideStage::reversePositionStopping, guide);
  // This callback represents the actual full Stop outcome from the session.
  TEST_ASSERT_TRUE(guide.stopped());
  ASSERT_STAGE(LiftGuideStage::reverseReadyUp, guide);
}

static void test_segmented_save_requires_reversal_knowledge_but_direct_profile_is_known() {
  LiftGuide segmented;
  review(segmented);
  TEST_ASSERT_FALSE(segmented.reversalsKnown());
  reject(segmented, LiftGuideAction::save, 41000);
  ASSERT_STAGE(LiftGuideStage::review, segmented);
  LiftGuide direct;
  TEST_ASSERT_TRUE(direct.begin(LiftKind::direct, 100));
  TEST_ASSERT_TRUE(direct.reversalsKnown());
  LiftGuide completeDirect;
  review(completeDirect, LiftKind::direct);
  TEST_ASSERT_TRUE(completeDirect.reversalsKnown());
  accept(completeDirect, LiftGuideAction::save, 41000, LiftGuideEffect::save);
}
static void test_explicit_no_reversal_assertion_is_human_only_and_emits_no_rf() {
  LiftGuide guide;
  review(guide);
  reject(guide, LiftGuideAction::confirmNoReversal, 41000, false);
  TEST_ASSERT_FALSE(guide.reversalsKnown());
  accept(guide, LiftGuideAction::confirmNoReversal, 42000);
  ASSERT_STAGE(LiftGuideStage::review, guide);
  TEST_ASSERT_TRUE(guide.reversalsKnown());
  TEST_ASSERT_EQUAL_UINT32(0, guide.calibration().up.reversalMs);
  TEST_ASSERT_EQUAL_UINT32(0, guide.calibration().down.reversalMs);
  accept(guide, LiftGuideAction::save, 43000, LiftGuideEffect::save);
}
static void test_full_reversal_measurement_uses_each_actual_start_and_final_human_open_anchor() {
  LiftGuide guide;
  positionAtHalf(guide);
  accept(guide, LiftGuideAction::startReverseUp, 56000, LiftGuideEffect::driveUp);
  ASSERT_STAGE(LiftGuideStage::reverseStartingUp, guide);
  TEST_ASSERT_TRUE(guide.queued(4));
  guide.txEvent(tx_event_t::started, 4, 60000);
  ASSERT_STAGE(LiftGuideStage::reverseTakeupUp, guide);
  // Visible movement may be observed before the directional press finishes.
  accept(guide, LiftGuideAction::moving, 60150, LiftGuideEffect::stop);
  ASSERT_STAGE(LiftGuideStage::reverseStoppingUp, guide);
  TEST_ASSERT_EQUAL_UINT32(150, guide.calibration().up.reversalMs);
  TEST_ASSERT_FALSE(guide.stopped());
  guide.txEvent(tx_event_t::finished, 4, 60274);
  TEST_ASSERT_TRUE(guide.stopped());
  ASSERT_STAGE(LiftGuideStage::reverseReadyDown, guide);
  accept(guide, LiftGuideAction::startReverseDown, 61000, LiftGuideEffect::driveDown);
  ASSERT_STAGE(LiftGuideStage::reverseStartingDown, guide);
  TEST_ASSERT_TRUE(guide.queued(5));
  guide.txEvent(tx_event_t::started, 5, 65000);
  guide.txEvent(tx_event_t::finished, 5, 65274);
  accept(guide, LiftGuideAction::moving, 65800, LiftGuideEffect::stop);
  TEST_ASSERT_EQUAL_UINT32(800, guide.calibration().down.reversalMs);
  ASSERT_STAGE(LiftGuideStage::reverseStoppingDown, guide);
  TEST_ASSERT_TRUE(guide.stopped());
  ASSERT_STAGE(LiftGuideStage::reverseFinish, guide);
  TEST_ASSERT_FALSE(guide.reversalsKnown());
  reject(guide, LiftGuideAction::save, 66000);
  accept(guide, LiftGuideAction::open, 67000, LiftGuideEffect::driveUp);
  TEST_ASSERT_TRUE(guide.queued(6));
  guide.txEvent(tx_event_t::started, 6, 68000);
  reject(guide, LiftGuideAction::confirmOpen, 68100);
  guide.txEvent(tx_event_t::finished, 6, 68274);
  reject(guide, LiftGuideAction::confirmOpen, 70000, false);
  TEST_ASSERT_FALSE(guide.reversalsKnown());
  accept(guide, LiftGuideAction::confirmOpen, 71000, LiftGuideEffect::anchorOpen);
  ASSERT_STAGE(LiftGuideStage::review, guide);
  TEST_ASSERT_TRUE(guide.reversalsKnown());
  TEST_ASSERT_EQUAL_UINT32(10000, guide.downTime());
  TEST_ASSERT_EQUAL_UINT32(20000, guide.upTime());
  TEST_ASSERT_EQUAL_UINT32(1000, guide.calibration().down.takeupMs);
  TEST_ASSERT_EQUAL_UINT32(2000, guide.calibration().up.takeupMs);
  accept(guide, LiftGuideAction::save, 72000, LiftGuideEffect::save);
}
static void test_human_markers_and_whole_press_outcomes_cannot_be_replaced_by_stale_rf() {
  LiftGuide guide;
  review(guide);
  accept(guide, LiftGuideAction::measureReversals, 41000, LiftGuideEffect::driveDown);
  TEST_ASSERT_TRUE(guide.queued(3));
  TEST_ASSERT_FALSE(guide.stopped());
  reject(guide, LiftGuideAction::stopAtHalf, 42000);
  guide.txEvent(tx_event_t::started, 99, 49000);
  reject(guide, LiftGuideAction::stopAtHalf, 49500);
  guide.txEvent(tx_event_t::started, 3, 50000);
  guide.txEvent(tx_event_t::finished, 3, 50274);
  reject(guide, LiftGuideAction::stopAtHalf, 55000, false);
  ASSERT_STAGE(LiftGuideStage::reversePosition, guide);
  accept(guide, LiftGuideAction::stopAtHalf, 55000, LiftGuideEffect::stop);
  TEST_ASSERT_TRUE(guide.stopped());
  accept(guide, LiftGuideAction::startReverseUp, 56000, LiftGuideEffect::driveUp);
  TEST_ASSERT_TRUE(guide.queued(4));
  guide.txEvent(tx_event_t::started, 4, 60000);
  reject(guide, LiftGuideAction::moving, 60100, false);
  ASSERT_STAGE(LiftGuideStage::reverseTakeupUp, guide);
  accept(guide, LiftGuideAction::moving, 60150, LiftGuideEffect::stop);
  guide.txEvent(tx_event_t::finished, 99, 60274);
  TEST_ASSERT_FALSE(guide.stopped());
  ASSERT_STAGE(LiftGuideStage::reverseStoppingUp, guide);
  guide.txEvent(tx_event_t::finished, 4, 60274);
  TEST_ASSERT_TRUE(guide.stopped());
  TEST_ASSERT_FALSE(guide.stopped()); // No second Stop acknowledgement can advance another stage.
  ASSERT_STAGE(LiftGuideStage::reverseReadyDown, guide);
}
static void test_refused_dropped_or_aborted_reversal_press_never_enables_save() {
  for(unsigned failure = 0; failure < 3; ++failure) {
    LiftGuide guide;
    positionAtHalf(guide);
    accept(guide, LiftGuideAction::startReverseUp, 56000, LiftGuideEffect::driveUp);
    if(failure == 0) guide.refused();
    else {
      TEST_ASSERT_TRUE(guide.queued(4));
      if(failure == 2) guide.txEvent(tx_event_t::started, 4, 60000);
      guide.txEvent(failure == 1 ? tx_event_t::dropped : tx_event_t::aborted, 4, 60100);
    }
    ASSERT_STAGE(LiftGuideStage::failed, guide);
    TEST_ASSERT_FALSE(guide.reversalsKnown());
    reject(guide, LiftGuideAction::save, 62000);
    TEST_ASSERT_FALSE(guide.stopped());
  }
}
static void test_zero_reversal_observation_and_uint32_wrap_use_real_start_origin() {
  LiftGuide guide;
  const uint32_t start = 0xfffffff0u;
  positionAtHalf(guide, start - 60000u);
  accept(guide, LiftGuideAction::startReverseUp, start - 100, LiftGuideEffect::driveUp);
  TEST_ASSERT_TRUE(guide.queued(4));
  guide.txEvent(tx_event_t::started, 4, start);
  // Zero is an explicit observed no-delay reversal, not an unmeasured default.
  accept(guide, LiftGuideAction::moving, start, LiftGuideEffect::stop);
  TEST_ASSERT_EQUAL_UINT32(0, guide.calibration().up.reversalMs);
  TEST_ASSERT_FALSE(guide.stopped());
  guide.txEvent(tx_event_t::finished, 4, start + 274u);
  TEST_ASSERT_TRUE(guide.stopped());
  accept(guide, LiftGuideAction::startReverseDown, start + 500u, LiftGuideEffect::driveDown);
  TEST_ASSERT_TRUE(guide.queued(5));
  guide.txEvent(tx_event_t::started, 5, start + 1000u);
  guide.txEvent(tx_event_t::finished, 5, start + 1274u);
  accept(guide, LiftGuideAction::moving, start + 1450u, LiftGuideEffect::stop);
  TEST_ASSERT_EQUAL_UINT32(450, guide.calibration().down.reversalMs);
  TEST_ASSERT_TRUE(guide.stopped());
  ASSERT_STAGE(LiftGuideStage::reverseFinish, guide);
  TEST_ASSERT_FALSE(guide.reversalsKnown());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_segmented_save_requires_reversal_knowledge_but_direct_profile_is_known);
  RUN_TEST(test_explicit_no_reversal_assertion_is_human_only_and_emits_no_rf);
  RUN_TEST(test_full_reversal_measurement_uses_each_actual_start_and_final_human_open_anchor);
  RUN_TEST(test_human_markers_and_whole_press_outcomes_cannot_be_replaced_by_stale_rf);
  RUN_TEST(test_refused_dropped_or_aborted_reversal_press_never_enables_save);
  RUN_TEST(test_zero_reversal_observation_and_uint32_wrap_use_real_start_origin);
  return UNITY_END();
}
