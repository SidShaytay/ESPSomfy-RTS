// Transport fault tests are included by test_gitupdate.cpp, sharing its runner.
#include "GitUpdateTransfer.h"
using namespace GitUpdateTransfer;
static void body_rejects_missing_and_oversized_lengths() {
  TEST_ASSERT_FALSE(valid({64,0,false,false}));
  TEST_ASSERT_FALSE(valid({64,65,false,false}));
  TEST_ASSERT_FALSE(valid({64,-1,false,false}));
  TEST_ASSERT_FALSE(valid({64,0,true,false}));
  TEST_ASSERT_TRUE(valid({64,0,true,true}));
}
static void body_accepts_partial_reads_and_requires_clean_completion() {
  uint8_t out[8]{};size_t received=0;unsigned reads=0;
  const auto read=[&](uint8_t *dest,size_t n) { reads++;dest[0]=42;return int(n>1 ? 2 : 1); };
  const auto stop=[] { return false; };
  const auto progress=[](size_t,size_t) {};
  TEST_ASSERT_EQUAL_INT(int(Result::Complete),int(readBody({8,8,false,false},out,received,read,[] { return true; },stop,progress)));
  TEST_ASSERT_EQUAL_UINT(8,received);TEST_ASSERT_EQUAL_UINT(4,reads);
  received=0;
  TEST_ASSERT_EQUAL_INT(int(Result::Truncated),int(readBody({8,8,false,false},out,received,read,[] { return false; },stop,progress)));
}
static void body_stalls_cancellation_and_chunked_overflow_fail_closed() {
  uint8_t out[8]{};size_t received=0;
  const auto progress=[](size_t,size_t) {};
  TEST_ASSERT_EQUAL_INT(int(Result::Truncated),int(readBody({8,8,false,false},out,received,[](uint8_t *,size_t) { return 0; },[] { return false; },[] { return false; },progress)));
  unsigned calls=0;
  TEST_ASSERT_EQUAL_INT(int(Result::Stopped),int(readBody({8,8,false,false},out,received,[](uint8_t *,size_t) { return 1; },[] { return false; },[&] { return calls++>1; },progress)));
  received=0;
  TEST_ASSERT_EQUAL_INT(int(Result::Overflow),int(readBody({8,0,true,true},out,received,[](uint8_t *,size_t n) { return int(n); },[] { return false; },[] { return false; },progress)));
}
static void chunked_metadata_requires_real_end_and_enforces_capacity() {
  uint8_t out[8]{};size_t received=0;bool ended=false;
  const auto progress=[](size_t,size_t) {};
  const auto read=[&](uint8_t *,size_t) { if(received==3) { ended=true;return 0; } return 1; };
  TEST_ASSERT_EQUAL_INT(int(Result::Complete),int(readBody({8,0,true,true},out,received,read,[&] { return ended; },[] { return false; },progress)));
  TEST_ASSERT_EQUAL_UINT(3,received);
}
