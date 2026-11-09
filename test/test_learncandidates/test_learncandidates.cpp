#include "LocalLearnCandidates.h"
#include "SomfyFrame.h"
#include <unity.h>
using namespace LocalUi;
static void heard(ReceiveHub &hub, uint64_t at, uint32_t address, uint16_t code,
                  uint8_t bits = 56, uint8_t protocol = 0, bool valid = true,
                  bool scanning = false,
                  uint8_t command = static_cast<uint8_t>(somfy_commands::Up)) {
  ReceiveObservation value;
  value.observedAt = at;
  value.address = address;
  value.rollingCode = code;
  value.bits = bits;
  value.protocol = protocol;
  value.command = command;
  value.flags = (valid ? ReceiveObservation::validFlag : 0) |
                (scanning ? ReceiveObservation::scanningFlag : 0);
  hub.publish(value);
}
void setUp() {}
void tearDown() {}
static void bounded_passive_list_preserves_selected_identity_and_duplicates() {
  ReceiveHub hub;
  LearnCandidates cache(hub);
  TEST_ASSERT_TRUE(cache.begin(100));
  for (uint8_t i = 1; i <= 8; i++)
    heard(hub, 100 + i, i, 10 + i);
  TEST_ASSERT_EQUAL(8, cache.count());
  heard(hub, 109, 99, 999);
  TEST_ASSERT_TRUE(cache.overflowed());
  TEST_ASSERT_EQUAL(8, cache.count());
  LearnCandidate first;
  TEST_ASSERT_TRUE(cache.candidate(0, first));
  TEST_ASSERT_EQUAL(1, first.address);
  const auto revision = first.revision;
  heard(hub, 110, 1, 11);
  TEST_ASSERT_TRUE(cache.candidate(0, first));
  TEST_ASSERT_EQUAL(revision, first.revision);
  TEST_ASSERT_EQUAL(110, first.observedAt);
  LearnSelection selected;
  TEST_ASSERT_EQUAL(LearnCheck::ready, cache.select(0, 111, selected));
  heard(hub, 112, 99, 999);
  TEST_ASSERT_EQUAL(LearnCheck::ready, cache.validate(selected, 113));
}
static void frozen_review_requires_explicit_new_selection_for_higher_code() {
  ReceiveHub hub;
  LearnCandidates cache(hub);
  cache.begin(0);
  heard(hub, 1, 123, 20);
  LearnSelection first, next;
  TEST_ASSERT_EQUAL(LearnCheck::ready, cache.select(0, 2, first));
  heard(hub, 3, 123, 20);
  TEST_ASSERT_EQUAL(LearnCheck::ready, cache.validate(first, 4));
  heard(hub, 5, 123, 21);
  TEST_ASSERT_EQUAL(LearnCheck::changed, cache.validate(first, 6));
  TEST_ASSERT_EQUAL(LearnCheck::ready, cache.select(0, 7, next));
  TEST_ASSERT_EQUAL(21, next.rollingCode);
  TEST_ASSERT_NOT_EQUAL(first.revision, next.revision);
  TEST_ASSERT_EQUAL(LearnCheck::changed, cache.validate(first, 8));
  TEST_ASSERT_EQUAL(LearnCheck::ready, cache.validate(next, 9));
  cache.end();
  cache.begin(10);
  heard(hub, 11, 123, 21);
  LearnSelection replacement;
  cache.select(0, 12, replacement);
  TEST_ASSERT_NOT_EQUAL(first.session, replacement.session);
  TEST_ASSERT_EQUAL(LearnCheck::changed, cache.validate(next, 13));
}
static void address_highwater_covers_formats_even_at_capacity() {
  ReceiveHub hub;
  LearnCandidates cache(hub);
  cache.begin(0);
  for (uint8_t i = 1; i <= 8; i++)
    heard(hub, i, i, 100);
  heard(hub, 9, 1, 101, 80);
  LearnCandidate first;
  cache.candidate(0, first);
  TEST_ASSERT_EQUAL(101, first.highwater);
  LearnSelection selection;
  TEST_ASSERT_EQUAL(LearnCheck::changed, cache.select(0, 10, selection));
  TEST_ASSERT_TRUE(cache.overflowed());
  // A second dropped format must retain the ADDRESS maximum and make its
  // lower counter sticky; it cannot lower the first format's admission bound.
  heard(hub, 11, 1, 90, 80);
  cache.candidate(0, first);
  TEST_ASSERT_EQUAL(101, first.highwater);
  TEST_ASSERT_EQUAL(LearnCheck::regressed, cache.select(0, 12, selection));
  cache.end();
  cache.begin(20);
  heard(hub, 21, 1, 100);
  TEST_ASSERT_EQUAL(LearnCheck::ready, cache.select(0, 22, selection));
  heard(hub, 23, 1, 100, 80);
  TEST_ASSERT_EQUAL(LearnCheck::changed, cache.validate(selection, 24));
  TEST_ASSERT_EQUAL(1, cache.count());
}
static void lower_and_wrapped_counters_require_rescan_without_inference() {
  ReceiveHub hub;
  LearnCandidates cache(hub);
  cache.begin(0);
  heard(hub, 1, 123, 65535);
  LearnSelection selection;
  cache.select(0, 2, selection);
  heard(hub, 3, 123, 1);
  TEST_ASSERT_EQUAL(LearnCheck::regressed, cache.validate(selection, 4));
  LearnCandidate value;
  cache.candidate(0, value);
  TEST_ASSERT_EQUAL(65535, value.rollingCode);
  TEST_ASSERT_EQUAL(65535, value.highwater);
  TEST_ASSERT_EQUAL(1, value.lastCode);
  TEST_ASSERT_EQUAL(LearnCheck::regressed, cache.select(0, 5, selection));
  cache.end();
  TEST_ASSERT_EQUAL(0, cache.count());
  TEST_ASSERT_FALSE(cache.candidate(0, value));
  TEST_ASSERT_EQUAL(0, value.address);
  TEST_ASSERT_FALSE(hub.active());
}
static void
valid_sensor_zero_is_eligible_invalid_scan_and_unsupported_are_not() {
  ReceiveHub hub;
  LearnCandidates cache(hub);
  cache.begin(0);
  heard(hub, 1, 1, 1, 56, 0, false);
  heard(hub, 2, 1, 1, 56, 0, true, true);
  heard(hub, 3, 1, 1, 44);
  heard(hub, 4, 1, 1, 56, 8);
  heard(hub, 5, 0, 1);
  heard(hub, 6, 0x1000000, 1);
  TEST_ASSERT_EQUAL(0, cache.count());
  heard(hub, 7, 2, 0, 80, 2, true, false,
        static_cast<uint8_t>(somfy_commands::Sensor));
  TEST_ASSERT_EQUAL(1, cache.count());
  LearnSelection selection;
  TEST_ASSERT_EQUAL(LearnCheck::ready, cache.select(0, 8, selection));
  TEST_ASSERT_EQUAL(0, selection.rollingCode);
}
static void timeout_clock_and_subscription_loss_wipe_and_fail_closed() {
  ReceiveHub hub;
  LearnCandidates cache(hub), other(hub);
  TEST_ASSERT_TRUE(cache.begin(100));
  TEST_ASSERT_FALSE(other.begin(100));
  TEST_ASSERT_EQUAL(LearnCheck::busy, other.status());
  heard(hub, 101, 123, 20);
  LearnSelection selection;
  cache.select(0, 102, selection);
  TEST_ASSERT_EQUAL(LearnCheck::clock, cache.validate(selection, 99));
  TEST_ASSERT_FALSE(hub.active());
  TEST_ASSERT_EQUAL(0, cache.count());
  cache.begin(200);
  heard(hub, 201, 123, 20);
  cache.select(0, 202, selection);
  TEST_ASSERT_EQUAL(LearnCheck::expired, cache.validate(selection, 30200));
  TEST_ASSERT_EQUAL(0, cache.count());
  TEST_ASSERT_TRUE(other.begin(40000));
  heard(hub, 40001, 123, 20);
  TEST_ASSERT_EQUAL(1, other.count());
  other.end();
  TEST_ASSERT_FALSE(hub.active());
}
static void
invalid_observation_sequence_and_lost_subscription_never_keep_link_authority() {
  ReceiveHub hub;
  LearnCandidates cache(hub);
  cache.begin(0);
  heard(hub, 1, 123, 20);
  LearnSelection selected;
  cache.select(0, 2, selected);
  ReceiveObservation replay;
  replay.observedAt = 3;
  replay.sequence = 0;
  replay.address = 123;
  replay.rollingCode = 21;
  replay.bits = 56;
  replay.flags = ReceiveObservation::validFlag;
  static_cast<ReceiveSink &>(cache).received(replay, {});
  TEST_ASSERT_EQUAL(LearnCheck::sequence, cache.status());
  TEST_ASSERT_EQUAL(0, cache.count());
  TEST_ASSERT_FALSE(hub.active());
  cache.begin(10);
  heard(hub, 11, 123, 20);
  cache.select(0, 12, selected);
  TEST_ASSERT_TRUE(
      hub.unsubscribe({selected.session, ReceiveChannel::learning}, cache));
  TEST_ASSERT_EQUAL(LearnCheck::inactive, cache.validate(selected, 13));
  TEST_ASSERT_EQUAL(0, cache.count());
}
static void
observation_window_keeps_full_64_bit_time_and_invalid_selection_drops_old_intent() {
  ReceiveHub hub;
  LearnCandidates cache(hub);
  constexpr uint64_t base = uint64_t(1) << 32;
  cache.begin(base);
  heard(hub, base + 1, 123, 20);
  LearnSelection selection;
  TEST_ASSERT_EQUAL(LearnCheck::ready, cache.select(0, base + 2, selection));
  const auto old = selection;
  TEST_ASSERT_EQUAL(LearnCheck::invalid, cache.select(7, base + 3, selection));
  TEST_ASSERT_EQUAL(0, selection.session);
  TEST_ASSERT_EQUAL(LearnCheck::changed, cache.validate(old, base + 4));
  TEST_ASSERT_EQUAL(LearnCheck::expired, cache.tick(base + 30000));
  TEST_ASSERT_FALSE(hub.active());
}
static void repeated_nonzero_sequence_and_destruction_detach_before_reuse() {
  ReceiveHub hub;
  {
    LearnCandidates cache(hub);
    TEST_ASSERT_TRUE(cache.begin(0));
    heard(hub, 1, 123, 20);
    LearnSelection selected;
    cache.select(0, 2, selected);
    ReceiveObservation replay;
    replay.observedAt = 3;
    replay.sequence = selected.revision;
    replay.address = 123;
    replay.rollingCode = 21;
    replay.bits = 56;
    replay.flags = ReceiveObservation::validFlag;
    static_cast<ReceiveSink &>(cache).received(replay, {});
    TEST_ASSERT_EQUAL(LearnCheck::sequence, cache.status());
    TEST_ASSERT_EQUAL(0, cache.count());
    TEST_ASSERT_FALSE(hub.active());
    TEST_ASSERT_TRUE(cache.begin(10));
    heard(hub, 11, 456, 30);
    TEST_ASSERT_EQUAL(1, cache.count());
  }
  TEST_ASSERT_FALSE(hub.active());
  LearnCandidates replacement(hub);
  TEST_ASSERT_TRUE(replacement.begin(12));
  heard(hub, 13, 789, 40);
  LearnCandidate value;
  TEST_ASSERT_TRUE(replacement.candidate(0, value));
  TEST_ASSERT_EQUAL(789, value.address);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(bounded_passive_list_preserves_selected_identity_and_duplicates);
  RUN_TEST(frozen_review_requires_explicit_new_selection_for_higher_code);
  RUN_TEST(address_highwater_covers_formats_even_at_capacity);
  RUN_TEST(lower_and_wrapped_counters_require_rescan_without_inference);
  RUN_TEST(valid_sensor_zero_is_eligible_invalid_scan_and_unsupported_are_not);
  RUN_TEST(timeout_clock_and_subscription_loss_wipe_and_fail_closed);
  RUN_TEST(
      invalid_observation_sequence_and_lost_subscription_never_keep_link_authority);
  RUN_TEST(
      observation_window_keeps_full_64_bit_time_and_invalid_selection_drops_old_intent);
  RUN_TEST(repeated_nonzero_sequence_and_destruction_detach_before_reuse);
  return UNITY_END();
}
