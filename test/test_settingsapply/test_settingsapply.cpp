#include <unity.h>
#include <nvs.h>
#include <string>
#include "SettingsApply.h"
#include "SettingsRevisions.h"
#include "SettingsDurability.h"

class Effects : public SettingsEffects {
public:
  bool apClients=false;
  int hostnames=0,times=0,mqtt=0,radio=0,completions=0;
  settings_section_t completedSection=settings_section_t::general;
  bool completedReboot=false;
  RestartResult completionResult{true,RestartReason::none};
  ConfigSettings *observed=nullptr;
  std::string hostnameAtCompletion;
  settings_save_result_t radioResult;
  SettingsRevisions revisions;
  RestartResult completed(settings_section_t section,bool reboot) override {
    completions++; completedSection=section; completedReboot=reboot;
    revisions.completed(section);
    if(observed) hostnameAtCompletion=observed->hostname;
    return reboot ? completionResult : RestartResult{};
  }
  bool wifiClientsPresent() const override { return apClients; }
  void hostnameChanged() override { hostnames++; }
  void timeChanged() override { times++; }
  void mqttChanged() override { mqtt++; }
  settings_save_result_t saveRadio(JsonObject) override { radio++; settingsDurability().fullSave(SettingsNamespace::radio,radioResult);return radioResult; }
};
void setUp() { FakeNvs::reset();settingsDurability()=SettingsDurability{}; }
void tearDown() { TEST_ASSERT_TRUE(FakeNvs::handles.empty()); }
static settings_apply_result_t apply(ConfigSettings &settings, settings_section_t section, const char *fields, Effects &effects) {
  JsonDocument request;
  TEST_ASSERT_EQUAL(DeserializationError::Ok,deserializeJson(request,fields).code());
  return applySettings(settings,section,request.as<JsonObject>(),effects);
}
static void test_connection_change_reboots_only_after_verified_save() {
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
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
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
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
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
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
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
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
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
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
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
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
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
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
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
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
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
  Effects effects;
  auto result=apply(settings,settings_section_t::general,"{}",effects);
  TEST_ASSERT_FALSE(result.saved());
  TEST_ASSERT_EQUAL(0,FakeNvs::opens);
  TEST_ASSERT_EQUAL(0,effects.hostnames+effects.times+effects.mqtt+effects.radio);
}
#if HAS_ETHERNET
static void test_ethernet_failure_keeps_live_pins_and_no_restart() {
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
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
static void test_completion_occurs_once_after_live_apply_and_reboot_decision() {
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
  settings.connType=conn_types_t::wifi;
  Effects effects;
  effects.observed=&settings;
  auto saved=apply(settings,settings_section_t::general,R"({"hostname":"after-save"})",effects);
  TEST_ASSERT_TRUE(saved.saved());
  TEST_ASSERT_EQUAL(1,effects.completions);
  TEST_ASSERT_EQUAL(settings_section_t::general,effects.completedSection);
  TEST_ASSERT_EQUAL_STRING("after-save",effects.hostnameAtCompletion.c_str());
  TEST_ASSERT_FALSE(effects.completedReboot);
  saved=apply(settings,settings_section_t::network,R"({"connType":2})",effects);
  TEST_ASSERT_TRUE(saved.saved());
  TEST_ASSERT_EQUAL(2,effects.completions);
  TEST_ASSERT_TRUE(effects.completedReboot);
  TEST_ASSERT_EQUAL(saved.reboot,effects.completedReboot);
}
static void test_failed_and_partial_saves_never_complete() {
  for(int phase=0;phase<3;phase++) {
    FakeNvs::reset();
    ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
    settings.connType=conn_types_t::wifi;
    Effects effects;
    if(phase==0) FakeNvs::failOpen=1;
    if(phase==1) FakeNvs::failSet=2;
    if(phase==2) FakeNvs::failCommit=1;
    auto failed=apply(settings,settings_section_t::network,R"({"connType":2,"ssid":"changed"})",effects);
    TEST_ASSERT_FALSE(failed.saved());
    TEST_ASSERT_EQUAL(0,effects.completions);
    TEST_ASSERT_FALSE(effects.completedReboot);
  }
}
static void test_radio_completion_uses_the_same_success_gate() {
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
  Effects effects;
  effects.radioResult={settings_save_state_t::failed,"CC1101"};
  TEST_ASSERT_FALSE(apply(settings,settings_section_t::radio,R"({"frequency":433.42})",effects).saved());
  TEST_ASSERT_EQUAL(0,effects.completions);
  effects.radioResult={};
  TEST_ASSERT_TRUE(apply(settings,settings_section_t::radio,R"({"frequency":433.42})",effects).saved());
  TEST_ASSERT_EQUAL(1,effects.completions);
  TEST_ASSERT_EQUAL(settings_section_t::radio,effects.completedSection);
  TEST_ASSERT_FALSE(effects.completedReboot);
}
static void test_successful_noop_security_save_invalidates_history_but_failure_does_not() {
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
  Effects effects;
  const auto initial=effects.revisions.get(settings_section_t::security);
  TEST_ASSERT_TRUE(apply(settings,settings_section_t::security,R"({"type":0})",effects).saved());
  TEST_ASSERT_EQUAL_UINT64(initial+1,effects.revisions.get(settings_section_t::security));
  TEST_ASSERT_EQUAL(security_types::None,settings.Security.type);
  TEST_ASSERT_TRUE(apply(settings,settings_section_t::security,R"({"type":0})",effects).saved());
  TEST_ASSERT_EQUAL_UINT64(initial+2,effects.revisions.get(settings_section_t::security));
  FakeNvs::failCommit=FakeNvs::commits+1;
  TEST_ASSERT_FALSE(apply(settings,settings_section_t::security,R"({"type":0})",effects).saved());
  TEST_ASSERT_EQUAL_UINT64(initial+2,effects.revisions.get(settings_section_t::security));
}
static void test_enabling_pin_requires_four_effective_bytes_before_any_storage() {
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
  Effects effects;
  auto refused=apply(settings,settings_section_t::security,R"({"type":1})",effects);
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::invalidPin,refused.refusal);
  TEST_ASSERT_FALSE(refused.saved());
  TEST_ASSERT_EQUAL(0,FakeNvs::opens);
  TEST_ASSERT_EQUAL(0,effects.completions);
  TEST_ASSERT_EQUAL(security_types::None,settings.Security.type);
  TEST_ASSERT_NOT_NULL(strstr(settingsApplyFailureDescription(refused),"nothing was saved"));
  refused=apply(settings,settings_section_t::security,R"({"type":1,"pin":"123"})",effects);
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::invalidPin,refused.refusal);
  TEST_ASSERT_EQUAL(0,FakeNvs::opens);
  TEST_ASSERT_TRUE(apply(settings,settings_section_t::security,R"({"type":1,"pin":"A B!"})",effects).saved());
  TEST_ASSERT_EQUAL_STRING("A B!",settings.Security.pin); // no numeric-only or trim policy
  TEST_ASSERT_TRUE(apply(settings,settings_section_t::security,R"({"pin":""})",effects).saved());
  TEST_ASSERT_EQUAL_STRING("A B!",settings.Security.pin);
}
static void test_password_security_requires_effective_username_and_password_without_trimming() {
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
  Effects effects;
  auto refused=apply(settings,settings_section_t::security,R"({"type":2})",effects);
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::usernameRequired,refused.refusal);
  TEST_ASSERT_EQUAL(0,FakeNvs::opens);
  refused=apply(settings,settings_section_t::security,R"({"type":2,"username":"user"})",effects);
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::passwordRequired,refused.refusal);
  TEST_ASSERT_EQUAL(0,FakeNvs::opens);
  TEST_ASSERT_EQUAL(0,effects.completions);
  TEST_ASSERT_TRUE(apply(settings,settings_section_t::security,R"({"type":2,"username":" ","password":" "})",effects).saved());
  TEST_ASSERT_EQUAL_STRING(" ",settings.Security.username);
  TEST_ASSERT_EQUAL_STRING(" ",settings.Security.password);
  refused=apply(settings,settings_section_t::security,R"({"username":""})",effects);
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::usernameRequired,refused.refusal);
  TEST_ASSERT_EQUAL_STRING(" ",settings.Security.username);
}
static void test_unchanged_legacy_credentials_are_preserved_but_new_active_changes_must_be_valid() {
  ConfigSettings settings;settings.Security.loadState=SecurityLoadState::knownEmpty;
  Effects effects;
  settings.Security.type=security_types::PinEntry;
  strcpy(settings.Security.pin,"old");
  TEST_ASSERT_TRUE(apply(settings,settings_section_t::security,R"({"type":1,"pin":""})",effects).saved());
  TEST_ASSERT_EQUAL_STRING("old",settings.Security.pin);
  auto refused=apply(settings,settings_section_t::security,R"({"pin":"xy"})",effects);
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::invalidPin,refused.refusal);
  TEST_ASSERT_EQUAL_STRING("old",settings.Security.pin);
  TEST_ASSERT_TRUE(apply(settings,settings_section_t::security,R"({"pin":"1234"})",effects).saved());
  settings.Security.type=security_types::Password;
  settings.Security.username[0]=0;
  settings.Security.password[0]=0;
  TEST_ASSERT_TRUE(apply(settings,settings_section_t::security,R"({"type":2})",effects).saved());
  refused=apply(settings,settings_section_t::security,R"({"password":"changed"})",effects);
  TEST_ASSERT_EQUAL(settings_apply_refusal_t::usernameRequired,refused.refusal);
  TEST_ASSERT_TRUE(apply(settings,settings_section_t::security,R"({"username":"fixed","password":"fixed"})",effects).saved());
}
static void test_composite_failure_remarks_earlier_full_draft_namespace() {
  ConfigSettings current;Effects effects;
  settingsDurability().markUncertain(settingsNamespaceBit(SettingsNamespace::cfg));
  FakeNvs::failOpen=2;
  const auto result=apply(current,settings_section_t::general,R"({"hostname":"staged","ntpServer":"new.clock"})",effects);
  TEST_ASSERT_FALSE(result.saved());
  TEST_ASSERT_EQUAL_UINT8(settingsNamespaceBit(SettingsNamespace::cfg),settingsDurability().status().uncertain);
  TEST_ASSERT_EQUAL(0,effects.completions);
}
static void test_unrelated_success_reports_saved_but_restart_blocked_before_completion() {
  ConfigSettings current;current.connType=conn_types_t::wifi;Effects effects;
  settingsDurability().markUncertain(settingsNamespaceBit(SettingsNamespace::security));
  const auto result=apply(current,settings_section_t::network,R"({"connType":2})",effects);
  TEST_ASSERT_TRUE(result.saved());TEST_ASSERT_TRUE(result.restartBlocked);
  TEST_ASSERT_EQUAL(RestartReason::settingsUncertain,result.restartReason);
  TEST_ASSERT_FALSE(result.reboot);TEST_ASSERT_FALSE(effects.completedReboot);
  TEST_ASSERT_EQUAL(1,effects.completions);
  TEST_ASSERT_EQUAL_UINT8(settingsNamespaceBit(SettingsNamespace::security),settingsDurability().status().uncertain);
}
static void actual_completion_refusal_is_reported_without_replaying_effects() {
  for(auto reason:{RestartReason::persistenceBusy,RestartReason::pending,RestartReason::counterUncertain}) {
    ConfigSettings current;current.connType=conn_types_t::wifi;
    Effects effects;effects.completionResult={false,reason};
    const auto result=apply(current,settings_section_t::network,R"({"connType":2})",effects);
    TEST_ASSERT_TRUE(result.saved());TEST_ASSERT_FALSE(result.reboot);
    TEST_ASSERT_TRUE(result.restartBlocked);TEST_ASSERT_EQUAL(reason,result.restartReason);
    TEST_ASSERT_EQUAL(conn_types_t::ethernet,current.connType);
    TEST_ASSERT_EQUAL(1,effects.completions);TEST_ASSERT_TRUE(effects.completedReboot);
    TEST_ASSERT_EQUAL_UINT64(2,effects.revisions.get(settings_section_t::network));
  }
}
int main() {
  UNITY_BEGIN();
  RUN_TEST(actual_completion_refusal_is_reported_without_replaying_effects);
  RUN_TEST(test_composite_failure_remarks_earlier_full_draft_namespace);
  RUN_TEST(test_unrelated_success_reports_saved_but_restart_blocked_before_completion);
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
  RUN_TEST(test_completion_occurs_once_after_live_apply_and_reboot_decision);
  RUN_TEST(test_failed_and_partial_saves_never_complete);
  RUN_TEST(test_radio_completion_uses_the_same_success_gate);
  RUN_TEST(test_successful_noop_security_save_invalidates_history_but_failure_does_not);
  RUN_TEST(test_enabling_pin_requires_four_effective_bytes_before_any_storage);
  RUN_TEST(test_password_security_requires_effective_username_and_password_without_trimming);
  RUN_TEST(test_unchanged_legacy_credentials_are_preserved_but_new_active_changes_must_be_valid);
  return UNITY_END();
}
