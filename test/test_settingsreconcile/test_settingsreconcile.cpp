#include "QualifiedSettings.h"
#include <unity.h>
#include <nvs.h>
#include "SettingsReconcile.h"
#include "Transceiver.h"
class Effects : public SettingsEffects {
public:
  uint8_t sections=0;unsigned calls=0;
  bool networkPinsAvailable(const ConfigSettings &) const override{return true;}
  bool wifiClientsPresent() const override { TEST_FAIL_MESSAGE("No runtime query during reconcile");return false; }
  void hostnameChanged() override { TEST_FAIL_MESSAGE("No hostname apply"); }
  void timeChanged() override { TEST_FAIL_MESSAGE("No time apply"); }
  void mqttChanged() override { TEST_FAIL_MESSAGE("No MQTT disconnect"); }
  settings_save_result_t saveRadio(JsonObject) override { TEST_FAIL_MESSAGE("No radio apply");return {}; }
  RestartResult completed(settings_section_t section,bool reboot) override {
    TEST_ASSERT_FALSE(reboot);++calls;sections|=1u<<static_cast<uint8_t>(section);
    return {reboot,RestartReason::none};
  }
};
void setUp() { FakeNvs::reset();settingsDurability()=SettingsDurability{}; }
void tearDown() { TEST_ASSERT_TRUE(FakeNvs::handles.empty()); }
static void reconcile_uses_running_values_not_failed_edit_and_never_restarts() {
  ConfigSettings current;qualifySettings(current);current.Security.loadState=SecurityLoadState::knownEmpty;strcpy(current.hostname,"running");
  ConfigSettings abandoned;qualifySettings(abandoned);strcpy(abandoned.hostname,"old draft");
  FakeNvs::failCommit=1;TEST_ASSERT_FALSE(abandoned.save().saved());FakeNvs::failCommit=0;
  Effects effects;transceiver_config_t radio;qualifySettings(radio);
  const auto result=reconcileRunningSettings(current,radio,effects);
  TEST_ASSERT_TRUE(result.saved());TEST_ASSERT_EQUAL_UINT8(1,result.reconciled);
  TEST_ASSERT_EQUAL_STRING("running",reinterpret_cast<const char *>(FakeNvs::values.at("CFG/hostname").bytes.data()));
  TEST_ASSERT_EQUAL_UINT8(3,effects.sections); // CFG affects general and network
  TEST_ASSERT_EQUAL(2,effects.calls);
}
static void failed_reconcile_keeps_uncertainty_and_notifies_only_verified_namespaces() {
  settingsDurability().markUncertain(settingsNamespaceBit(SettingsNamespace::cfg)|settingsNamespaceBit(SettingsNamespace::security));
  ConfigSettings current;qualifySettings(current);current.Security.loadState=SecurityLoadState::knownEmpty;Effects effects;transceiver_config_t radio;qualifySettings(radio);
  FakeNvs::failOpen=2;
  const auto result=reconcileRunningSettings(current,radio,effects);
  TEST_ASSERT_FALSE(result.saved());TEST_ASSERT_EQUAL(settings_save_state_t::partial,result.persistence.state);
  TEST_ASSERT_EQUAL_UINT8(settingsNamespaceBit(SettingsNamespace::security),result.durability.uncertain);
  TEST_ASSERT_EQUAL_UINT8(3,effects.sections);TEST_ASSERT_EQUAL(2,effects.calls);
}
static void radio_reconcile_preserves_operational_guard_and_never_applies() {
  for(uint8_t guard=0;guard<2;guard++) {
    FakeNvs::reset();
    nvs_handle_t handle;TEST_ASSERT_EQUAL(ESP_OK,nvs_open("CC1101",NVS_READWRITE,&handle));
    TEST_ASSERT_EQUAL(ESP_OK,nvs_set_u8(handle,"radioInit",guard));nvs_close(handle);
    settingsDurability().markUncertain(settingsNamespaceBit(SettingsNamespace::radio));
    ConfigSettings current;qualifySettings(current);current.Security.loadState=SecurityLoadState::knownEmpty;Effects effects;transceiver_config_t radio;qualifySettings(radio);radio.radioInit=guard==0;
    const auto result=reconcileRunningSettings(current,radio,effects);
    TEST_ASSERT_TRUE(result.saved());TEST_ASSERT_EQUAL(guard,FakeNvs::values.at("CC1101/radioInit").bytes[0]);
    TEST_ASSERT_EQUAL(guard==0,radio.radioInit);TEST_ASSERT_EQUAL_UINT8(16,effects.sections);
  }
}
static void later_partial_failure_remains_partial_even_when_nothing_fully_reconciles() {
  settingsDurability().markUncertain(settingsNamespaceBit(SettingsNamespace::cfg)|settingsNamespaceBit(SettingsNamespace::ntp));
  ConfigSettings current;qualifySettings(current);current.Security.loadState=SecurityLoadState::knownEmpty;Effects effects;transceiver_config_t radio;qualifySettings(radio);
  FakeNvs::failOpen=1;FakeNvs::failCommit=1;
  const auto result=reconcileRunningSettings(current,radio,effects);
  TEST_ASSERT_EQUAL(settings_save_state_t::partial,result.persistence.state);
  TEST_ASSERT_FALSE(result.saved());TEST_ASSERT_EQUAL(0,effects.calls);
}
static void radio_invalid_or_unreadable_guard_cannot_clear_uncertainty_or_write() {
  for(int failure=0;failure<4;failure++) {
    FakeNvs::reset();
    nvs_handle_t h;TEST_ASSERT_EQUAL(ESP_OK,nvs_open("CC1101",NVS_READWRITE,&h));
    if(failure==0) TEST_ASSERT_EQUAL(ESP_OK,nvs_set_u8(h,"radioInit",2));
    else if(failure==1) TEST_ASSERT_EQUAL(ESP_OK,nvs_set_i8(h,"radioInit",1));
    else TEST_ASSERT_EQUAL(ESP_OK,nvs_set_u8(h,"radioInit",0));
    nvs_close(h);
    const int before=FakeNvs::sets;
    if(failure==2) FakeNvs::failOpen=FakeNvs::opens+1;
    if(failure==3) FakeNvs::failRead=FakeNvs::reads+1;
    settingsDurability().markUncertain(settingsNamespaceBit(SettingsNamespace::radio));
    ConfigSettings current;qualifySettings(current);current.Security.loadState=SecurityLoadState::knownEmpty;Effects effects;transceiver_config_t radio;qualifySettings(radio);
    const auto result=reconcileRunningSettings(current,radio,effects);
    TEST_ASSERT_FALSE(result.saved());TEST_ASSERT_EQUAL(before,FakeNvs::sets);
    TEST_ASSERT_EQUAL_UINT8(settingsNamespaceBit(SettingsNamespace::radio),result.durability.uncertain);
    TEST_ASSERT_EQUAL(0,effects.calls);
  }
}
static void no_uncertainty_does_not_write_or_notify() {
  ConfigSettings current;qualifySettings(current);current.Security.loadState=SecurityLoadState::knownEmpty;Effects effects;transceiver_config_t radio;qualifySettings(radio);
  const auto result=reconcileRunningSettings(current,radio,effects);
  TEST_ASSERT_TRUE(result.saved());TEST_ASSERT_EQUAL(0,FakeNvs::opens);TEST_ASSERT_EQUAL(0,effects.calls);
}
static void successful_security_reconcile_refreshes_history_even_without_value_changes() {
  settingsDurability().markUncertain(settingsNamespaceBit(SettingsNamespace::security));
  ConfigSettings current;qualifySettings(current);current.Security.loadState=SecurityLoadState::knownEmpty;Effects effects;transceiver_config_t radio;qualifySettings(radio);
  strcpy(current.Security.password,"running secret");
  const auto result=reconcileRunningSettings(current,radio,effects);
  TEST_ASSERT_TRUE(result.saved());TEST_ASSERT_EQUAL_UINT8(8,effects.sections);TEST_ASSERT_EQUAL(1,effects.calls);
  TEST_ASSERT_EQUAL_STRING("running secret",reinterpret_cast<const char *>(FakeNvs::values.at("SEC/password").bytes.data()));
}
static void all_supported_namespaces_rewrite_once_and_complete_each_section_once() {
  ConfigSettings current;qualifySettings(current);current.Security.loadState=SecurityLoadState::knownEmpty;Effects effects;transceiver_config_t radio;qualifySettings(radio);
  nvs_handle_t h;TEST_ASSERT_EQUAL(ESP_OK,nvs_open("CC1101",NVS_READWRITE,&h));
  TEST_ASSERT_EQUAL(ESP_OK,nvs_set_u8(h,"radioInit",0));nvs_close(h);
  uint8_t mask=255;
#if !HAS_ETHERNET
  mask&=~settingsNamespaceBit(SettingsNamespace::ethernet);
#endif
  settingsDurability().markUncertain(mask);
  const auto result=reconcileRunningSettings(current,radio,effects);
  TEST_ASSERT_TRUE(result.saved());TEST_ASSERT_EQUAL_UINT8(mask,result.reconciled);
  TEST_ASSERT_EQUAL_UINT8(31,effects.sections);TEST_ASSERT_EQUAL(5,effects.calls);
}
static void untrusted_security_load_cannot_be_declared_reconciled_even_with_empty_write_ledger() {
  for(bool dirty:{false,true}) {
    settingsDurability()=SettingsDurability{};FakeNvs::reset();
    if(dirty)settingsDurability().markUncertain(settingsNamespaceBit(SettingsNamespace::security));
    ConfigSettings current;qualifySettings(current);current.Security.loadState=SecurityLoadState::failed;
    Effects effects;transceiver_config_t radio;qualifySettings(radio);
    const auto before=settingsDurability().status();
    const auto result=reconcileRunningSettings(current,radio,effects);
    TEST_ASSERT_FALSE(result.saved());TEST_ASSERT_EQUAL(0,result.reconciled);
    TEST_ASSERT_EQUAL(0,FakeNvs::opens);TEST_ASSERT_EQUAL(0,effects.calls);
    TEST_ASSERT_EQUAL(before.epoch,settingsDurability().status().epoch);
    TEST_ASSERT_EQUAL(before.uncertain,settingsDurability().status().uncertain);
    TEST_ASSERT_FALSE(current.Security.loadTrusted());
  }
}
static void bad_nonsecurity_load_is_not_certified_by_an_empty_write_ledger() {
 for(int field=0;field<3;++field){
  ConfigSettings current;qualifySettings(current);current.Security.loadState=SecurityLoadState::knownEmpty;
  transceiver_config_t radio;qualifySettings(radio);Effects effects;
  if(field==0)current.WIFI.loadStatus.state=SettingsLoadState::failed;
  if(field==1)current.IP.loadStatus.state=SettingsLoadState::invalid;
  if(field==2)radio.loadStatus.state=SettingsLoadState::unknown;
  const auto result=reconcileRunningSettings(current,radio,effects);
  TEST_ASSERT_FALSE(result.saved());TEST_ASSERT_EQUAL(0,result.reconciled);TEST_ASSERT_EQUAL(0,FakeNvs::opens);TEST_ASSERT_EQUAL(0,effects.calls);
 }
}
int main() {
  UNITY_BEGIN();RUN_TEST(bad_nonsecurity_load_is_not_certified_by_an_empty_write_ledger);RUN_TEST(untrusted_security_load_cannot_be_declared_reconciled_even_with_empty_write_ledger);
  RUN_TEST(all_supported_namespaces_rewrite_once_and_complete_each_section_once);
  RUN_TEST(later_partial_failure_remains_partial_even_when_nothing_fully_reconciles);
  RUN_TEST(radio_invalid_or_unreadable_guard_cannot_clear_uncertainty_or_write);
  RUN_TEST(no_uncertainty_does_not_write_or_notify);
  RUN_TEST(successful_security_reconcile_refreshes_history_even_without_value_changes);
  RUN_TEST(reconcile_uses_running_values_not_failed_edit_and_never_restarts);
  RUN_TEST(failed_reconcile_keeps_uncertainty_and_notifies_only_verified_namespaces);
  RUN_TEST(radio_reconcile_preserves_operational_guard_and_never_applies);
  return UNITY_END();
}
