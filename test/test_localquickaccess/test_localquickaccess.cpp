#include <unity.h>
#include "LocalQuickAccess.h"
#include <cstring>
#include <initializer_list>
using namespace LocalUi;
void setUp() {} void tearDown() {}
static QuickAccess populated() {
  QuickAccess value;
  TEST_ASSERT_EQUAL(QuickAccessEdit::changed,value.assign(0,{EntityKind::device,7},0x010203));
  TEST_ASSERT_EQUAL(QuickAccessEdit::changed,value.assign(2,{EntityKind::group,7},0xABCDEF));
  return value;
}
static void assertEmpty(const QuickAccess &value) {
  for(uint8_t i=0;i<quickAccessSlots;i++) {
    const auto *entry=value.at(i);TEST_ASSERT_NOT_NULL(entry);
    TEST_ASSERT_TRUE(entry->empty());TEST_ASSERT_EQUAL_UINT32(0,entry->identity);
  }
}
static void empty_slots_and_edits_are_bounded() {
  QuickAccess value;assertEmpty(value);TEST_ASSERT_NULL(value.at(3));TEST_ASSERT_NULL(value.at(255));
  TEST_ASSERT_EQUAL(QuickAccessEdit::invalidSlot,value.remove(3));
  TEST_ASSERT_EQUAL(QuickAccessEdit::unchanged,value.remove(0));
  TEST_ASSERT_EQUAL(QuickAccessEdit::invalidSlot,value.move(0,3));
  TEST_ASSERT_EQUAL(QuickAccessEdit::invalidSlot,value.assign(255,{EntityKind::device,7},1));
  TEST_ASSERT_NULL(value.at(size_t(-1)));
  TEST_ASSERT_EQUAL(QuickAccessEdit::invalidSlot,value.move(size_t(-1),0));
  TEST_ASSERT_EQUAL(QuickAccessEdit::invalidSlot,value.assign(256,{EntityKind::device,7},1));
  assertEmpty(value);
}
static void assignment_rejects_invalid_targets_and_duplicate_typed_ids() {
  auto value=populated();
  for(auto ref:{EntityRef{EntityKind::device,0},EntityRef{EntityKind::device,255},EntityRef{EntityKind(255),1}})
    TEST_ASSERT_EQUAL(QuickAccessEdit::invalidBinding,value.assign(1,ref,1));
  for(uint32_t address:{0u,0x1000000u,0xFFFFFFFFu})
    TEST_ASSERT_EQUAL(QuickAccessEdit::invalidBinding,value.assign(1,{EntityKind::device,8},address));
  TEST_ASSERT_EQUAL(QuickAccessEdit::duplicate,value.assign(1,{EntityKind::device,7},0xAABBCC));
  TEST_ASSERT_TRUE(value.at(1)->empty());
  TEST_ASSERT_EQUAL(QuickAccessEdit::unchanged,value.assign(0,{EntityKind::device,7},0x010203));
  TEST_ASSERT_EQUAL(QuickAccessEdit::changed,value.assign(0,{EntityKind::device,7},0xFFFFFF));
  TEST_ASSERT_EQUAL_UINT32(0xFFFFFF,value.at(0)->identity);
}
static void removal_and_reordering_preserve_holes_and_other_entries() {
  auto value=populated();
  TEST_ASSERT_EQUAL(QuickAccessEdit::changed,value.move(2,0));
  TEST_ASSERT_EQUAL(EntityKind::group,value.at(0)->ref.kind);
  TEST_ASSERT_EQUAL(EntityKind::device,value.at(1)->ref.kind);
  TEST_ASSERT_TRUE(value.at(2)->empty());
  TEST_ASSERT_EQUAL(QuickAccessEdit::changed,value.move(0,2));
  TEST_ASSERT_EQUAL(EntityKind::device,value.at(0)->ref.kind);
  TEST_ASSERT_TRUE(value.at(1)->empty());
  TEST_ASSERT_EQUAL(EntityKind::group,value.at(2)->ref.kind);
  TEST_ASSERT_EQUAL(QuickAccessEdit::unchanged,value.move(2,2));
  TEST_ASSERT_EQUAL(QuickAccessEdit::changed,value.remove(0));
  TEST_ASSERT_TRUE(value.at(0)->empty());TEST_ASSERT_TRUE(value.at(1)->empty());
  TEST_ASSERT_EQUAL_UINT32(0xABCDEF,value.at(2)->identity);
}
// Format bytes and CRC independently generated with Python's zlib.crc32.
static const uint8_t fixture[quickAccessBytes]={
  0x4c,0x51,0x41,0x01, 0x01,0x07,0x01,0x02,0x03,0,0,0,
  0,0,0,0,0,0,0,0, 0x02,0x07,0xab,0xcd,0xef,0,0,0,
  0xaa,0x16,0x79,0x32
};
static void encoding_is_fixed_versioned_and_never_overwrites_short_output() {
  auto value=populated();uint8_t out[quickAccessBytes+1];memset(out,0xA5,sizeof(out));
  TEST_ASSERT_FALSE(encodeQuickAccess(value,out,quickAccessBytes-1));
  for(auto byte:out)TEST_ASSERT_EQUAL_HEX8(0xA5,byte);
  TEST_ASSERT_FALSE(encodeQuickAccess(value,nullptr,quickAccessBytes));
  TEST_ASSERT_TRUE(encodeQuickAccess(value,out,sizeof(out)));
  TEST_ASSERT_EQUAL_MEMORY(fixture,out,quickAccessBytes);TEST_ASSERT_EQUAL_HEX8(0xA5,out[quickAccessBytes]);
  QuickAccess decoded;TEST_ASSERT_EQUAL(QuickAccessLoad::valid,decodeQuickAccess(out,quickAccessBytes,decoded));
  TEST_ASSERT_TRUE(decoded.at(1)->empty());TEST_ASSERT_EQUAL_UINT32(0xABCDEF,decoded.at(2)->identity);
}
static void full_and_empty_records_roundtrip_at_identity_boundaries() {
  QuickAccess value;
  uint8_t bytes[quickAccessBytes];QuickAccess decoded;
  TEST_ASSERT_TRUE(encodeQuickAccess(value,bytes,sizeof(bytes)));
  TEST_ASSERT_EQUAL(QuickAccessLoad::valid,decodeQuickAccess(bytes,sizeof(bytes),decoded));assertEmpty(decoded);
  TEST_ASSERT_EQUAL(QuickAccessEdit::changed,value.assign(0,{EntityKind::device,1},1));
  TEST_ASSERT_EQUAL(QuickAccessEdit::changed,value.assign(1,{EntityKind::device,254},0xFFFFFF));
  TEST_ASSERT_EQUAL(QuickAccessEdit::changed,value.assign(2,{EntityKind::group,254},0x010000));
  TEST_ASSERT_TRUE(encodeQuickAccess(value,bytes,sizeof(bytes)));
  TEST_ASSERT_EQUAL(QuickAccessLoad::valid,decodeQuickAccess(bytes,sizeof(bytes),decoded));
  for(size_t i=0;i<quickAccessSlots;i++) {
    TEST_ASSERT_TRUE(value.at(i)->ref==decoded.at(i)->ref);
    TEST_ASSERT_EQUAL_UINT32(value.at(i)->identity,decoded.at(i)->identity);
  }
}
static void malformed_size_and_any_single_bit_corruption_clear_prior_shortcuts() {
  uint8_t bytes[65]={};memcpy(bytes,fixture,sizeof(fixture));
  for(size_t size=0;size<=sizeof(bytes);size++) {
    if(size==quickAccessBytes)continue;
    auto decoded=populated();TEST_ASSERT_EQUAL(QuickAccessLoad::invalid,decodeQuickAccess(bytes,size,decoded));assertEmpty(decoded);
  }
  auto decoded=populated();TEST_ASSERT_EQUAL(QuickAccessLoad::invalid,decodeQuickAccess(nullptr,quickAccessBytes,decoded));assertEmpty(decoded);
  for(size_t i=0;i<quickAccessBytes;i++)for(unsigned bit=0;bit<8;bit++) {
    memcpy(bytes,fixture,sizeof(fixture));bytes[i]^=1u<<bit;decoded=populated();
    TEST_ASSERT_EQUAL(QuickAccessLoad::invalid,decodeQuickAccess(bytes,quickAccessBytes,decoded));assertEmpty(decoded);
  }
}
static void structural_errors_are_rejected_even_with_a_correct_crc() {
  struct Mutation {uint8_t offset,value;uint32_t crc;};
  const Mutation cases[]={ {0,0,0x547E4D76u},{3,2,0x0794A0F9u},{4,3,0xEB42DAA5u},{5,0,0x44C08E04u},{5,255,0x3F09F408u},{9,1,0xE59B96F2u},{12,1,0x9C11873Bu},{13,1,0x8B82CD42u},{14,1,0xAF76F7DCu},{15,1,0xEFEFCF2Fu},{16,1,0xA9DC5AC5u},{19,1,0x250202E9u} };
  for(const auto &change:cases) {
    uint8_t bytes[quickAccessBytes];memcpy(bytes,fixture,sizeof(bytes));bytes[change.offset]=change.value;
    for(unsigned i=0;i<4;i++)bytes[28+i]=uint8_t(change.crc>>(i*8));
    auto decoded=populated();TEST_ASSERT_EQUAL(QuickAccessLoad::invalid,decodeQuickAccess(bytes,sizeof(bytes),decoded));assertEmpty(decoded);
  }
  uint8_t duplicate[quickAccessBytes];memcpy(duplicate,fixture,sizeof(duplicate));duplicate[20]=1;
  const uint32_t crc=0xBCF61149u;
  for(unsigned i=0;i<4;i++)duplicate[28+i]=uint8_t(crc>>(i*8));
  auto decoded=populated();TEST_ASSERT_EQUAL(QuickAccessLoad::invalid,decodeQuickAccess(duplicate,sizeof(duplicate),decoded));assertEmpty(decoded);
  // Structurally nonempty record with a zero RF anchor and a valid CRC.
  uint8_t zeroAddress[quickAccessBytes];memcpy(zeroAddress,fixture,sizeof(zeroAddress));
  zeroAddress[6]=zeroAddress[7]=zeroAddress[8]=0;
  const uint32_t zeroCrc=0x6E3C3EDDu;
  for(unsigned i=0;i<4;i++)zeroAddress[28+i]=uint8_t(zeroCrc>>(i*8));
  decoded=populated();TEST_ASSERT_EQUAL(QuickAccessLoad::invalid,decodeQuickAccess(zeroAddress,sizeof(zeroAddress),decoded));assertEmpty(decoded);
}
static void missing_invalid_and_unreadable_reads_are_distinct_and_fail_closed() {
  auto value=populated();
  TEST_ASSERT_EQUAL(QuickAccessLoad::missing,loadQuickAccess({QuickAccessReadState::missing,nullptr,0},value));assertEmpty(value);
  value=populated();TEST_ASSERT_EQUAL(QuickAccessLoad::unreadable,loadQuickAccess({QuickAccessReadState::unreadable,nullptr,0},value));assertEmpty(value);
  value=populated();TEST_ASSERT_EQUAL(QuickAccessLoad::invalid,loadQuickAccess({QuickAccessReadState(255),fixture,sizeof(fixture)},value));assertEmpty(value);
  TEST_ASSERT_EQUAL(QuickAccessLoad::valid,loadQuickAccess({QuickAccessReadState::present,fixture,sizeof(fixture)},value));
  TEST_ASSERT_EQUAL_UINT32(0x010203,value.at(0)->identity);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(full_and_empty_records_roundtrip_at_identity_boundaries);
  RUN_TEST(empty_slots_and_edits_are_bounded);
  RUN_TEST(assignment_rejects_invalid_targets_and_duplicate_typed_ids);
  RUN_TEST(removal_and_reordering_preserve_holes_and_other_entries);
  RUN_TEST(encoding_is_fixed_versioned_and_never_overwrites_short_output);
  RUN_TEST(malformed_size_and_any_single_bit_corruption_clear_prior_shortcuts);
  RUN_TEST(structural_errors_are_rejected_even_with_a_correct_crc);
  RUN_TEST(missing_invalid_and_unreadable_reads_are_distinct_and_fail_closed);
  return UNITY_END();
}
