#include <unity.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include "MotionDiagnostics.h"

// Contract: fixed capacity, numeric records only; categories motion=1,
// radio=2, calibration=4. Successful appends receive lifetime-monotonic
// uint64 sequences. read(after,out,max) returns chronological records strictly
// after the cursor, next=last returned (or unchanged), and reports lost history
// when after predates the oldest retained sequence minus one. Expiry stops
// capture but retains evidence; configure(0,0,now) disables and clears it.
static_assert(std::is_trivially_copyable<MotionDiagnosticRecord>::value,
    "Diagnostic records must not own strings or other dynamic storage");

void setUp() {}
void tearDown() {}

static bool append(MotionDiagnostics &diagnostics, uint8_t category, uint32_t now,
    int32_t value = 123) {
  return diagnostics.append(category, 9, 7, 41, value, now);
}
static void test_default_diagnostics_are_disabled_and_empty() {
  MotionDiagnostics diagnostics;
  TEST_ASSERT_FALSE(diagnostics.enabled(0));
  TEST_ASSERT_FALSE(append(diagnostics, 1, 0));
  TEST_ASSERT_FALSE(append(diagnostics, 2, 100));
  std::array<MotionDiagnosticRecord, MotionDiagnostics::capacity> records{};
  const auto result = diagnostics.read(0, records.data(), records.size());
  TEST_ASSERT_EQUAL_UINT(0, result.count);
  TEST_ASSERT_EQUAL_UINT64(0, result.next);
  TEST_ASSERT_FALSE(result.overflow);
}
static void test_each_category_bit_filters_capture_for_every_supported_mask() {
  for(uint8_t mask = 1; mask <= 7; ++mask) {
    MotionDiagnostics diagnostics;
    TEST_ASSERT_TRUE(diagnostics.configure(mask, 1000, 100));
    unsigned expected = 0;
    for(const uint8_t category : {1, 2, 4}) {
      const bool selected = (mask & category) != 0;
      TEST_ASSERT_EQUAL(selected, append(diagnostics, category, 200));
      expected += selected;
    }
    std::array<MotionDiagnosticRecord, 3> records{};
    const auto result = diagnostics.read(0, records.data(), records.size());
    TEST_ASSERT_EQUAL_UINT(expected, result.count);
    for(size_t i = 0; i < result.count; ++i)
      TEST_ASSERT_TRUE((records[i].category & mask) != 0);
  }
}
static void test_configuration_limits_are_checked_without_displacing_active_capture() {
  MotionDiagnostics diagnostics;
  TEST_ASSERT_TRUE(diagnostics.configure(2, 1000, 100));
  TEST_ASSERT_TRUE(append(diagnostics, 2, 150));
  TEST_ASSERT_FALSE(diagnostics.configure(1, 0, 200));
  TEST_ASSERT_FALSE(diagnostics.configure(1, 900001, 200));
  TEST_ASSERT_FALSE(diagnostics.configure(1, UINT32_MAX, 200));
  TEST_ASSERT_FALSE(diagnostics.configure(8, 1000, 200));
  TEST_ASSERT_FALSE(diagnostics.configure(255, 1000, 200));
  TEST_ASSERT_TRUE(diagnostics.enabled(250));
  TEST_ASSERT_TRUE(append(diagnostics, 2, 250));
  TEST_ASSERT_FALSE(append(diagnostics, 1, 250));
  std::array<MotionDiagnosticRecord, 3> records{};
  TEST_ASSERT_EQUAL_UINT(2, diagnostics.read(0, records.data(), records.size()).count);
  MotionDiagnostics maximum;
  TEST_ASSERT_TRUE(maximum.configure(7, 900000, 0));
  TEST_ASSERT_TRUE(maximum.enabled(899999));
  TEST_ASSERT_FALSE(maximum.enabled(900000));
}
static void test_ttl_expires_at_its_boundary_and_retains_preexpiry_evidence() {
  MotionDiagnostics diagnostics;
  TEST_ASSERT_TRUE(diagnostics.configure(1, 1, 0));
  TEST_ASSERT_TRUE(diagnostics.enabled(0));
  TEST_ASSERT_TRUE(append(diagnostics, 1, 0));
  TEST_ASSERT_FALSE(diagnostics.enabled(1));
  TEST_ASSERT_FALSE(append(diagnostics, 1, 1));
  TEST_ASSERT_FALSE(append(diagnostics, 1, 100));
  TEST_ASSERT_FALSE(diagnostics.enabled(0)); // Expiry cannot reactivate after clock wrap.
  std::array<MotionDiagnosticRecord, 2> records{};
  const auto result = diagnostics.read(0, records.data(), records.size());
  TEST_ASSERT_EQUAL_UINT(1, result.count);
  TEST_ASSERT_EQUAL_UINT32(0, records[0].at);
}
static void test_ttl_and_chronological_sequence_work_across_uint32_wrap() {
  MotionDiagnostics diagnostics;
  const uint32_t start = 0xfffffff0u;
  TEST_ASSERT_TRUE(diagnostics.configure(1, 100, start));
  TEST_ASSERT_TRUE(append(diagnostics, 1, start, 1));
  TEST_ASSERT_TRUE(append(diagnostics, 1, start + 99u, 2));
  TEST_ASSERT_FALSE(diagnostics.enabled(start + 100u));
  TEST_ASSERT_FALSE(append(diagnostics, 1, start + 100u, 3));
  std::array<MotionDiagnosticRecord, 3> records{};
  const auto result = diagnostics.read(0, records.data(), records.size());
  TEST_ASSERT_EQUAL_UINT(2, result.count);
  TEST_ASSERT_EQUAL_UINT32(start, records[0].at);
  TEST_ASSERT_EQUAL_UINT32(start + 99u, records[1].at);
  TEST_ASSERT_TRUE(records[1].sequence > records[0].sequence);
  TEST_ASSERT_EQUAL_INT32(1, records[0].value);
  TEST_ASSERT_EQUAL_INT32(2, records[1].value);
}
static void test_reads_are_bounded_and_cursor_continues_without_duplicate_records() {
  MotionDiagnostics diagnostics;
  TEST_ASSERT_TRUE(diagnostics.configure(7, 1000, 100));
  for(unsigned i = 0; i < 5; ++i)
    TEST_ASSERT_TRUE(diagnostics.append(4, 3, 0, UINT32_MAX, INT32_MIN + static_cast<int32_t>(i), 100 + i));
  std::array<MotionDiagnosticRecord, 4> records{};
  records[2].value = 777;
  const auto first = diagnostics.read(0, records.data(), 2);
  TEST_ASSERT_EQUAL_UINT(2, first.count);
  TEST_ASSERT_FALSE(first.overflow);
  TEST_ASSERT_EQUAL_UINT64(records[1].sequence, first.next);
  TEST_ASSERT_TRUE(records[1].sequence > records[0].sequence);
  TEST_ASSERT_EQUAL_UINT8(4, records[0].category);
  TEST_ASSERT_EQUAL_UINT8(3, records[0].kind);
  TEST_ASSERT_EQUAL_UINT8(0, records[0].shadeId);
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, records[0].ticket);
  TEST_ASSERT_EQUAL_INT32(INT32_MIN, records[0].value);
  TEST_ASSERT_EQUAL_INT32(777, records[2].value); // Never writes beyond max.
  const auto second = diagnostics.read(first.next, records.data(), records.size());
  TEST_ASSERT_EQUAL_UINT(3, second.count);
  TEST_ASSERT_FALSE(second.overflow);
  TEST_ASSERT_TRUE(records[0].sequence > first.next);
  TEST_ASSERT_EQUAL_INT32(INT32_MIN + 2, records[0].value);
  const auto empty = diagnostics.read(second.next, records.data(), records.size());
  TEST_ASSERT_EQUAL_UINT(0, empty.count);
  TEST_ASSERT_EQUAL_UINT64(second.next, empty.next);
}
static void test_ring_overflow_reports_missing_history_and_returns_only_retained_records() {
  MotionDiagnostics diagnostics;
  TEST_ASSERT_TRUE(diagnostics.configure(1, 900000, 0));
  const unsigned total = MotionDiagnostics::capacity + 5;
  for(unsigned i = 0; i < total; ++i) TEST_ASSERT_TRUE(append(diagnostics, 1, i, i));
  std::array<MotionDiagnosticRecord, MotionDiagnostics::capacity> records{};
  const auto result = diagnostics.read(0, records.data(), records.size());
  TEST_ASSERT_TRUE(result.overflow);
  TEST_ASSERT_EQUAL_UINT(MotionDiagnostics::capacity, result.count);
  TEST_ASSERT_EQUAL_INT32(5, records[0].value);
  TEST_ASSERT_EQUAL_INT32(total - 1, records[result.count - 1].value);
  for(size_t i = 1; i < result.count; ++i)
    TEST_ASSERT_EQUAL_UINT64(records[i - 1].sequence + 1, records[i].sequence);
  const auto cursor = records[0].sequence;
  const auto continued = diagnostics.read(cursor, records.data(), 1);
  TEST_ASSERT_EQUAL_UINT(1, continued.count);
  TEST_ASSERT_FALSE(continued.overflow);
  TEST_ASSERT_EQUAL_UINT64(cursor + 1, records[0].sequence);
}
static void test_explicit_disable_clears_records_and_rearming_does_not_reuse_old_sequences() {
  MotionDiagnostics diagnostics;
  TEST_ASSERT_TRUE(diagnostics.configure(1, 1000, 100));
  TEST_ASSERT_TRUE(append(diagnostics, 1, 200));
  MotionDiagnosticRecord record{};
  const auto previous = diagnostics.read(0, &record, 1).next;
  TEST_ASSERT_TRUE(diagnostics.configure(0, 0, 300));
  TEST_ASSERT_FALSE(diagnostics.enabled(300));
  TEST_ASSERT_FALSE(append(diagnostics, 1, 301));
  TEST_ASSERT_EQUAL_UINT(0, diagnostics.read(0, &record, 1).count);
  TEST_ASSERT_TRUE(diagnostics.configure(4, 1000, 400));
  TEST_ASSERT_TRUE(append(diagnostics, 4, 500));
  const auto next = diagnostics.read(previous, &record, 1);
  TEST_ASSERT_EQUAL_UINT(1, next.count);
  TEST_ASSERT_TRUE(next.next > previous);
  TEST_ASSERT_FALSE(next.overflow);
}
static void test_zero_capacity_or_null_output_does_not_write_or_advance_cursor() {
  MotionDiagnostics diagnostics;
  TEST_ASSERT_TRUE(diagnostics.configure(1, 1000, 0));
  TEST_ASSERT_TRUE(append(diagnostics, 1, 100));
  MotionDiagnosticRecord record{};
  record.value = 777;
  const auto noCapacity = diagnostics.read(0, &record, 0);
  TEST_ASSERT_EQUAL_UINT(0, noCapacity.count);
  TEST_ASSERT_EQUAL_UINT64(0, noCapacity.next);
  TEST_ASSERT_EQUAL_INT32(777, record.value);
  const auto noOutput = diagnostics.read(99, nullptr, 10);
  TEST_ASSERT_EQUAL_UINT(0, noOutput.count);
  TEST_ASSERT_EQUAL_UINT64(99, noOutput.next);
}

static void test_new_capture_after_explicit_clear_has_no_false_overflow() {
  MotionDiagnostics diagnostics;
  diagnostics.configure(2,1000,0);
  append(diagnostics,2,100);
  diagnostics.configure(0,0,200);
  diagnostics.configure(2,1000,300);
  append(diagnostics,2,400);
  MotionDiagnosticRecord record;
  const auto result=diagnostics.read(0,&record,1);
  TEST_ASSERT_EQUAL_UINT(1,result.count);
  TEST_ASSERT_FALSE(result.overflow);
  TEST_ASSERT_TRUE(record.sequence>1);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_new_capture_after_explicit_clear_has_no_false_overflow);
  RUN_TEST(test_default_diagnostics_are_disabled_and_empty);
  RUN_TEST(test_each_category_bit_filters_capture_for_every_supported_mask);
  RUN_TEST(test_configuration_limits_are_checked_without_displacing_active_capture);
  RUN_TEST(test_ttl_expires_at_its_boundary_and_retains_preexpiry_evidence);
  RUN_TEST(test_ttl_and_chronological_sequence_work_across_uint32_wrap);
  RUN_TEST(test_reads_are_bounded_and_cursor_continues_without_duplicate_records);
  RUN_TEST(test_ring_overflow_reports_missing_history_and_returns_only_retained_records);
  RUN_TEST(test_explicit_disable_clears_records_and_rearming_does_not_reuse_old_sequences);
  RUN_TEST(test_zero_capacity_or_null_output_does_not_write_or_advance_cursor);
  return UNITY_END();
}
