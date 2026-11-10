#include <unity.h>
#include <nvs.h>
#include "SecurityLoad.h"
#include "ConfigSettings.h"
#include <string.h>
void setUp(){FakeNvs::reset();}void tearDown(){TEST_ASSERT_TRUE(FakeNvs::handles.empty());}
static void seed(int8_t type,const char *user="",const char *password="",const char *pin="") {
 nvs_handle_t h;nvs_open("SEC",NVS_READWRITE,&h);nvs_set_i8(h,"type",type);nvs_set_str(h,"username",user);nvs_set_str(h,"password",password);nvs_set_str(h,"pin",pin);nvs_close(h);
 FakeNvs::opens=FakeNvs::reads=FakeNvs::closes=FakeNvs::sets=FakeNvs::commits=0;
}
static void first_boot_and_explicit_none_are_distinct_from_open_failure() {
 SecurityRecord record;
 TEST_ASSERT_EQUAL(SecurityLoadState::knownEmpty,readSecurityRecord(record));
 TEST_ASSERT_EQUAL(0,FakeNvs::sets);TEST_ASSERT_EQUAL(NVS_READONLY,FakeNvs::lastOpenMode);
 FakeNvs::reset();FakeNvs::failOpen=1;FakeNvs::openError=ESP_ERR_NVS_NOT_FOUND;
 TEST_ASSERT_EQUAL(SecurityLoadState::knownEmpty,readSecurityRecord(record));
 FakeNvs::reset();FakeNvs::failOpen=1;
 TEST_ASSERT_EQUAL(SecurityLoadState::failed,readSecurityRecord(record));
 FakeNvs::reset();seed(0);TEST_ASSERT_EQUAL(SecurityLoadState::loaded,readSecurityRecord(record));
}
static void every_read_error_refuses_without_returning_partial_credentials() {
 seed(2,"user","password");SecurityRecord record;TEST_ASSERT_EQUAL(SecurityLoadState::loaded,readSecurityRecord(record));
 const auto count=FakeNvs::reads;
 for(int i=1;i<=count;++i){FakeNvs::reset();seed(2,"user","password");FakeNvs::failRead=i;strcpy(record.password,"old secret");
  TEST_ASSERT_EQUAL(SecurityLoadState::failed,readSecurityRecord(record));
  const uint8_t wiped[sizeof(record)]={};TEST_ASSERT_EQUAL_MEMORY(wiped,&record,sizeof(record));TEST_ASSERT_EQUAL(0,FakeNvs::sets);
 }
}
static void invalid_active_credentials_cannot_become_blank_login() {
 SecurityRecord record;
 for(int type=1;type<=3;type++){FakeNvs::reset();seed(type);TEST_ASSERT_EQUAL(SecurityLoadState::failed,readSecurityRecord(record));}
 FakeNvs::reset();seed(1,"","","aB!9");TEST_ASSERT_EQUAL(SecurityLoadState::loaded,readSecurityRecord(record));
 FakeNvs::reset();seed(2,"user","password");TEST_ASSERT_EQUAL(SecurityLoadState::loaded,readSecurityRecord(record));
}
static void malformed_or_missing_type_and_text_never_silently_default() {
 SecurityRecord record;seed(2,"user","password");FakeNvs::values.erase("SEC/type");TEST_ASSERT_EQUAL(SecurityLoadState::failed,readSecurityRecord(record));
 FakeNvs::reset();seed(2,"user","password");FakeNvs::values["SEC/type"].type=FakeNvs::Type::u8;TEST_ASSERT_EQUAL(SecurityLoadState::failed,readSecurityRecord(record));
 FakeNvs::reset();seed(2,"user","password");FakeNvs::values["SEC/password"].bytes.assign(34,'x');TEST_ASSERT_EQUAL(SecurityLoadState::failed,readSecurityRecord(record));
 FakeNvs::reset();seed(2,"user","password");FakeNvs::values["SEC/password"].bytes={'p',0,'x',0};TEST_ASSERT_EQUAL(SecurityLoadState::failed,readSecurityRecord(record));
 FakeNvs::reset();seed(0);FakeNvs::values["SEC/pin"].type=FakeNvs::Type::blob;TEST_ASSERT_EQUAL(SecurityLoadState::failed,readSecurityRecord(record));
}
static void actual_security_settings_publish_only_complete_load_and_save_cannot_reset_failure() {
 SecuritySettings settings;TEST_ASSERT_FALSE(settings.loadTrusted());
 TEST_ASSERT_FALSE(settings.save().saved());TEST_ASSERT_EQUAL(0,FakeNvs::opens);
 seed(1,"","","aB!9");TEST_ASSERT_TRUE(settings.begin());TEST_ASSERT_TRUE(settings.loadTrusted());TEST_ASSERT_EQUAL_STRING("aB!9",settings.pin);
 FakeNvs::reads=0;FakeNvs::failRead=1;
 TEST_ASSERT_FALSE(settings.load());TEST_ASSERT_FALSE(settings.loadTrusted());const uint8_t emptyPin[5]={};TEST_ASSERT_EQUAL_MEMORY(emptyPin,settings.pin,5);
 const auto writes=FakeNvs::sets,opens=FakeNvs::opens;
 TEST_ASSERT_FALSE(settings.save().saved());TEST_ASSERT_EQUAL(writes,FakeNvs::sets);TEST_ASSERT_EQUAL(opens,FakeNvs::opens);
 FakeNvs::reset();TEST_ASSERT_TRUE(settings.load());TEST_ASSERT_TRUE(settings.loadTrusted());TEST_ASSERT_EQUAL(SecurityLoadState::knownEmpty,settings.loadState);
}
int main(int,char**){UNITY_BEGIN();RUN_TEST(actual_security_settings_publish_only_complete_load_and_save_cannot_reset_failure);RUN_TEST(first_boot_and_explicit_none_are_distinct_from_open_failure);RUN_TEST(every_read_error_refuses_without_returning_partial_credentials);RUN_TEST(invalid_active_credentials_cannot_become_blank_login);RUN_TEST(malformed_or_missing_type_and_text_never_silently_default);return UNITY_END();}
