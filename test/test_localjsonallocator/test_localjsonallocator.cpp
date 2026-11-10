#include <unity.h>
#include <ArduinoJson.h>
#include "LocalJsonAllocator.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <map>
static std::map<void *,size_t> blocks;
static bool failAllocation=false, allWiped=true;
static void *acquire(size_t size) {
  if(failAllocation) return nullptr;
  void *p=malloc(size); if(p) { memset(p,0xa5,size); blocks[p]=size; } return p;
}
static void release(void *p) {
  const auto size=blocks.at(p);
  for(size_t i=0;i<size;i++) if(static_cast<unsigned char *>(p)[i]) allWiped=false;
  blocks.erase(p); free(p);
}
void setUp() { failAllocation=false; allWiped=true; TEST_ASSERT_TRUE(blocks.empty()); }
void tearDown() {
  const bool empty=blocks.empty();
  for(const auto &block:blocks) free(block.first);
  blocks.clear();
  TEST_ASSERT_TRUE(empty);
}
static void budget_counts_real_storage_and_checks_overflow() {
  LocalUi::JsonAllocator memory(512,acquire,release);
  void *p=memory.allocate(64); TEST_ASSERT_NOT_NULL(p);
  TEST_ASSERT_EQUAL(0,reinterpret_cast<uintptr_t>(p)%alignof(max_align_t));
  TEST_ASSERT_GREATER_THAN(64,memory.used());
  TEST_ASSERT_EQUAL(blocks.begin()->second,memory.used());
  const auto before=memory.used();
  TEST_ASSERT_NULL(memory.allocate(512)); TEST_ASSERT_EQUAL(before,memory.used());
  TEST_ASSERT_NULL(memory.allocate(SIZE_MAX)); TEST_ASSERT_NULL(memory.allocate(0));
  memory.deallocate(p); TEST_ASSERT_EQUAL(0,memory.used()); TEST_ASSERT_TRUE(allWiped);
}
static void resize_preserves_content_wipes_shrink_and_free() {
  LocalUi::JsonAllocator memory(1024,acquire,release);
  auto *p=static_cast<unsigned char *>(memory.allocate(64)); TEST_ASSERT_NOT_NULL(p);
  memset(p,'S',64); const auto allocated=memory.used();
  TEST_ASSERT_EQUAL_PTR(p,memory.reallocate(p,12));
  for(size_t i=0;i<12;i++) TEST_ASSERT_EQUAL('S',p[i]);
  for(size_t i=12;i<64;i++) TEST_ASSERT_EQUAL(0,p[i]);
  TEST_ASSERT_EQUAL(allocated,memory.used());
  auto *grown=static_cast<unsigned char *>(memory.reallocate(p,128)); TEST_ASSERT_NOT_NULL(grown);
  for(size_t i=0;i<12;i++) TEST_ASSERT_EQUAL('S',grown[i]);
  TEST_ASSERT_TRUE(allWiped); TEST_ASSERT_EQUAL(1,blocks.size());
  TEST_ASSERT_NULL(memory.reallocate(grown,0)); TEST_ASSERT_TRUE(allWiped); TEST_ASSERT_EQUAL(0,memory.used());
}
static void failed_growth_keeps_original_owned_and_unchanged() {
  LocalUi::JsonAllocator memory(256,acquire,release);
  auto *p=static_cast<char *>(memory.allocate(120)); TEST_ASSERT_NOT_NULL(p);
  strcpy(p,"private draft"); const auto before=memory.used();
  TEST_ASSERT_NULL(memory.reallocate(p,200));
  TEST_ASSERT_EQUAL_STRING("private draft",p); TEST_ASSERT_EQUAL(before,memory.used());
  memory.deallocate(nullptr); memory.deallocate(p); TEST_ASSERT_TRUE(allWiped);
}
static void backing_failure_and_missing_callbacks_do_not_consume_budget() {
  LocalUi::JsonAllocator memory(1024,acquire,release);
  auto *p=static_cast<char *>(memory.allocate(32)); TEST_ASSERT_NOT_NULL(p);
  strcpy(p,"keep me"); const auto before=memory.used();
  failAllocation=true;
  TEST_ASSERT_NULL(memory.allocate(32)); TEST_ASSERT_EQUAL(before,memory.used());
  TEST_ASSERT_NULL(memory.reallocate(p,64));
  TEST_ASSERT_EQUAL_STRING("keep me",p); TEST_ASSERT_EQUAL(before,memory.used());
  memory.deallocate(p); TEST_ASSERT_TRUE(allWiped);
  LocalUi::JsonAllocator noAcquire(1024,nullptr,release),noRelease(1024,acquire,nullptr);
  TEST_ASSERT_NULL(noAcquire.allocate(32)); TEST_ASSERT_NULL(noRelease.allocate(32));
  TEST_ASSERT_EQUAL(0,noAcquire.used()); TEST_ASSERT_EQUAL(0,noRelease.used());
}
static void actual_json_drafts_are_wiped_and_fail_closed_under_budget() {
  LocalUi::JsonAllocator memory(16384,acquire,release);
  {
    JsonDocument draft(&memory);
    char secret[]="fresh-private-password";
    draft["password"]=secret;
    TEST_ASSERT_FALSE(draft.overflowed()); TEST_ASSERT_GREATER_THAN(0,memory.used());
    TEST_ASSERT_EQUAL_STRING(secret,draft["password"].as<const char *>());
    draft.clear(); TEST_ASSERT_EQUAL(0,memory.used()); TEST_ASSERT_TRUE(allWiped);
  }
  LocalUi::JsonAllocator tiny(64,acquire,release);
  { JsonDocument draft(&tiny); draft["password"]="abc"; TEST_ASSERT_TRUE(draft.overflowed()); }
  TEST_ASSERT_EQUAL(0,tiny.used()); TEST_ASSERT_TRUE(allWiped);
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(budget_counts_real_storage_and_checks_overflow);
  RUN_TEST(resize_preserves_content_wipes_shrink_and_free);
  RUN_TEST(failed_growth_keeps_original_owned_and_unchanged);
  RUN_TEST(backing_failure_and_missing_callbacks_do_not_consume_budget);
  RUN_TEST(actual_json_drafts_are_wiped_and_fail_closed_under_budget);
  return UNITY_END();
}
