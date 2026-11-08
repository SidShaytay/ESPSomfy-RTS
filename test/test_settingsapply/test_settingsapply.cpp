#include <unity.h>
#include <nvs.h>
#include "SettingsApply.h"

class Effects : public SettingsEffects {
public:
  bool apClients=false;
  int hostnames=0,times=0,mqtt=0,radio=0;
  settings_save_result_t radioResult;
  bool wifiClientsPresent() const override { return apClients; }
  void hostnameChanged() override { hostnames++; }
  void timeChanged() override { times++; }
  void mqttChanged() override { mqtt++; }
  settings_save_result_t saveRadio(JsonObject) override { radio++; return radioResult; }
};
void setUp() { FakeNvs::reset(); }
void tearDown() { TEST_ASSERT_TRUE(FakeNvs::handles.empty()); }
static settings_apply_result_t apply(ConfigSettings &settings, settings_section_t section, const char *fields, Effects &effects) {
  JsonDocument request;
  TEST_ASSERT_EQUAL(DeserializationError::Ok,deserializeJson(request,fields).code());
  return applySettings(settings,section,request.as<JsonObject>(),effects);
}
static void test_connection_change_reboots_only_after_verified_save() {
  ConfigSettings settings;
  settings.connType=conn_types_t::wifi;
  Effects effects;
  FakeNvs::failOpen=1;
  auto failed=apply(settings,settings_section_t::network,R"({"connType":2})",effects);
  TEST_ASSERT_FALSE(failed.saved());
  TEST_ASSERT_FALSE(failed.reboot);
  TEST_ASSERT_EQUAL(conn_types_t::wifi,settings.connType);
  FakeNvs::failOpen=0;
  auto saved=apply(settings,settings_section_t::network,R"({"connType":2})",effects);
  TEST_ASSERT_TRUE(saved.saved());
  TEST_ASSERT_TRUE(saved.reboot);
  TEST_ASSERT_EQUAL(conn_types_t::ethernet,settings.connType);
}
static void test_partial_multisection_save_does_not_apply_runtime_changes() {
  ConfigSettings settings;
  strcpy(settings.hostname,"before");
  strcpy(settings.NTP.ntpServer,"old.clock");
  Effects effects;
  FakeNvs::failOpen=2; // CFG complete, NTP cannot open
  auto result=apply(settings,settings_section_t::general,R"({"hostname":"after","ntpServer":"new.clock"})",effects);
  TEST_ASSERT_EQUAL(settings_save_state_t::partial,result.persistence.state);
  TEST_ASSERT_EQUAL_STRING("before",settings.hostname);
  TEST_ASSERT_EQUAL_STRING("old.clock",settings.NTP.ntpServer);
  TEST_ASSERT_EQUAL(0,effects.hostnames+effects.times+effects.mqtt);
  TEST_ASSERT_FALSE(result.reboot);
  TEST_ASSERT_TRUE(result.persistedParts & (1u<<static_cast<uint8_t>(settings_part_t::general)));
  TEST_ASSERT_EQUAL_STRING("after",reinterpret_cast<const char *>(FakeNvs::values.at("CFG/hostname").bytes.data()));
  TEST_ASSERT_NOT_NULL(strstr(settingsSaveFailureDescription(result.persistence.state),"partly persisted"));
}
static void test_success_applies_requested_sections_once_without_cross_section_aliases() {
  ConfigSettings settings;
  strcpy(settings.hostname,"device");
  strcpy(settings.MQTT.hostname,"broker");
  Effects effects;
  auto result=apply(settings,settings_section_t::general,R"({"hostname":"new-device","ntpServer":"new.clock"})",effects);
  TEST_ASSERT_TRUE(result.saved());
  TEST_ASSERT_EQUAL_STRING("new-device",settings.hostname);
  TEST_ASSERT_EQUAL_STRING("broker",settings.MQTT.hostname);
  TEST_ASSERT_EQUAL_STRING("new.clock",settings.NTP.ntpServer);
  TEST_ASSERT_EQUAL(1,effects.hostnames);
  TEST_ASSERT_EQUAL(1,effects.times);
  TEST_ASSERT_EQUAL(0,effects.mqtt);
}
static void test_wifi_retains_blank_secret_for_same_ssid_and_clears_for_changed_ssid() {
  ConfigSettings settings;
  settings.connType=conn_types_t::wifi;
  strcpy(settings.WIFI.ssid,"old network");
  strcpy(settings.WIFI.passphrase,"saved secret");
  Effects effects;
  auto same=apply(settings,settings_section_t::network,R"({"ssid":"old network","passphrase":""})",effects);
  TEST_ASSERT_TRUE(same.saved());
  TEST_ASSERT_FALSE(same.reboot);
  TEST_ASSERT_EQUAL_STRING("saved secret",settings.WIFI.passphrase);
  auto changed=apply(settings,settings_section_t::network,R"({"ssid":"new open network","passphrase":""})",effects);
  TEST_ASSERT_TRUE(changed.saved());
  TEST_ASSERT_TRUE(changed.reboot);
  TEST_ASSERT_EQUAL_STRING("",settings.WIFI.passphrase);
  effects.apClients=true;
  auto onAP=apply(settings,settings_section_t::network,R"({"ssid":"another network","passphrase":"new"})",effects);
  TEST_ASSERT_TRUE(onAP.saved());
  TEST_ASSERT_FALSE(onAP.reboot);
}
static void test_mqtt_failure_never_disconnects_or_changes_live_secret() {
  ConfigSettings settings;
  strcpy(settings.MQTT.username,"user");
  strcpy(settings.MQTT.password,"before");
  Effects effects;
  FakeNvs::failCommit=5; // password may now be durable, but running RAM stays old
  auto result=apply(settings,settings_section_t::mqtt,R"({"password":"after"})",effects);
  TEST_ASSERT_EQUAL(settings_save_state_t::partial,result.persistence.state);
  TEST_ASSERT_EQUAL_STRING("before",settings.MQTT.password);
  TEST_ASSERT_EQUAL(0,effects.mqtt);
  TEST_ASSERT_FALSE(result.reboot);
  FakeNvs::failCommit=0;
  auto saved=apply(settings,settings_section_t::mqtt,R"({"password":"after"})",effects);
  TEST_ASSERT_TRUE(saved.saved());
  TEST_ASSERT_EQUAL_STRING("after",settings.MQTT.password);
  TEST_ASSERT_EQUAL(1,effects.mqtt);
}
static void test_mqtt_and_security_preserve_existing_secret_semantics() {
  ConfigSettings settings;
  strcpy(settings.MQTT.username,"user");
  strcpy(settings.MQTT.password,"mqtt secret");
  strcpy(settings.Security.username,"admin");
  strcpy(settings.Security.password,"login secret");
  strcpy(settings.Security.pin,"1234");
  Effects effects;
  TEST_ASSERT_TRUE(apply(settings,settings_section_t::mqtt,R"({"username":"user","password":""})",effects).saved());
  TEST_ASSERT_EQUAL_STRING("mqtt secret",settings.MQTT.password);
  TEST_ASSERT_TRUE(apply(settings,settings_section_t::mqtt,R"({"username":"new-user","password":""})",effects).saved());
  TEST_ASSERT_EQUAL_STRING("",settings.MQTT.password);
  TEST_ASSERT_TRUE(apply(settings,settings_section_t::security,R"({"type":2,"password":"","pin":""})",effects).saved());
  TEST_ASSERT_EQUAL_STRING("login secret",settings.Security.password);
  TEST_ASSERT_EQUAL_STRING("1234",settings.Security.pin);
  TEST_ASSERT_EQUAL(security_types::Password,settings.Security.type);
}
static void test_security_failure_keeps_live_authentication_unchanged() {
  ConfigSettings settings;
  settings.Security.type=security_types::Password;
  strcpy(settings.Security.password,"before");
  Effects effects;
  FakeNvs::failSet=2;
  auto result=apply(settings,settings_section_t::security,R"({"type":0,"password":"after"})",effects);
  TEST_ASSERT_FALSE(result.saved());
  TEST_ASSERT_EQUAL(security_types::Password,settings.Security.type);
  TEST_ASSERT_EQUAL_STRING("before",settings.Security.password);
}
static void test_radio_result_is_forwarded_without_other_effects() {
  ConfigSettings settings;
  Effects effects;
  effects.radioResult={settings_save_state_t::partial,"frequency"};
  auto result=apply(settings,settings_section_t::radio,R"({"frequency":433.42})",effects);
  TEST_ASSERT_EQUAL(settings_save_state_t::partial,result.persistence.state);
  TEST_ASSERT_FALSE(result.reboot);
  TEST_ASSERT_EQUAL(0,result.persistedParts);
  TEST_ASSERT_EQUAL(1,effects.radio);
  TEST_ASSERT_EQUAL(0,effects.hostnames+effects.times+effects.mqtt);
}
static void test_empty_request_fails_without_storage_or_effects() {
  ConfigSettings settings;
  Effects effects;
  auto result=apply(settings,settings_section_t::general,"{}",effects);
  TEST_ASSERT_FALSE(result.saved());
  TEST_ASSERT_EQUAL(0,FakeNvs::opens);
  TEST_ASSERT_EQUAL(0,effects.hostnames+effects.times+effects.mqtt+effects.radio);
}
#if HAS_ETHERNET
static void test_ethernet_failure_keeps_live_pins_and_no_restart() {
  ConfigSettings settings;
  settings.connType=conn_types_t::ethernet;
  const auto oldPin=settings.Ethernet.MDCPin;
  Effects effects;
  FakeNvs::failSet=3;
  auto result=apply(settings,settings_section_t::network,R"({"MDCPin":19})",effects);
  TEST_ASSERT_FALSE(result.saved());
  TEST_ASSERT_FALSE(result.reboot);
  TEST_ASSERT_EQUAL(oldPin,settings.Ethernet.MDCPin);
  FakeNvs::failSet=0;
  auto saved=apply(settings,settings_section_t::network,R"({"MDCPin":19})",effects);
  TEST_ASSERT_TRUE(saved.saved());
  TEST_ASSERT_TRUE(saved.reboot);
  TEST_ASSERT_EQUAL(19,settings.Ethernet.MDCPin);
}
#endif
int main() {
  UNITY_BEGIN();
  RUN_TEST(test_connection_change_reboots_only_after_verified_save);
  RUN_TEST(test_partial_multisection_save_does_not_apply_runtime_changes);
  RUN_TEST(test_success_applies_requested_sections_once_without_cross_section_aliases);
  RUN_TEST(test_wifi_retains_blank_secret_for_same_ssid_and_clears_for_changed_ssid);
  RUN_TEST(test_mqtt_failure_never_disconnects_or_changes_live_secret);
  RUN_TEST(test_mqtt_and_security_preserve_existing_secret_semantics);
  RUN_TEST(test_security_failure_keeps_live_authentication_unchanged);
  RUN_TEST(test_radio_result_is_forwarded_without_other_effects);
  RUN_TEST(test_empty_request_fails_without_storage_or_effects);
#if HAS_ETHERNET
  RUN_TEST(test_ethernet_failure_keeps_live_pins_and_no_restart);
#endif
  return UNITY_END();
}
