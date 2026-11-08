#include <nvs.h>
#include <string.h>
#include "ConfigSettings.h"
#include "Secrets.h"
#include "Transceiver.h"

namespace {
// Keep the existing Preferences namespaces and NVS key types, including the
// historical signed MQTT port. No namespace clear and no new storage format.
class SettingsWriter {
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
  explicit SettingsWriter(const char *space) {
    opened = nvs_open(space,NVS_READWRITE,&handle) == ESP_OK;
    if(!opened) failed(space);
  }
  ~SettingsWriter() { if(opened) nvs_close(handle); }
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
  SettingsWriter out("CFG");
  out.text("hostname", hostname);
  out.flag("ssdpBroadcast", ssdpBroadcast);
  out.i8("connType", static_cast<uint8_t>(connType));
  out.flag("checkForUpdate", checkForUpdate);
  return out.result();
}
settings_save_result_t MQTTSettings::save() {
  SettingsWriter out("MQTT");
  out.text("protocol", protocol);
  out.text("hostname", hostname);
  out.i16("port", static_cast<int16_t>(port));
  out.text("username", username);
  out.text("password", password);
  out.text("rootTopic", rootTopic);
  out.flag("enabled", enabled);
  out.flag("pubDisco", pubDisco);
  out.text("discoTopic", discoTopic);
  return out.result();
}
settings_save_result_t NTPSettings::save() {
  SettingsWriter out("NTP");
  out.text("ntpServer", ntpServer);
  out.text("posixZone", posixZone);
  return out.result();
}
settings_save_result_t IPSettings::save() {
  SettingsWriter out("IP");
  out.flag("dhcp", dhcp);
  out.text("ip", ip.toString().c_str());
  out.text("gateway", gateway.toString().c_str());
  out.text("subnet", subnet.toString().c_str());
  out.text("dns1", dns1.toString().c_str());
  out.text("dns2", dns2.toString().c_str());
  return out.result();
}
settings_save_result_t SecuritySettings::save() {
  SettingsWriter out("SEC");
  out.i8("type", static_cast<uint8_t>(type));
  out.text("username", username);
  out.text("password", password);
  out.text("pin", pin);
  return out.result();
}
settings_save_result_t WifiSettings::save() {
  SettingsWriter out("WIFI");
  out.text("ssid", ssid);
  out.text("passphrase", passphrase);
  out.flag("roaming", roaming);
  out.flag("hidden", hidden);
  return out.result();
}
#if HAS_ETHERNET
settings_save_result_t EthernetSettings::save() {
  SettingsWriter out("ETH");
  out.i8("boardType", boardType);
  out.i8("phyAddress", phyAddress);
  out.i8("phyType", phyType);
  out.i8("CLKMode", CLKMode);
  out.i8("PWRPin", PWRPin);
  out.i8("MDCPin", MDCPin);
  out.i8("MDIOPin", MDIOPin);
  return out.result();
}
#endif
settings_save_result_t transceiver_config_t::save() {
  applyBoardPins();
  SettingsWriter out("CC1101");
  out.u8("type", type);
  out.u8("TXPin", TXPin);
  out.u8("RXPin", RXPin);
  out.u8("SCKPin", SCKPin);
  out.u8("MOSIPin", MOSIPin);
  out.u8("MISOPin", MISOPin);
  out.u8("CSNPin", CSNPin);
  out.real("frequency", frequency);
  out.real("deviation", deviation);
  out.real("rxBandwidth", rxBandwidth);
  out.flag("enabled", enabled);
  out.flag("radioInit", true);
  out.i8("txPower", txPower);
  out.i8("proto", static_cast<uint8_t>(proto));
  out.flag("noiseDetect", noiseDetection);
  return out.result();
}
