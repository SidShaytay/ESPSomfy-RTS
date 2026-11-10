#include "LocalPrimaryCounter.h"
#include <unity.h>
using namespace LocalUi;
using namespace EntityAdmin;
void setUp() {}
void tearDown() {}
static PrimaryCounterReview review() {
  PrimaryCounterReview value;
  value.address = 16777215;
  value.shadeId = 32;
  value.primary = 1000;
  value.stored = {1000, true};
  value.matching[0] = 1;
  value.codes[0] = 1000;
  return value;
}
static void
explicit_numeric_apply_and_default_cancel_never_correct_on_rotation() {
  PrimaryCounterDraft draft;
  draft.open(review());
  FormView view;
  draft.render(view, 32);
  TEST_ASSERT_EQUAL(0, view.selected);
  TEST_ASSERT_EQUAL_STRING("Cancel", view.rows[0].label);
  TEST_ASSERT_NOT_NULL(strstr(view.rows[2].label, "16777215"));
  TEST_ASSERT_EQUAL(PrimaryCounterEffect::leave, draft.activate(1));
  draft.open(review());
  draft.turn(1, 2);
  TEST_ASSERT_EQUAL(PrimaryCounterEffect::none, draft.activate(3));
  draft.turn(-1, 4);
  draft.render(view, 32);
  TEST_ASSERT_EQUAL_STRING("999", view.rows[0].value);
  TEST_ASSERT_EQUAL(PrimaryCounterEffect::none,
                    draft.activate(5)); // Open editor actions only.
  draft.render(view, 32);
  TEST_ASSERT_EQUAL_STRING("Cancel edit", view.rows[0].label);
  TEST_ASSERT_EQUAL(0, view.selected);
  draft.turn(1, 6);
  TEST_ASSERT_EQUAL(
      PrimaryCounterEffect::none,
      draft.activate(7)); // Explicitly Apply draft, still no correction.
  draft.render(view, 32);
  TEST_ASSERT_EQUAL_STRING("Set code: 999", view.rows[1].label);
  TEST_ASSERT_EQUAL(0, view.selected);
  TEST_ASSERT_EQUAL(999, draft.requested());
  TEST_ASSERT_NOT_NULL(strstr(view.hint, "ALL"));
  TEST_ASSERT_EQUAL(
      PrimaryCounterEffect::none,
      draft.activate(8)); // Default Cancel returns current review.
  TEST_ASSERT_EQUAL(1000, draft.requested());
}
static void coarse_steps_boundaries_and_reconciliation_qualification() {
  PrimaryCounterDraft draft;
  auto original = review();
  original.primary = 65535;
  original.codes[0] = 65535;
  original.stored.code = 65535;
  draft.open(original);
  draft.turn(1, 1);
  draft.activate(2);
  draft.turn(1, 3);
  FormView view;
  draft.render(view, 32);
  TEST_ASSERT_EQUAL_STRING("65535", view.rows[0].value);
  draft.activate(4);
  draft.turn(-1, 5);
  draft.activate(6); // Step action: default Cancel -> last Step.
  draft.turn(-1, 7);
  draft.render(view, 32);
  TEST_ASSERT_EQUAL_STRING("64535", view.rows[0].value);
  for (unsigned i = 0; i < 70; i++)
    draft.turn(-1, 8 + i);
  draft.render(view, 32);
  TEST_ASSERT_EQUAL_STRING("0", view.rows[0].value);
  auto beginAndApply = [&](PrimaryCounterReview current) {
    draft.open(current);
    draft.turn(1, 100);
    draft.activate(101);
    draft.activate(102);
    draft.turn(1, 103);
    return draft.activate(104);
  };
  TEST_ASSERT_EQUAL(PrimaryCounterEffect::checkUnchanged,
                    beginAndApply(review()));
  original = review();
  original.stored.present = false;
  TEST_ASSERT_EQUAL(PrimaryCounterEffect::none, beginAndApply(original));
  draft.render(view, 32);
  TEST_ASSERT_EQUAL_STRING("Set code: 1000", view.rows[1].label);
  original = review();
  original.stored.code = 999;
  TEST_ASSERT_EQUAL(PrimaryCounterEffect::none, beginAndApply(original));
  original = review();
  original.matching[0] = 3;
  original.codes[1] = 999;
  TEST_ASSERT_EQUAL(PrimaryCounterEffect::none, beginAndApply(original));
}
static void
applied_file_failure_has_only_file_retry_and_unknown_nvs_close_only() {
  PrimaryCounterDraft draft;
  draft.open(review());
  draft.complete({CounterDomainState::modelFailed,
                  true,
                  {ModelCommitState::failed, ModelCommitStage::write}});
  FormView view;
  draft.render(view, 7);
  TEST_ASSERT_EQUAL(1, view.selected);
  TEST_ASSERT_EQUAL_STRING("Retry model file save", view.rows[0].label);
  TEST_ASSERT_FALSE(draft.review().address);
  TEST_ASSERT_EQUAL(0, draft.requested());
  draft.turn(-1, 1);
  TEST_ASSERT_EQUAL(PrimaryCounterEffect::queueFile, draft.activate(2));
  draft.queued();
  draft.render(view, 7);
  TEST_ASSERT_EQUAL_STRING("File save queued", view.title);
  draft.busy();
  draft.render(view, 7);
  TEST_ASSERT_EQUAL_STRING("Close result", view.rows[0].label);
  TEST_ASSERT_NOT_NULL(strstr(view.hint, "already applied"));
  draft.completeFile({ModelCommitState::saved});
  draft.render(view, 7);
  TEST_ASSERT_EQUAL_STRING("Counter saved", view.title);
  TEST_ASSERT_EQUAL(1, view.rowCount);
  TEST_ASSERT_EQUAL(PrimaryCounterEffect::leave, draft.activate(3));
  draft.open(review());
  draft.complete({CounterDomainState::nvsUncertain, false, {}});
  draft.render(view, 7);
  TEST_ASSERT_EQUAL_STRING("Counter uncertain", view.title);
  TEST_ASSERT_EQUAL(1, view.rowCount);
  TEST_ASSERT_EQUAL_STRING("Close", view.rows[0].label);
  TEST_ASSERT_FALSE(draft.review().address);
  TEST_ASSERT_EQUAL(PrimaryCounterEffect::leave, draft.activate(4));
  draft.open(review());
  draft.complete({CounterDomainState::saved, false, {ModelCommitState::saved}});
  draft.render(view, 7);
  TEST_ASSERT_NOT_EQUAL(
      0, strcmp("Counter saved", view.title)); // An inconsistent outcome cannot
                                               // become verified success.
}
static void actions_navigation_counts_as_numeric_activity_without_applying() {
  PrimaryCounterDraft draft;
  draft.open(review());
  draft.turn(1, 1);
  draft.activate(2);
  draft.activate(1000);
  draft.turn(1, 29000);
  draft.tick(30003);
  TEST_ASSERT_FALSE(draft.expired());
  draft.back(30004);
  draft.tick(59004);
  TEST_ASSERT_FALSE(draft.expired());
  draft.tick(60004);
  TEST_ASSERT_TRUE(draft.expired());
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(actions_navigation_counts_as_numeric_activity_without_applying);
  RUN_TEST(explicit_numeric_apply_and_default_cancel_never_correct_on_rotation);
  RUN_TEST(coarse_steps_boundaries_and_reconciliation_qualification);
  RUN_TEST(applied_file_failure_has_only_file_retry_and_unknown_nvs_close_only);
  return UNITY_END();
}
