#include <unity.h>
#include "GroupTargetIds.h"
void setUp() {}
void tearDown() {}
static void test_duplicate_pending_and_completed_replay_without_new_work() {
  GroupTargetIds ids;
  group_target_reply_t reply;
  TEST_ASSERT_EQUAL(target_id_result_t::fresh, ids.begin(1, 2, 50, reply));
  TEST_ASSERT_EQUAL(target_id_result_t::pending, ids.begin(1, 2, 50, reply));
  TEST_ASSERT_EQUAL(target_id_result_t::conflict, ids.begin(1, 2, 51, reply));
  TEST_ASSERT_EQUAL(target_id_result_t::conflict, ids.begin(1, 3, 50, reply));
  ids.complete(1, {proto_error_t::radioUnavailable, "refused"});
  TEST_ASSERT_EQUAL(target_id_result_t::replay, ids.begin(1, 2, 50, reply));
  TEST_ASSERT_EQUAL(proto_error_t::radioUnavailable, reply.code);
  TEST_ASSERT_EQUAL_STRING("refused", reply.desc);
}
static void test_evicted_ids_cannot_start_again_and_pending_are_not_evicted() {
  GroupTargetIds ids;
  group_target_reply_t reply;
  TEST_ASSERT_EQUAL(target_id_result_t::fresh, ids.begin(1, 1, 10, reply));
  for(uint32_t id = 2; id < 30; id++) {
    TEST_ASSERT_EQUAL(target_id_result_t::fresh, ids.begin(id, 1, 10, reply));
    ids.complete(id, {});
  }
  TEST_ASSERT_EQUAL(target_id_result_t::pending, ids.begin(1, 1, 10, reply));
  TEST_ASSERT_EQUAL(target_id_result_t::stale, ids.begin(2, 1, 10, reply));
  ids.reset(); // new connection
  TEST_ASSERT_EQUAL(target_id_result_t::fresh, ids.begin(1, 1, 10, reply));
}
static void test_full_pending_cache_refuses_new_work_but_allows_retry() {
  GroupTargetIds ids;
  group_target_reply_t reply;
  for(uint32_t id = 1; id <= GroupTargetIds::CAPACITY; id++)
    TEST_ASSERT_EQUAL(target_id_result_t::fresh, ids.begin(id, 1, 10, reply));
  TEST_ASSERT_EQUAL(target_id_result_t::full, ids.begin(99, 1, 10, reply));
  ids.complete(1, {});
  TEST_ASSERT_EQUAL(target_id_result_t::fresh, ids.begin(99, 1, 10, reply));
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_duplicate_pending_and_completed_replay_without_new_work);
  RUN_TEST(test_evicted_ids_cannot_start_again_and_pending_are_not_evicted);
  RUN_TEST(test_full_pending_cache_refuses_new_work_but_allows_retry);
  return UNITY_END();
}
