#include <nvs.h>
#include <string.h>
#include "ConfigSettings.h"
#include "Secrets.h"
#include "SettingsDurability.h"
#include "SettingsTypedValidation.h"
#include "SettingsLoad.h"
#include "Transceiver.h"

namespace {
// Keep the existing Preferences namespaces and NVS key types, including the
// historical signed MQTT port. No namespace clear and no new storage format.
class SettingsWriter {
  SettingsNamespace space;
  nvs_handle_t handle = 0;
  bool opened = false, written = false;
  settings_save_result_t outcome;
  void failed(const char *key) {
    outcome = {written ? settings_save_state_t::partial : settings_save_state_t::failed, key};
  }
  bool committed(const char *key, esp_err_t setResult) {
    if(setResult != ESP_OK) { failed(key); return false; }
    // A set can have modified durable state even when commit/verification fails.
    written = true;
    if(nvs_commit(handle) != ESP_OK) { failed(key); return false; }
    return true;
  }
  template<class T, class Set, class Get>
  void number(const char *key, T value, Set set, Get get) {
    if(!outcome.saved()) return;
    if(!committed(key, set(handle,key,value))) return;
    T check{};
    if(get(handle,key,&check) != ESP_OK || check != value) failed(key);
  }
public:
  explicit SettingsWriter(SettingsNamespace id,const char *name) : space(id) {
    opened = nvs_open(name,NVS_READWRITE,&handle) == ESP_OK;
    if(!opened) failed(name);
  }
  ~SettingsWriter() { if(opened) nvs_close(handle);settingsDurability().fullSave(space,outcome); }
  settings_save_result_t result() const { return outcome; }
  void text(const char *key, const char *value) {
    if(!outcome.saved()) return;
    char check[65]; // the largest settings string is 64 bytes plus terminator
    const size_t expected = strlen(value)+1;
    if(expected > sizeof(check)) { failed(key); return; }
    if(!committed(key,nvs_set_str(handle,key,value))) return;
    size_t size=sizeof(check);
    const bool verified = nvs_get_str(handle,key,check,&size) == ESP_OK && size == expected && memcmp(check,value,expected) == 0;
    wipeSecret(check);
    if(!verified) failed(key);
  }
  void i8(const char *key, int8_t value) { number(key,value,nvs_set_i8,nvs_get_i8); }
  void u8(const char *key, uint8_t value) { number(key,value,nvs_set_u8,nvs_get_u8); }
  void i16(const char *key, int16_t value) { number(key,value,nvs_set_i16,nvs_get_i16); }
  void flag(const char *key, bool value) { u8(key,value ? 1 : 0); }
  void real(const char *key, float value) {
    if(!outcome.saved()) return;
    if(!committed(key,nvs_set_blob(handle,key,&value,sizeof(value)))) return;
    float check{};
    size_t size=sizeof(check);
    if(nvs_get_blob(handle,key,&check,&size) != ESP_OK || size != sizeof(check) || memcmp(&check,&value,sizeof(value)) != 0) failed(key);
  }
};
}

settings_save_result_t ConfigSettings::save() {
  if(!loadStatus.readable() || !validSettings(*this))return {settings_save_state_t::failed,"settings load"};
  SettingsWriter out(SettingsNamespace::cfg,"CFG");
  out.text("hostname", hostname);
  out.flag("ssdpBroadcast", ssdpBroadcast);
  out.i8("connType", static_cast<uint8_t>(connType));
  out.flag("checkForUpdate", checkForUpdate);
  const auto result=out.result();if(result.saved())loadStatus={SettingsLoadState::loaded};return result;
}
settings_save_result_t MQTTSettings::save() {
  if(!loadStatus.readable() || !validSettings(*this))return {settings_save_state_t::failed,"settings load"};
  SettingsWriter out(SettingsNamespace::mqtt,"MQTT");
  out.text("protocol", protocol);
  out.text("hostname", hostname);
  out.i16("port", static_cast<int16_t>(port));
  out.text("username", username);
  out.text("password", password);
  out.text("rootTopic", rootTopic);
  out.flag("enabled", enabled);
  out.flag("pubDisco", pubDisco);
  out.text("discoTopic", discoTopic);
  const auto result=out.result();if(result.saved())loadStatus={SettingsLoadState::loaded};return result;
}
settings_save_result_t NTPSettings::save() {
  if(!loadStatus.readable() || !validSettings(*this))return {settings_save_state_t::failed,"settings load"};
  SettingsWriter out(SettingsNamespace::ntp,"NTP");
  out.text("ntpServer", ntpServer);
  out.text("posixZone", posixZone);
  const auto result=out.result();if(result.saved())loadStatus={SettingsLoadState::loaded};return result;
}
settings_save_result_t IPSettings::save() {
  if(!loadStatus.readable() || !validSettings(*this))return {settings_save_state_t::failed,"settings load"};
  SettingsWriter out(SettingsNamespace::ip,"IP");
  out.flag("dhcp", dhcp);
  out.text("ip", ip.toString().c_str());
  out.text("gateway", gateway.toString().c_str());
  out.text("subnet", subnet.toString().c_str());
  out.text("dns1", dns1.toString().c_str());
  out.text("dns2", dns2.toString().c_str());
  const auto result=out.result();if(result.saved())loadStatus={SettingsLoadState::loaded};return result;
}
settings_save_result_t SecuritySettings::save() {
  if(!loadTrusted())return {settings_save_state_t::failed,"SEC load"};
  SettingsWriter out(SettingsNamespace::security,"SEC");
  out.i8("type", static_cast<uint8_t>(type));
  out.text("username", username);
  out.text("password", password);
  out.text("pin", pin);
  return out.result();
}
settings_save_result_t WifiSettings::save() {
  if(!loadStatus.readable() || !validSettings(*this))return {settings_save_state_t::failed,"settings load"};
  SettingsWriter out(SettingsNamespace::wifi,"WIFI");
  out.text("ssid", ssid);
  out.text("passphrase", passphrase);
  out.flag("roaming", roaming);
  out.flag("hidden", hidden);
  const auto result=out.result();if(result.saved())loadStatus={SettingsLoadState::loaded};return result;
}
#if HAS_ETHERNET
settings_save_result_t EthernetSettings::save() {
  if(!loadStatus.readable() || !validSettings(*this))return {settings_save_state_t::failed,"settings load"};
  SettingsWriter out(SettingsNamespace::ethernet,"ETH");
  out.i8("boardType", boardType);
  out.i8("phyAddress", phyAddress);
  out.i8("phyType", phyType);
  out.i8("CLKMode", CLKMode);
  out.i8("PWRPin", PWRPin);
  out.i8("MDCPin", MDCPin);
  out.i8("MDIOPin", MDIOPin);
  const auto result=out.result();if(result.saved())loadStatus={SettingsLoadState::loaded};return result;
}
#endif
static settings_save_result_t saveRadioSettings(const transceiver_config_t &cfg,bool marker) {
  SettingsWriter out(SettingsNamespace::radio,"CC1101");
  out.u8("type", cfg.type);
  out.u8("TXPin", cfg.TXPin);
  out.u8("RXPin", cfg.RXPin);
  out.u8("SCKPin", cfg.SCKPin);
  out.u8("MOSIPin", cfg.MOSIPin);
  out.u8("MISOPin", cfg.MISOPin);
  out.u8("CSNPin", cfg.CSNPin);
  out.real("frequency", cfg.frequency);
  out.real("deviation", cfg.deviation);
  out.real("rxBandwidth", cfg.rxBandwidth);
  out.flag("enabled", cfg.enabled);
  out.flag("radioInit", marker);
  out.i8("txPower", cfg.txPower);
  out.i8("proto", static_cast<uint8_t>(cfg.proto));
  out.flag("noiseDetect", cfg.noiseDetection);
  return out.result();
}

settings_save_result_t transceiver_config_t::save() {
  if(!loadStatus.readable())return {settings_save_state_t::failed,"CC1101 load"};
  applyBoardPins();
  if(!validSettings(*this))return {settings_save_state_t::failed,"CC1101 invalid"};
  const auto result=saveRadioSettings(*this,true);
  if(result.saved()){loadStatus={SettingsLoadState::loaded};startupInitAllowed=true;}
  return result;
}
settings_save_result_t transceiver_config_t::savePreservingInit() {
  if(!loadStatus.readable())return {settings_save_state_t::failed,"CC1101 load"};
  bool marker=true;
  {
    SettingsLoadReader reader("CC1101");reader.flag("radioInit",marker);
    if(!reader.status().usable())return {settings_save_state_t::failed,"radioInit"};
  }
  applyBoardPins();
  if(!validSettings(*this))return {settings_save_state_t::failed,"CC1101 invalid"};
  const auto result=saveRadioSettings(*this,marker);
  if(result.saved()){loadStatus={SettingsLoadState::loaded};startupInitAllowed=marker;}
  return result;
}

settings_migration_result_t ConfigSettings::saveLegacyMigration() {
  settings_migration_result_t out;
  out.persistence=save();
  if(!out.saved()) return out; // Never remove a failed migration's source.
  nvs_handle_t legacy;
  if(nvs_open("WIFI",NVS_READWRITE,&legacy)!=ESP_OK) return out;
  const auto hostname=nvs_erase_key(legacy,"hostname");
  const auto ssdp=nvs_erase_key(legacy,"ssdpBroadcast");
  const auto committed=nvs_commit(legacy);
  nvs_close(legacy);
  out.sourceCleaned=(hostname==ESP_OK || hostname==ESP_ERR_NVS_NOT_FOUND) &&
    (ssdp==ESP_OK || ssdp==ESP_ERR_NVS_NOT_FOUND) && committed==ESP_OK;
  return out;
}
