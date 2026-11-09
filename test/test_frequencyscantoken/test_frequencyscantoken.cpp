#include "FrequencyScanToken.h"
#include <unity.h>
#include <string.h>
#include <initializer_list>
void setUp(){}void tearDown(){}
static void exact_boot_and_owner_identity_round_trips_without_numeric_rounding(){
 for(const auto token:{FrequencyScanToken{1,1},FrequencyScanToken{9007199254740993ULL,9007199254740995ULL},FrequencyScanToken{UINT64_MAX,UINT64_MAX}}){
  char text[42];TEST_ASSERT_TRUE(formatFrequencyScanToken(token,text,sizeof(text)));
  FrequencyScanToken parsed;TEST_ASSERT_TRUE(parseFrequencyScanToken(text,strlen(text),parsed));
  TEST_ASSERT_TRUE(token.same(parsed));
 }
}
static void malformed_and_stale_identity_is_not_replaced_with_a_default(){
 for(const char *text:{"","0.1","1.0","01.1","1.01","+1.2","1.-2","1.2.3","1", ".2","1.","1.2 "," 1.2","18446744073709551616.1","1.18446744073709551616"}){
  FrequencyScanToken token{9,10};TEST_ASSERT_FALSE(parseFrequencyScanToken(text,strlen(text),token));
  TEST_ASSERT_EQUAL_UINT64(9,token.bootSession);TEST_ASSERT_EQUAL_UINT64(10,token.generation);
 }
 const char embedded[]={'1','.','2',0,'.','3'};FrequencyScanToken unchanged{9,10};
 TEST_ASSERT_FALSE(parseFrequencyScanToken(embedded,sizeof(embedded),unchanged));TEST_ASSERT_EQUAL_UINT64(9,unchanged.bootSession);
 FrequencyScanToken value{1,2};TEST_ASSERT_FALSE(parseFrequencyScanToken(nullptr,3,value));
 TEST_ASSERT_FALSE(value.same({2,2}));TEST_ASSERT_FALSE(value.same({1,3}));TEST_ASSERT_FALSE(FrequencyScanToken{}.same({}));
}
static void bounded_formatting_never_returns_a_truncated_capability(){
 char out[42];memset(out,'x',sizeof(out));
 TEST_ASSERT_FALSE(formatFrequencyScanToken({0,2},out,sizeof(out)));TEST_ASSERT_EQUAL(0,out[0]);
 TEST_ASSERT_FALSE(formatFrequencyScanToken({1,2},out,3));TEST_ASSERT_EQUAL(0,out[0]);
 TEST_ASSERT_TRUE(formatFrequencyScanToken({1,2},out,4));TEST_ASSERT_EQUAL_STRING("1.2",out);
 TEST_ASSERT_FALSE(formatFrequencyScanToken({1,2},nullptr,42));
}
int main(){UNITY_BEGIN();RUN_TEST(exact_boot_and_owner_identity_round_trips_without_numeric_rounding);RUN_TEST(malformed_and_stale_identity_is_not_replaced_with_a_default);RUN_TEST(bounded_formatting_never_returns_a_truncated_capability);return UNITY_END();}
