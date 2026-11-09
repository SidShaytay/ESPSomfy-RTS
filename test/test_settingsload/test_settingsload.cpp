#include <unity.h>
#include <nvs.h>
#include "SettingsLoad.h"
#include <string.h>
void setUp(){FakeNvs::reset();}void tearDown(){TEST_ASSERT_TRUE(FakeNvs::handles.empty());}
static void seed(){nvs_handle_t h;nvs_open("TEST",NVS_READWRITE,&h);nvs_set_i8(h,"signed",-1);nvs_set_u8(h,"unsigned",80);nvs_set_i16(h,"port",-1);nvs_set_u8(h,"flag",1);nvs_set_str(h,"text","a\n\xff");float x=433.42f;nvs_set_blob(h,"float",&x,sizeof(x));nvs_close(h);FakeNvs::opens=FakeNvs::reads=FakeNvs::closes=FakeNvs::sets=FakeNvs::commits=FakeNvs::readOnlyOpens=FakeNvs::writeOpens=0;}
static void exact_absence_is_not_open_or_read_error(){
 {SettingsLoadReader r("TEST");uint8_t value=8;TEST_ASSERT_EQUAL(SettingsReadField::absent,r.u8("missing",value));TEST_ASSERT_EQUAL(8,value);TEST_ASSERT_EQUAL(SettingsLoadState::knownEmpty,r.status().state);}
 FakeNvs::reset();FakeNvs::failOpen=1;
 {SettingsLoadReader r("TEST");uint8_t value=8;TEST_ASSERT_EQUAL(SettingsReadField::failed,r.u8("missing",value));TEST_ASSERT_FALSE(r.status().readable());TEST_ASSERT_EQUAL(8,value);}
 FakeNvs::reset();FakeNvs::readOnlyNamespaceMissing=true;
 {SettingsLoadReader r("TEST");bool value=true;TEST_ASSERT_EQUAL(SettingsReadField::absent,r.flag("flag",value));TEST_ASSERT_TRUE(r.status().usable());TEST_ASSERT_EQUAL(0,FakeNvs::reads);}
 TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);
}
static void typed_values_and_exact_bytes_are_staged_without_any_write(){
 seed();{SettingsLoadReader r("TEST");int8_t signedValue=0;uint8_t value=0;int16_t port=0;bool flag=false;float number=0;char text[8]={};
 TEST_ASSERT_EQUAL(SettingsReadField::present,r.i8("signed",signedValue));TEST_ASSERT_EQUAL(-1,signedValue);
 TEST_ASSERT_EQUAL(SettingsReadField::present,r.u8("unsigned",value));TEST_ASSERT_EQUAL(80,value);
 TEST_ASSERT_EQUAL(SettingsReadField::present,r.i16("port",port));TEST_ASSERT_EQUAL_UINT16(65535,static_cast<uint16_t>(port));
 TEST_ASSERT_EQUAL(SettingsReadField::present,r.flag("flag",flag));TEST_ASSERT_TRUE(flag);
 TEST_ASSERT_EQUAL(SettingsReadField::present,r.text("text",text));TEST_ASSERT_EQUAL_STRING("a\n\xff",text);
 TEST_ASSERT_EQUAL(SettingsReadField::present,r.real("float",number));TEST_ASSERT_FLOAT_WITHIN(0.001,433.42f,number);
 TEST_ASSERT_EQUAL(SettingsLoadState::loaded,r.status().state);r.invalid();TEST_ASSERT_TRUE(r.status().readable());TEST_ASSERT_FALSE(r.status().usable());}
 TEST_ASSERT_EQUAL(0,FakeNvs::writeOpens);TEST_ASSERT_EQUAL(0,FakeNvs::sets);TEST_ASSERT_EQUAL(0,FakeNvs::commits);
}
static void every_string_read_failure_and_malformed_string_leave_destination_unchanged(){
 for(int phase=0;phase<5;++phase){FakeNvs::reset();seed();
  if(phase<2)FakeNvs::failRead=phase+1;
  if(phase==2)FakeNvs::values["TEST/text"].type=FakeNvs::Type::blob;
  if(phase==3)FakeNvs::values["TEST/text"].bytes={'a',0,'x',0};
  if(phase==4)FakeNvs::values["TEST/text"].bytes.assign(9,'x');
  SettingsLoadReader r("TEST");char text[8]="prior";TEST_ASSERT_EQUAL(SettingsReadField::failed,r.text("text",text));TEST_ASSERT_EQUAL_STRING("prior",text);TEST_ASSERT_FALSE(r.status().readable());
 }
}
static void scalar_type_error_invalid_bool_and_wrong_blob_size_are_unavailable(){
 for(int phase=0;phase<3;++phase){FakeNvs::reset();seed();SettingsLoadReader r("TEST");
  if(phase==0){uint8_t value=9;TEST_ASSERT_EQUAL(SettingsReadField::failed,r.u8("signed",value));TEST_ASSERT_EQUAL(9,value);}
  if(phase==1){FakeNvs::values["TEST/flag"].bytes[0]=2;bool value=false;TEST_ASSERT_EQUAL(SettingsReadField::failed,r.flag("flag",value));TEST_ASSERT_FALSE(value);}
  if(phase==2){FakeNvs::values["TEST/float"].bytes.resize(3);float value=9;TEST_ASSERT_EQUAL(SettingsReadField::failed,r.real("float",value));TEST_ASSERT_EQUAL_FLOAT(9,value);}
  TEST_ASSERT_FALSE(r.status().readable());
 }
}
static void raw_nonfinite_values_are_preserved_and_invalid_status_is_sticky() {
 for(uint32_t bits:{0x7fc01234u,0x7f800000u,0xff800000u}) {
  FakeNvs::reset();seed();
  memcpy(FakeNvs::values["TEST/float"].bytes.data(),&bits,sizeof(bits));
  SettingsLoadReader r("TEST");float value=0;uint32_t actual=0;
  TEST_ASSERT_EQUAL(SettingsReadField::present,r.real("float",value));
  memcpy(&actual,&value,sizeof(actual));TEST_ASSERT_EQUAL_HEX32(bits,actual);
  TEST_ASSERT_TRUE(r.status().usable()); // Semantic validation belongs to the typed namespace.
  r.invalid();uint8_t next=0;
  TEST_ASSERT_EQUAL(SettingsReadField::present,r.u8("unsigned",next));
  TEST_ASSERT_EQUAL(SettingsLoadState::invalid,r.status().state);
  TEST_ASSERT_EQUAL(SettingsReadField::absent,r.u8("missing",next));
  TEST_ASSERT_EQUAL(SettingsLoadState::invalid,r.status().state);
  TEST_ASSERT_EQUAL(SettingsReadField::failed,r.u8("signed",next));
  r.invalid();TEST_ASSERT_EQUAL(SettingsLoadState::failed,r.status().state);
 }
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(raw_nonfinite_values_are_preserved_and_invalid_status_is_sticky);RUN_TEST(exact_absence_is_not_open_or_read_error);RUN_TEST(typed_values_and_exact_bytes_are_staged_without_any_write);RUN_TEST(every_string_read_failure_and_malformed_string_leave_destination_unchanged);RUN_TEST(scalar_type_error_invalid_bool_and_wrong_blob_size_are_unavailable);return UNITY_END();}
