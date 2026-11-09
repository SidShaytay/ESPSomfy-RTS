#include "LivePinClaims.h"
#include <thread>
#include <unity.h>
void setUp(){}void tearDown(){}
static void claims_are_monotonic_and_none_cannot_own_a_pad(){
 LivePinClaims claims;TEST_ASSERT_EQUAL_UINT64(0,claims.snapshot());claims.retain(LivePinClaims::pinMask(17));claims.retain(LivePinClaims::pinMask(255));claims.retain(LivePinClaims::pinMask(0)|LivePinClaims::pinMask(63));
 TEST_ASSERT_TRUE(claims.usesPin(0));TEST_ASSERT_TRUE(claims.usesPin(17));TEST_ASSERT_TRUE(claims.usesPin(63));TEST_ASSERT_FALSE(claims.usesPin(255));TEST_ASSERT_FALSE(claims.usesPin(64));
 claims.retain(0);TEST_ASSERT_EQUAL_UINT64((uint64_t(1)<<63)|(uint64_t(1)<<17)|1,claims.snapshot());
}
static void concurrent_low_and_high_claims_never_lose_updates_or_publish_torn_snapshots(){
 LivePinClaims claims;
 std::thread low([&]{for(unsigned pass=0;pass<1000;++pass)for(uint8_t pin=0;pin<32;++pin)claims.retain(LivePinClaims::pinMask(pin));});
 std::thread high([&]{for(unsigned pass=0;pass<1000;++pass)for(uint8_t pin=32;pin<64;++pin)claims.retain(LivePinClaims::pinMask(pin));});
 uint64_t prior=0;bool monotonic=true;for(unsigned read=0;read<10000;++read){const auto next=claims.snapshot();monotonic&=(prior&next)==prior;prior=next;}
 low.join();high.join();TEST_ASSERT_TRUE(monotonic);TEST_ASSERT_EQUAL_UINT64(UINT64_MAX,claims.snapshot());
}
int main(){UNITY_BEGIN();RUN_TEST(claims_are_monotonic_and_none_cannot_own_a_pad);RUN_TEST(concurrent_low_and_high_claims_never_lose_updates_or_publish_torn_snapshots);return UNITY_END();}
