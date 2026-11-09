#include "CounterResults.h"
#include <unity.h>
void setUp() {}
void tearDown() {}
static void typed_write_failure_and_uncertainty_are_not_collapsed() {
  const auto failed = counterLinkResult(4, {CounterDomainState::nvsFailed});
  const auto uncertain =
      counterLinkResult(4, {CounterDomainState::nvsUncertain});
  TEST_ASSERT_TRUE(failed.hasCounterResult);
  TEST_ASSERT_TRUE(uncertain.hasCounterResult);
  TEST_ASSERT_EQUAL(CounterDomainState::nvsFailed, failed.counterState);
  TEST_ASSERT_EQUAL(CounterDomainState::nvsUncertain, uncertain.counterState);
  TEST_ASSERT_FALSE(failed.applied);
  TEST_ASSERT_FALSE(uncertain.applied);
  TEST_ASSERT_EQUAL(edit_failure_t::internal, failed.failure);
  TEST_ASSERT_EQUAL(edit_failure_t::internal, uncertain.failure);
  const edit_result_t ordinary;
  TEST_ASSERT_FALSE(ordinary.hasCounterResult);
  TEST_ASSERT_EQUAL(CounterDomainState::invalid, ordinary.counterState);
}
static void
applied_file_failure_keeps_id_application_and_exact_counter_outcome() {
  const auto failed = counterLinkResult(
      4, {CounterDomainState::modelFailed,
          true,
          {ModelCommitState::failed, ModelCommitStage::write}});
  const auto uncertain = counterLinkResult(
      4, {CounterDomainState::modelUncertain,
          true,
          {ModelCommitState::uncertain, ModelCommitStage::verifyBytes}});
  TEST_ASSERT_TRUE(failed.hasCounterResult);
  TEST_ASSERT_EQUAL(CounterDomainState::modelFailed, failed.counterState);
  TEST_ASSERT_TRUE(uncertain.hasCounterResult);
  TEST_ASSERT_EQUAL(CounterDomainState::modelUncertain, uncertain.counterState);
  TEST_ASSERT_TRUE(failed.applied);
  TEST_ASSERT_TRUE(uncertain.applied);
  TEST_ASSERT_EQUAL(4, failed.id);
  TEST_ASSERT_FALSE(failed.ok);
  TEST_ASSERT_FALSE(uncertain.ok);
  TEST_ASSERT_EQUAL(ModelCommitState::failed, failed.persistence.state);
  TEST_ASSERT_EQUAL(ModelCommitState::uncertain, uncertain.persistence.state);
}
static void all_shared_counter_states_have_a_typed_tag_including_missing() {
  const CounterDomainState states[] = {
      CounterDomainState::saved,   CounterDomainState::invalid,
      CounterDomainState::missing, CounterDomainState::full,
      CounterDomainState::busy,    CounterDomainState::quarantined,
      CounterDomainState::stale};
  for (const auto state : states) {
    auto result = counterLinkResult(7, {state});
    TEST_ASSERT_TRUE(result.hasCounterResult);
    TEST_ASSERT_EQUAL(state, result.counterState);
  }
  TEST_ASSERT_TRUE(
      counterLinkResult(7, {CounterDomainState::missing}).notFound);
  const auto saved = counterLinkResult(
      7, {CounterDomainState::saved, true, {ModelCommitState::saved}});
  TEST_ASSERT_TRUE(saved.ok);
  TEST_ASSERT_TRUE(saved.applied);
  TEST_ASSERT_TRUE(saved.hasCounterResult);
  TEST_ASSERT_EQUAL(CounterDomainState::saved, saved.counterState);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(typed_write_failure_and_uncertainty_are_not_collapsed);
  RUN_TEST(applied_file_failure_keeps_id_application_and_exact_counter_outcome);
  RUN_TEST(all_shared_counter_states_have_a_typed_tag_including_missing);
  return UNITY_END();
}
