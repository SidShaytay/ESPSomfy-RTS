#include "LocalLearnQueue.h"
#include <unity.h>
using namespace LocalUi;
namespace LocalUi {
struct LearnQueueTestAccess {
  static void exhaust(LearnQueue &q) { q.generation = UINT64_MAX; }
};
} // namespace LocalUi
struct Sink : LearnPending {
  LearnQueue &queue;
  LearnQueueReason validity = LearnQueueReason::ready,
                   last = LearnQueueReason::ready;
  unsigned executed = 0, cancelledCount = 0;
  bool retireObserved = false, destroy = false;
  Sink *replacement = nullptr;
  uint64_t replacementToken = 0;
  explicit Sink(LearnQueue &q) : queue(q) {}
  LearnQueueReason current(uint64_t) override { return validity; }
  void callback(uint64_t now) {
    retireObserved = !queue.active();
    if (replacement)
      replacementToken = queue.reserve(*replacement, now).token;
    if (destroy)
      delete this;
  }
  void execute(uint64_t now) override {
    ++executed;
    callback(now);
  }
  void cancelled(LearnQueueReason reason) override {
    ++cancelledCount;
    last = reason;
    callback(100);
  }
};
void setUp() {}
void tearDown() {}
static void exact_tokens_no_aba_and_callbacks_observe_retirement() {
  LearnQueue q;
  Sink first(q), other(q);
  auto a = q.reserve(first, 1);
  TEST_ASSERT_EQUAL(LearnQueueReason::ready, a.reason);
  TEST_ASSERT_TRUE(q.active(a.token));
  TEST_ASSERT_FALSE(q.detach(a.token, other));
  TEST_ASSERT_FALSE(q.detach(a.token + 1, first));
  TEST_ASSERT_EQUAL(LearnQueueReason::busy, q.reserve(other, 2).reason);
  TEST_ASSERT_TRUE(q.detach(a.token, first));
  auto b = q.reserve(other, 3);
  TEST_ASSERT_TRUE(b.token > a.token);
  q.run(a.token, 4);
  TEST_ASSERT_EQUAL(0, other.executed);
  q.run(b.token, 5);
  TEST_ASSERT_TRUE(other.retireObserved);
  TEST_ASSERT_EQUAL(1, other.executed);
  q.run(b.token, 6);
  TEST_ASSERT_EQUAL(1, other.executed);
}
static void age_clock_and_source_invalidations_retire_without_execution() {
  LearnQueue q;
  Sink sink(q);
  auto a = q.reserve(sink, 100);
  q.refresh(5099);
  TEST_ASSERT_TRUE(q.active(a.token));
  q.refresh(5100);
  TEST_ASSERT_EQUAL(LearnQueueReason::expired, sink.last);
  TEST_ASSERT_EQUAL(0, sink.executed);
  TEST_ASSERT_FALSE(q.active());
  a = q.reserve(sink, 6000);
  q.run(a.token, 5999);
  TEST_ASSERT_EQUAL(LearnQueueReason::clock, sink.last);
  TEST_ASSERT_FALSE(q.active());
  a = q.reserve(sink, 7000);
  sink.validity = LearnQueueReason::candidate;
  q.refresh(7001);
  TEST_ASSERT_EQUAL(LearnQueueReason::candidate, sink.last);
  TEST_ASSERT_FALSE(q.active());
  TEST_ASSERT_TRUE(sink.retireObserved);
}
static void
callback_may_destroy_source_and_register_replacement_without_clobber() {
  LearnQueue q;
  Sink replacement(q);
  auto *source = new Sink(q);
  source->replacement = &replacement;
  source->destroy = true;
  auto a = q.reserve(*source, 1);
  q.run(a.token, 2);
  TEST_ASSERT_TRUE(q.active());
  q.run(a.token, 3);
  TEST_ASSERT_EQUAL(0, replacement.executed);
  q.run(a.token + 1, 4);
  TEST_ASSERT_EQUAL(1, replacement.executed);
  TEST_ASSERT_FALSE(q.active());
  source = new Sink(q);
  source->replacement = &replacement;
  source->destroy = true;
  source->validity = LearnQueueReason::ready;
  a = q.reserve(*source, 5);
  source->validity = LearnQueueReason::stale;
  q.refresh(6);
  TEST_ASSERT_TRUE(q.active());
  q.run(a.token + 1, 101);
  TEST_ASSERT_EQUAL(2, replacement.executed);
}
static void exhausted_history_and_invalid_initial_source_never_reserve() {
  LearnQueue q;
  Sink sink(q);
  sink.validity = LearnQueueReason::stale;
  TEST_ASSERT_EQUAL(LearnQueueReason::stale, q.reserve(sink, 0).reason);
  TEST_ASSERT_FALSE(q.active());
  sink.validity = LearnQueueReason::ready;
  const auto first = q.reserve(sink, 1);
  TEST_ASSERT_EQUAL(1, first.token);
  q.detach(first.token, sink);
  LearnQueueTestAccess::exhaust(q);
  const auto exhausted = q.reserve(sink, 2);
  TEST_ASSERT_EQUAL(LearnQueueReason::exhausted, exhausted.reason);
  TEST_ASSERT_EQUAL(0, exhausted.token);
  TEST_ASSERT_FALSE(q.active());
  TEST_ASSERT_EQUAL(0, sink.executed);
  TEST_ASSERT_EQUAL(0, sink.cancelledCount);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(exhausted_history_and_invalid_initial_source_never_reserve);
  RUN_TEST(exact_tokens_no_aba_and_callbacks_observe_retirement);
  RUN_TEST(age_clock_and_source_invalidations_retire_without_execution);
  RUN_TEST(
      callback_may_destroy_source_and_register_replacement_without_clobber);
  return UNITY_END();
}
