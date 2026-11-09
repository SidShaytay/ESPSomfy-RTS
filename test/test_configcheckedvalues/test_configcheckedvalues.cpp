#include <unity.h>
#include <string>
#include <math.h>
#include "ConfigRecords.h"
struct Input:ConfigReader {std::string bytes;size_t at=0;explicit Input(const char *s):bytes(s){}bool readByte(uint8_t &v)override{if(at==bytes.size())return false;v=uint8_t(bytes[at++]);return true;}uint32_t position()override{return at;}};
void setUp(){}void tearDown(){}
static void checked_numeric_values_never_wrap_truncate_or_accept_junk() {
 for(const char *text:{"257,","-1,","1junk,","999999999999999999999,","1\n"}){
  Input in(text);in.beginCheckedValues();const auto value=in.readUInt8(17);const auto status=in.endCheckedValues();
  if(std::string(text)=="1\n"){TEST_ASSERT_TRUE(status.valid);TEST_ASSERT_EQUAL(1,value);}
  else{TEST_ASSERT_FALSE(status.valid);TEST_ASSERT_EQUAL(17,value);}
 }
 for(const char *text:{"32768,","-32769,","3.0,","0x10,"}){Input in(text);in.beginCheckedValues();in.readInt8(0);TEST_ASSERT_FALSE(in.endCheckedValues().valid);}
 Input absentEnd("1,");absentEnd.beginCheckedValues();absentEnd.readUInt8();absentEnd.endRecord();TEST_ASSERT_FALSE(absentEnd.endCheckedValues().valid);
 Input max(" 65535,");max.beginCheckedValues();TEST_ASSERT_EQUAL_UINT16(65535,max.readUInt16());TEST_ASSERT_TRUE(max.endCheckedValues().valid);
}
static void checked_boolean_and_float_values_require_complete_canonical_tokens(){
 for(const char *text:{"2,","TRUEjunk,","t,",","}){Input in(text);in.beginCheckedValues();in.readBool();TEST_ASSERT_FALSE(in.endCheckedValues().valid);}
 for(const char *text:{"true,","false,","1,","0,"}){Input in(text);in.beginCheckedValues();in.readBool();TEST_ASSERT_TRUE(in.endCheckedValues().valid);}
 for(const char *text:{"nan,","inf,","433.42junk,","1e999,",","}){Input in(text);in.beginCheckedValues();in.readFloat();TEST_ASSERT_FALSE(in.endCheckedValues().valid);}
 Input good(" 433.420,");good.beginCheckedValues();TEST_ASSERT_FLOAT_WITHIN(0.001f,433.42f,good.readFloat());TEST_ASSERT_TRUE(good.endCheckedValues().valid);
}
static void checked_text_preserves_exact_bytes_and_refuses_truncation_or_unterminated_values(){
 char text[5];Input exact("\"ab  \",");exact.beginCheckedValues();TEST_ASSERT_TRUE(exact.readVarString(text,sizeof(text)));auto result=exact.endCheckedValues();TEST_ASSERT_TRUE(result.valid);TEST_ASSERT_EQUAL_STRING("ab  ",text);TEST_ASSERT_EQUAL(1,result.fields);
 for(const char *raw:{"\"abcde\",","\"abc\n","unquoted,","\"abc\"junk,","\"abc"}){Input in(raw);in.beginCheckedValues();in.readVarString(text,sizeof(text));TEST_ASSERT_FALSE(in.endCheckedValues().valid);}
 Input nul("\"x\", ");nul.bytes=std::string("\"a\0b\",",6);nul.beginCheckedValues();nul.readVarString(text,sizeof(text));TEST_ASSERT_FALSE(nul.endCheckedValues().valid);
}
static void checked_field_count_distinguishes_missing_from_explicit_defaults_and_restores_legacy_mode(){
 Input in("1\n257,");in.beginCheckedValues();in.readUInt8();in.readUInt8();auto state=in.endCheckedValues();TEST_ASSERT_TRUE(state.valid);TEST_ASSERT_EQUAL(1,state.fields);
 in.beginRecord();TEST_ASSERT_EQUAL_UINT8(1,in.readUInt8()); // Legacy conversion remains unchanged outside checked mode.
}
int main(){UNITY_BEGIN();RUN_TEST(checked_numeric_values_never_wrap_truncate_or_accept_junk);RUN_TEST(checked_boolean_and_float_values_require_complete_canonical_tokens);RUN_TEST(checked_text_preserves_exact_bytes_and_refuses_truncation_or_unterminated_values);RUN_TEST(checked_field_count_distinguishes_missing_from_explicit_defaults_and_restores_legacy_mode);return UNITY_END();}
