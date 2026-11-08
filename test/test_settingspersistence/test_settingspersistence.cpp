#include <unity.h>
#include <nvs.h>
#include "ConfigSettings.h"
#include "SettingsDurability.h"
#include "Transceiver.h"
void setUp() { FakeNvs::reset();settingsDurability()=SettingsDurability{}; }
void tearDown() { TEST_ASSERT_TRUE(FakeNvs::handles.empty()); }
static void test_open_failure_is_not_saved() {
  FakeNvs::failOpen=1;
  WifiSettings cfg;
  TEST_ASSERT_EQUAL(settings_save_state_t::failed,cfg.save().state);
  TEST_ASSERT_EQUAL(0,FakeNvs::sets);
}
static void test_first_write_failure_is_not_saved() {
  FakeNvs::failSet=1;
  WifiSettings cfg;
  TEST_ASSERT_EQUAL(settings_save_state_t::failed,cfg.save().state);
}
static void test_commit_failure_is_partial_even_for_empty_string() {
  FakeNvs::failCommit=1;
  WifiSettings cfg;
  TEST_ASSERT_EQUAL(settings_save_state_t::partial,cfg.save().state);
}
static void test_readback_failure_is_partial() {
  FakeNvs::failRead=1;
  WifiSettings cfg;
  TEST_ASSERT_EQUAL(settings_save_state_t::partial,cfg.save().state);
}
static void test_empty_strings_are_verified_and_saved() {
  WifiSettings cfg;
  TEST_ASSERT_TRUE(cfg.save().saved());
  TEST_ASSERT_EQUAL(4,FakeNvs::reads);
  TEST_ASSERT_EQUAL(0,FakeNvs::values.at("WIFI/passphrase").bytes[0]);
}
template<class Settings>
static void inject_every_operation(Settings cfg) {
  FakeNvs::reset();
  TEST_ASSERT_TRUE(cfg.save().saved());
  const int keyCount=FakeNvs::sets;
  TEST_ASSERT_GREATER_THAN(0,keyCount);
  TEST_ASSERT_EQUAL(keyCount,FakeNvs::commits);
  TEST_ASSERT_EQUAL(keyCount,FakeNvs::reads);
  for(int key=1;key<=keyCount;key++) {
    for(int phase=0;phase<4;phase++) {
      FakeNvs::reset();
      if(phase==0) FakeNvs::failSet=key;
      if(phase==1) FakeNvs::failCommit=key;
      if(phase==2) FakeNvs::failRead=key;
      if(phase==3) FakeNvs::corruptRead=key;
      auto result=cfg.save();
      TEST_ASSERT_EQUAL(phase==0 && key==1 ? settings_save_state_t::failed : settings_save_state_t::partial,result.state);
      TEST_ASSERT_NOT_NULL(result.failedKey);
      TEST_ASSERT_EQUAL(key,FakeNvs::sets); // no further writes after the failure
      TEST_ASSERT_TRUE(FakeNvs::handles.empty());
    }
  }
  FakeNvs::reset();
  FakeNvs::failOpen=1;
  TEST_ASSERT_EQUAL(settings_save_state_t::failed,cfg.save().state);
  TEST_ASSERT_EQUAL(0,FakeNvs::sets);
}
static void test_every_save_checks_each_set_commit_and_readback() {
  inject_every_operation(ConfigSettings{});
  inject_every_operation(NTPSettings{});
  inject_every_operation(IPSettings{});
  inject_every_operation(SecuritySettings{});
  inject_every_operation(WifiSettings{});
  inject_every_operation(MQTTSettings{});
  inject_every_operation(transceiver_config_t{});
#if HAS_ETHERNET
  inject_every_operation(EthernetSettings{});
#endif
}
static void test_failed_write_retains_existing_namespace_keys() {
  SecuritySettings original;
  strcpy(original.password,"old password");
  strcpy(original.pin,"1234");
  TEST_ASSERT_TRUE(original.save().saved());
  auto originalPassword=FakeNvs::values.at("SEC/password").bytes;
  FakeNvs::failSet=FakeNvs::sets+1;
  SecuritySettings replacement;
  strcpy(replacement.password,"new password");
  TEST_ASSERT_FALSE(replacement.save().saved());
  TEST_ASSERT_TRUE(originalPassword==FakeNvs::values.at("SEC/password").bytes);
  TEST_ASSERT_TRUE(FakeNvs::values.count("SEC/pin"));
}
static void test_partial_write_reports_actual_remaining_durable_values() {
  WifiSettings original;
  strcpy(original.ssid,"old network");
  strcpy(original.passphrase,"old passphrase");
  TEST_ASSERT_TRUE(original.save().saved());
  FakeNvs::failSet=FakeNvs::sets+2;
  WifiSettings replacement;
  strcpy(replacement.ssid,"new network");
  auto result=replacement.save();
  TEST_ASSERT_EQUAL(settings_save_state_t::partial,result.state);
  TEST_ASSERT_EQUAL_STRING("passphrase",result.failedKey);
  TEST_ASSERT_EQUAL_STRING("new network",reinterpret_cast<const char *>(FakeNvs::values.at("WIFI/ssid").bytes.data()));
  TEST_ASSERT_EQUAL_STRING("old passphrase",reinterpret_cast<const char *>(FakeNvs::values.at("WIFI/passphrase").bytes.data()));
}
static void test_existing_key_types_are_preserved() {
  MQTTSettings mqtt;
  mqtt.port=65535;
  TEST_ASSERT_TRUE(mqtt.save().saved());
  TEST_ASSERT_EQUAL(FakeNvs::Type::i16,FakeNvs::values.at("MQTT/port").type);
  int16_t stored;
  memcpy(&stored,FakeNvs::values.at("MQTT/port").bytes.data(),sizeof(stored));
  TEST_ASSERT_EQUAL_UINT16(65535,static_cast<uint16_t>(stored));
  SecuritySettings security;
  TEST_ASSERT_TRUE(security.save().saved());
  TEST_ASSERT_EQUAL(FakeNvs::Type::i8,FakeNvs::values.at("SEC/type").type);
  transceiver_config_t radio;
  TEST_ASSERT_TRUE(radio.save().saved());
  TEST_ASSERT_EQUAL(FakeNvs::Type::u8,FakeNvs::values.at("CC1101/type").type);
  TEST_ASSERT_EQUAL(FakeNvs::Type::blob,FakeNvs::values.at("CC1101/frequency").type);
}
static void test_partial_write_blocks_restart_until_that_full_namespace_is_verified() {
  WifiSettings wifi;
  FakeNvs::failCommit=1;
  TEST_ASSERT_FALSE(wifi.save().saved());
  TEST_ASSERT_FALSE(settingsDurability().status().restartAllowed());
  TEST_ASSERT_EQUAL_UINT8(settingsNamespaceBit(SettingsNamespace::wifi),settingsDurability().status().uncertain);
  FakeNvs::failCommit=0;
  MQTTSettings mqtt;TEST_ASSERT_TRUE(mqtt.save().saved());
  TEST_ASSERT_FALSE(settingsDurability().status().restartAllowed());
  TEST_ASSERT_TRUE(wifi.save().saved());
  TEST_ASSERT_TRUE(settingsDurability().status().restartAllowed());
}
static void legacy_migration_preserves_source_on_first_set_and_partial_failures() {
  for(int failure=0;failure<2;failure++) {
    FakeNvs::reset();settingsDurability()=SettingsDurability{};
    nvs_handle_t h;TEST_ASSERT_EQUAL(ESP_OK,nvs_open("WIFI",NVS_READWRITE,&h));
    TEST_ASSERT_EQUAL(ESP_OK,nvs_set_str(h,"hostname","legacy running"));
    TEST_ASSERT_EQUAL(ESP_OK,nvs_set_u8(h,"ssdpBroadcast",1));nvs_close(h);
    const auto source=FakeNvs::values;
    if(failure==0) FakeNvs::failSet=FakeNvs::sets+1;
    else FakeNvs::failCommit=FakeNvs::commits+1;
    ConfigSettings current;strcpy(current.hostname,"legacy running");current.connType=conn_types_t::wifi;
    const auto result=current.saveLegacyMigration();
    TEST_ASSERT_FALSE(result.saved());TEST_ASSERT_EQUAL(0,FakeNvs::erases);
    TEST_ASSERT_TRUE(FakeNvs::values.at("WIFI/hostname").bytes==source.at("WIFI/hostname").bytes);
    TEST_ASSERT_TRUE(FakeNvs::values.at("WIFI/ssdpBroadcast").bytes==source.at("WIFI/ssdpBroadcast").bytes);
    TEST_ASSERT_EQUAL(failure==0,settingsDurability().status().restartAllowed());
  }
}
static void legacy_cleanup_follows_verified_cfg_and_preserves_active_wifi_keys() {
  nvs_handle_t h;TEST_ASSERT_EQUAL(ESP_OK,nvs_open("WIFI",NVS_READWRITE,&h));
  TEST_ASSERT_EQUAL(ESP_OK,nvs_set_str(h,"hostname","legacy running"));
  TEST_ASSERT_EQUAL(ESP_OK,nvs_set_u8(h,"ssdpBroadcast",1));
  TEST_ASSERT_EQUAL(ESP_OK,nvs_set_str(h,"ssid","active network"));nvs_close(h);
  ConfigSettings current;strcpy(current.hostname,"legacy running");current.connType=conn_types_t::wifi;
  const auto migrated=current.saveLegacyMigration();
  TEST_ASSERT_TRUE(migrated.saved());TEST_ASSERT_TRUE(migrated.sourceCleaned);
  TEST_ASSERT_FALSE(FakeNvs::values.count("WIFI/hostname"));TEST_ASSERT_FALSE(FakeNvs::values.count("WIFI/ssdpBroadcast"));
  TEST_ASSERT_EQUAL_STRING("active network",reinterpret_cast<const char *>(FakeNvs::values.at("WIFI/ssid").bytes.data()));
  TEST_ASSERT_EQUAL_STRING("legacy running",reinterpret_cast<const char *>(FakeNvs::values.at("CFG/hostname").bytes.data()));
  TEST_ASSERT_TRUE(settingsDurability().status().restartAllowed());
}
static void optional_legacy_cleanup_failure_does_not_invalidate_verified_cfg() {
  for(int failure=0;failure<2;failure++) {
    FakeNvs::reset();settingsDurability()=SettingsDurability{};
    nvs_handle_t h;TEST_ASSERT_EQUAL(ESP_OK,nvs_open("WIFI",NVS_READWRITE,&h));
    TEST_ASSERT_EQUAL(ESP_OK,nvs_set_str(h,"hostname","legacy"));nvs_close(h);
    if(failure==0) FakeNvs::failOpen=FakeNvs::opens+2;
    else FakeNvs::failCommit=FakeNvs::commits+5;
    ConfigSettings current;current.connType=conn_types_t::wifi;
    const auto result=current.saveLegacyMigration();
    TEST_ASSERT_TRUE(result.saved());TEST_ASSERT_FALSE(result.sourceCleaned);
    TEST_ASSERT_TRUE(settingsDurability().status().restartAllowed());
    TEST_ASSERT_TRUE(FakeNvs::values.count("CFG/connType"));
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(optional_legacy_cleanup_failure_does_not_invalidate_verified_cfg);
  RUN_TEST(legacy_migration_preserves_source_on_first_set_and_partial_failures);
  RUN_TEST(legacy_cleanup_follows_verified_cfg_and_preserves_active_wifi_keys);
  RUN_TEST(test_partial_write_blocks_restart_until_that_full_namespace_is_verified);
  RUN_TEST(test_open_failure_is_not_saved);
  RUN_TEST(test_first_write_failure_is_not_saved);
  RUN_TEST(test_commit_failure_is_partial_even_for_empty_string);
  RUN_TEST(test_readback_failure_is_partial);
  RUN_TEST(test_empty_strings_are_verified_and_saved);
  RUN_TEST(test_every_save_checks_each_set_commit_and_readback);
  RUN_TEST(test_failed_write_retains_existing_namespace_keys);
  RUN_TEST(test_partial_write_reports_actual_remaining_durable_values);
  RUN_TEST(test_existing_key_types_are_preserved);
  return UNITY_END();
}
