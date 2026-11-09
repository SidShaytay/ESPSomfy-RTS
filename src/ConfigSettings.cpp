#include <Arduino.h>
#include <LittleFS.h>       // https://github.com/espressif/arduino-esp32/tree/master/libraries/LittleFS
#include <time.h>
#include <WiFi.h>
#include <Preferences.h>
#include "ConfigSettings.h"
#if defined(ESPSOMFY_BUNDLED_WEB)
#include "BundledWeb.h"
#endif
#include "Utils.h"
#include "Secrets.h"
#include "Transceiver.h"
#include "esp_chip_info.h"
#include "esp_ota_ops.h"
#include "esp_log.h"

static const char *TAG = "Config";


void restore_options_t::fromJSON(JsonObject &obj) {
  if(!obj["shades"].isNull()) this->shades = obj["shades"];
  if(!obj["settings"].isNull()) this->settings = obj["settings"];
  if(!obj["network"].isNull()) this->network = obj["network"];
  if(!obj["transceiver"].isNull()) this->transceiver = obj["transceiver"];
  if(!obj["repeaters"].isNull()) this->repeaters = obj["repeaters"];
  if(!obj["mqtt"].isNull()) this->mqtt = obj["mqtt"];
}
int8_t appver_t::compare(appver_t &ver) {
  if(this->major == ver.major && this->minor == ver.minor && this->build == ver.build) return 0;
  if(this->major > ver.major) return 1;
  else if(this->major < ver.major) return -1;
  else {
    if(this->minor > ver.minor) return 1;
    else if(this->minor < ver.minor) return -1;
    else {
      if(this->build > ver.build) return 1;
      else if(this->build < ver.build) return -1;
    }
  }
  return 0;
}
void appver_t::copy(appver_t &ver) {
  strcpy(this->name, ver.name);
  this->major = ver.major;
  this->minor = ver.minor;
  this->build = ver.build;
  strcpy(this->suffix, ver.suffix);
}
void appver_t::parse(const char *ver) {
  // Now lets parse this pig.
  memset(this, 0x00, sizeof(appver_t));
  strlcpy(this->name, ver, sizeof(this->name));
  char num[3];
  uint8_t i = 0;
  memset(num, 0x00, sizeof(num));
  for(uint8_t j = 0; j < 3 && i < strlen(ver);) {
    char ch = ver[i++];
    // Trim off all the prefix.
    if(ch == '.') break;
    if(!isdigit(ch)) continue;
    if(ch != '.')
      num[j++] = ch;
    else
      break;
  }
  this->major = static_cast<uint8_t>(atoi(num) & 0xFF);
  memset(num, 0x00, sizeof(num));
  for(uint8_t j = 0; j < 3 && i < strlen(ver);) {
    char ch = ver[i++];
    if(ch != '.')
      num[j++] = ch;
    else
      break;
  }
  this->minor = static_cast<uint8_t>(atoi(num) & 0xFF);
  memset(num, 0x00, sizeof(num));
  for(uint8_t j = 0; j < 3 && i < strlen(ver);) {
    char ch = ver[i++];
    if(!isdigit(ch)) break;
    if(ch != '.')
      num[j++] = ch;
    else
      break;
  }
  this->build = static_cast<uint8_t>(atoi(num) & 0xFF);
  if(strlen(ver) < i) strlcpy(this->suffix, &ver[i], sizeof(this->suffix));
}

bool BaseSettings::load() { return true; }
bool BaseSettings::loadFile(const char *filename) { 
  size_t filesize = 10;
  String data = "";
  if(LittleFS.exists(filename)) {
    File file = LittleFS.open(filename, "r");
    filesize += file.size();
    while(file.available()) {
      char c = file.read();
      data += c;
    }
    JsonDocument doc;
    deserializeJson(doc, data);
    JsonObject obj = doc.as<JsonObject>();
    this->fromJSON(obj);
    file.close();
  }
  return false; 
}
bool ConfigSettings::begin() {
  uint32_t chipId = 0;
  esp_chip_info_t ci;
  esp_chip_info(&ci);
  switch(ci.model) {
    case esp_chip_model_t::CHIP_ESP32:
      strcpy(this->chipModel, "");
      break;
    case esp_chip_model_t::CHIP_ESP32S3:
      strcpy(this->chipModel, "s3");
      break;
    case esp_chip_model_t::CHIP_ESP32C3:
      strcpy(this->chipModel, "c3");
      break;
    #ifdef CONFIG_IDF_TARGET_ESP32C5
    case esp_chip_model_t::CHIP_ESP32C5:
      strcpy(this->chipModel, "c5");
      break;
    #endif
    #ifdef CONFIG_IDF_TARGET_ESP32C6
    case esp_chip_model_t::CHIP_ESP32C6:
      strcpy(this->chipModel, "c6");
      break;
    #endif
    #ifdef CONFIG_IDF_TARGET_ESP32P4
    // Chips before v2.0 run their own build (esp32p4_es libraries), so they
    // get their own release files.  v2.x and v3.00 run neither.
    case esp_chip_model_t::CHIP_ESP32P4:
      #if CONFIG_ESP32P4_SELECTS_REV_LESS_V3
      strcpy(this->chipModel, "p4es");
      #else
      strcpy(this->chipModel, "p4");
      #endif
      break;
    #endif
    default:
      sprintf(this->chipModel, "UNK%d", static_cast<int>(ci.model));
      break;
  }
  ESP_LOGD(TAG, "Chip Model ESP32-%s", this->chipModel);
  this->hasOtaSlot = esp_ota_get_next_update_partition(nullptr) != nullptr;
  this->fwVersion.parse(FW_VERSION);
  uint64_t mac = ESP.getEfuseMac();
  for(int i=0; i<17; i=i+8) {
    chipId |= ((mac >> (40 - i)) & 0xff) << i;
  }
  snprintf_P(this->serverId, sizeof(this->serverId), "%02X%02X%02X",
    (uint16_t)((chipId >> 16) & 0xff),
    (uint16_t)((chipId >> 8) & 0xff),
    (uint16_t)chipId & 0xff);
  this->getAppVersion();
  bool ready=this->load();
  ready &= this->Security.begin();
  ready &= this->IP.begin();
  ready &= this->WIFI.begin();
#if HAS_ETHERNET
  ready &= this->Ethernet.begin();
#endif
  ready &= this->NTP.begin();
  ready &= this->MQTT.begin();
  if(ready)this->print();
  else ESP_LOGW(TAG,"Settings require repair; unavailable values were not applied");
  return ready;
}

bool ConfigSettings::applyStartup(const transceiver_config_t &radio) {
  // All namespaces must have completed their read phase before any side effect.
  if(loadStatus.state==SettingsLoadState::unknown || NTP.loadStatus.state==SettingsLoadState::unknown ||
     IP.loadStatus.state==SettingsLoadState::unknown || WIFI.loadStatus.state==SettingsLoadState::unknown ||
     MQTT.loadStatus.state==SettingsLoadState::unknown || Security.loadState==SecurityLoadState::unknown ||
     radio.loadStatus.state==SettingsLoadState::unknown
#if HAS_ETHERNET
     || Ethernet.loadStatus.state==SettingsLoadState::unknown
#endif
     )return false;
  if(startupApplied)return settingsUsable() && radio.loadStatus.usable();
  startupApplied=true;
  if(NTP.loadStatus.usable())NTP.apply();
  if(legacyMigrationPending && loadStatus.usable() && WIFI.loadStatus.usable()) {
    const auto migrated=saveLegacyMigration();
    if(!migrated.saved())ESP_LOGW(TAG,"Legacy settings migration not saved; source retained");
    else legacyMigrationPending=false;
  }
  return settingsUsable() && radio.loadStatus.usable();
}

bool ConfigSettings::getAppVersion() {
#if defined(ESPSOMFY_BUNDLED_WEB)
  this->appVersion.parse(BundledWeb::sourceVersion());
  return true;
#else
  char app[15];
  if(!LittleFS.exists("/appversion")) return false;
  File f = LittleFS.open("/appversion", "r");
  memset(app, 0x00, sizeof(app));
  f.read((uint8_t *)app, sizeof(app) - 1);
  f.close();
  _trim(app);
  this->appVersion.parse(app);
  return true;
#endif
}

bool ConfigSettings::requiresAuth() { return !Security.loadTrusted() || Security.type != security_types::None; }
void ConfigSettings::print() {
  this->Security.print();
  ESP_LOGD(TAG, "Connection Type: %u", (unsigned int) this->connType);
  this->NTP.print();
  if(this->connType == conn_types_t::wifi || this->connType == conn_types_t::unset) this->WIFI.print();
#if HAS_ETHERNET
  if(this->connType == conn_types_t::ethernet || this->connType == conn_types_t::ethernetpref) this->Ethernet.print();
#endif
}




void NTPSettings::print() {
  ESP_LOGD(TAG, "NTP Settings %s TZ:%s", this->ntpServer, this->posixZone);
}
bool NTPSettings::apply() {
  // Sets the zone and starts SNTP, which sets the clock once the network is
  // up.  Waiting here with getLocalTime() held up boot by 5 s before the
  // network existed, and then returned before the zone was set.
  configTzTime(this->posixZone, this->ntpServer);
  return true;
}



void SecuritySettings::print() {
  ESP_LOGD(TAG, "SECURITY   Type:%u Username:[%s] Password:[%s] Pin:[%s]",
    static_cast<uint8_t>(this->type), this->username, maskSecret(this->password), maskSecret(this->pin));
}



String WifiSettings::mapEncryptionType(int type) {
  switch(type) {
    case WIFI_AUTH_OPEN:
      return "Open";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA/PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2/PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2/PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA/Enterprise";
  }
  return "Unknown";
}
void WifiSettings::print() {
  ESP_LOGD(TAG, "WIFI Settings SSID: [%s] PassPhrase: [%s]", this->ssid, maskSecret(this->passphrase));
}
void WifiSettings::printNetworks() {
  int n = WiFi.scanNetworks(false, false);
  ESP_LOGI(TAG, "Scanned %d Networks...", n);
  String network;
  for(int i = 0; i < n; i++) {
    ESP_LOGI(TAG, "%s%d: %s (%ddBm) CH:%d MAC:%s",
      (WiFi.SSID(i).compareTo(this->ssid) == 0) ? "*" : " ",
      i, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i), WiFi.BSSIDstr(i).c_str());
  }
}
bool WifiSettings::ssidExists(const char *ssid) {
  int n = WiFi.scanNetworks(false, true);
  for(int i = 0; i < n; i++) {
    if(WiFi.SSID(i).compareTo(ssid) == 0) return true;
  }
  return false;
}
#if HAS_ETHERNET

bool EthernetSettings::usesPin(uint8_t pin) const {
  if(ethClockPin(this->CLKMode) == pin) return true;
  else if(this->PWRPin == pin) return true;
  else if(this->MDCPin == pin) return true;
  else if(this->MDIOPin == pin) return true;
  return false;  
}

void EthernetSettings::print() {
  ESP_LOGD(TAG, "Ethernet Settings Board:%d PHYType:%d CLK:%d ADDR:%d PWR:%d MDC:%d MDIO:%d",
    this->boardType, this->phyType, this->CLKMode, this->phyAddress, this->PWRPin, this->MDCPin, this->MDIOPin);
}
#endif // HAS_ETHERNET
void ConfigSettings::printAvailHeap() {
  ESP_LOGD(TAG, "Max Heap: %u", (unsigned int)ESP.getMaxAllocHeap());
  ESP_LOGD(TAG, "Free Heap: %u", (unsigned int)ESP.getFreeHeap());
  ESP_LOGD(TAG, "Min Heap: %u", (unsigned int)ESP.getMinFreeHeap());
}
