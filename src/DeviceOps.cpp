#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <new>
#include "esp_log.h"
#include "DeviceOps.h"
#include "SettingsApply.h"
#include "ConfigSettings.h"
#include "ESPNetwork.h"
#include "MqttConnection.h"
#include "Secrets.h"
#include "Somfy.h"
#include "Utils.h"
#include "entryPoints/Web.h"
#include "outputs/ProtoSocket.h"

static const char *TAG = "DeviceOps";

extern ConfigSettings settings;
extern SomfyShadeController somfy;
extern ESPNetwork net;
extern MqttConnection mqtt;
extern Web webServer;
extern rebootDelay_t rebootDelay;

static std::atomic<bool> saveBusy(false);

settings_save_t *newSettingsSave(bool &busy) {
  busy = saveBusy.exchange(true);
  if(busy) return nullptr;
  settings_save_t *save = new (std::nothrow) settings_save_t();
  if(!save) saveBusy.store(false);
  return save;
}
void freeSettingsSave(settings_save_t *save) {
  delete save;
  saveBusy.store(false);
}

/*********************************************************************
 * getSettings: what the settings routes sent, under the dictionary's
 * keys.  Secrets only say whether they are set.
 ********************************************************************/
// An address of 0.0.0.0 is sent as "", as the settings routes did.
static void addAddress(ProtoWriter &w, proto_field_t field, const IPAddress &ip) {
  if(ip != IPAddress(0, 0, 0, 0)) w.add(field, ip.toString().c_str());
}
static void writeSettings(ProtoWriter &w, settings_section_t section) {
  switch(section) {
    case settings_section_t::general:
      w.add(proto_field_t::hostname, settings.hostname);
      w.add(proto_field_t::ssdpBroadcast, settings.ssdpBroadcast);
      w.add(proto_field_t::ntpServer, settings.NTP.ntpServer);
      w.add(proto_field_t::posixZone, settings.NTP.posixZone);
      break;
    case settings_section_t::network:
      w.add(proto_field_t::connMode, static_cast<uint8_t>(settings.connType));
      w.add(proto_field_t::ssid, settings.WIFI.ssid);
      w.add(proto_field_t::hasPassphrase, hasSecret(settings.WIFI.passphrase));
      w.add(proto_field_t::roaming, settings.WIFI.roaming);
      w.add(proto_field_t::hidden, settings.WIFI.hidden);
#if HAS_ETHERNET
      w.add(proto_field_t::hasEthernet, true);
      w.add(proto_field_t::boardType, settings.Ethernet.boardType);
      w.add(proto_field_t::phyType, settings.Ethernet.phyType);
      w.add(proto_field_t::CLKMode, settings.Ethernet.CLKMode);
      w.add(proto_field_t::phyAddress, settings.Ethernet.phyAddress);
      w.add(proto_field_t::PWRPin, settings.Ethernet.PWRPin);
      w.add(proto_field_t::MDCPin, settings.Ethernet.MDCPin);
      w.add(proto_field_t::MDIOPin, settings.Ethernet.MDIOPin);
#endif
      w.add(proto_field_t::dhcp, settings.IP.dhcp);
      addAddress(w, proto_field_t::ip, settings.IP.ip);
      addAddress(w, proto_field_t::gateway, settings.IP.gateway);
      addAddress(w, proto_field_t::subnet, settings.IP.subnet);
      addAddress(w, proto_field_t::dns1, settings.IP.dns1);
      addAddress(w, proto_field_t::dns2, settings.IP.dns2);
      break;
    case settings_section_t::mqtt:
      w.add(proto_field_t::enabled, settings.MQTT.enabled);
      w.add(proto_field_t::pubDisco, settings.MQTT.pubDisco);
      w.add(proto_field_t::protocol, settings.MQTT.protocol);
      w.add(proto_field_t::hostname, settings.MQTT.hostname);
      w.add(proto_field_t::port, settings.MQTT.port);
      w.add(proto_field_t::username, settings.MQTT.username);
      w.add(proto_field_t::hasPassword, hasSecret(settings.MQTT.password));
      w.add(proto_field_t::rootTopic, settings.MQTT.rootTopic);
      w.add(proto_field_t::discoTopic, settings.MQTT.discoTopic);
      break;
    case settings_section_t::security:
      w.add(proto_field_t::securityType, static_cast<uint8_t>(settings.Security.type));
      w.add(proto_field_t::username, settings.Security.username);
      w.add(proto_field_t::hasPassword, hasSecret(settings.Security.password));
      w.add(proto_field_t::hasPin, hasSecret(settings.Security.pin));
      break;
    case settings_section_t::radio: {
      const transceiver_config_t &cfg = somfy.transceiver.config;
      w.add(proto_field_t::bitLength, cfg.type);
      w.add(proto_field_t::proto, static_cast<uint8_t>(cfg.proto));
      w.add(proto_field_t::enabled, cfg.enabled);
      w.add(proto_field_t::TXPin, cfg.TXPin);
      w.add(proto_field_t::RXPin, cfg.RXPin);
      w.add(proto_field_t::SCKPin, cfg.SCKPin);
      w.add(proto_field_t::MOSIPin, cfg.MOSIPin);
      w.add(proto_field_t::MISOPin, cfg.MISOPin);
      w.add(proto_field_t::CSNPin, cfg.CSNPin);
      w.add(proto_field_t::frequency, cfg.frequency);
      w.add(proto_field_t::rxBandwidth, cfg.rxBandwidth);
      w.add(proto_field_t::deviation, cfg.deviation);
      w.add(proto_field_t::txPower, cfg.txPower);
      w.add(proto_field_t::noiseDetection, cfg.noiseDetection);
      w.add(proto_field_t::radioInit, cfg.radioInit);
      break;
    }
  }
}

/*********************************************************************
 * saveSettings: what the settings routes do with their request
 ********************************************************************/
class RuntimeSettingsEffects : public SettingsEffects {
public:
  bool wifiClientsPresent() const override { return WiFi.softAPgetStationNum() != 0; }
  void hostnameChanged() override { net.updateHostname(); }
  void timeChanged() override { settings.NTP.apply(); }
  void mqttChanged() override { mqtt.disconnect(); }
  settings_save_result_t saveRadio(JsonObject request) override {
    transceiver_config_t next = somfy.transceiver.config;
    next.fromJSON(request);
    return somfy.transceiver.setConfig(next);
  }
};
static void runSave(const proto_reply_to_t &to, settings_save_t *save) {
  const settings_section_t section = save->section;
  const security_types securityBefore = settings.Security.type;
  const uint32_t ip = save->ip;
  RuntimeSettingsEffects effects;
  const auto result = applySettings(settings,section,save->request.as<JsonObject>(),effects);
  freeSettingsSave(save);
  if(!result.saved()) {
    protoSocket.replyRefused(to,proto_error_t::internal,settingsSaveFailureDescription(result.persistence.state));
    ESP_LOGE(TAG,"Socket [%lu] %s settings save failed%s",(unsigned long)to.client,settingsSectionName(section),
      result.persistence.state == settings_save_state_t::partial ? "; durable state may be partial" : "");
    return;
  }
  char token[API_TOKEN_SIZE] = "";
  if(section == settings_section_t::security && settings.requiresAuth()) webServer.createAPIToken(IPAddress(ip),token);
  const bool reboot = result.reboot;
  ProtoWriter w = protoSocket.beginReply(to);
  w.add(proto_field_t::apiKey, token);
  w.add(proto_field_t::rebooting, reboot);
  protoSocket.endReply(to);
  ESP_LOGI(TAG, "Socket [%lu] saved the %s settings%s", (unsigned long)to.client, settingsSectionName(section), reboot ? ", rebooting" : "");
  // Under Secured every token may have changed, and a client that connected
  // under None had none: they all connect again, the saver with the token in
  // its reply, and the rest log in when theirs is refused.  None to None
  // changes nobody's access.
  const bool closeAll = section == settings_section_t::security &&
    (securityBefore != settings.Security.type || settings.requiresAuth());
  if(closeAll) protoSocket.closeAll(to.client);
  else protoSocket.settingsChanged(to.client, settingsSectionName(section));
  if(reboot) {
    rebootDelay.rebootTime = millis() + 1000;
    rebootDelay.reboot = true;
  }
}

void runDeviceOp(const loop_item_t &item) {
  switch(static_cast<proto_type_t>(item.op)) {
    case proto_type_t::getSettings: {
      ProtoWriter w = protoSocket.beginReply(item.replyTo);
      writeSettings(w, static_cast<settings_section_t>(item.section));
      protoSocket.endReply(item.replyTo);
      break;
    }
    case proto_type_t::saveSettings:
      runSave(item.replyTo, item.save);
      break;
    case proto_type_t::scanWifi:
      // The networks follow as wifiNetworks once the scan is done.
      protoSocket.wantsWifiNetworks(item.replyTo.client);
      net.scanForClients();
      protoSocket.beginReply(item.replyTo);
      protoSocket.endReply(item.replyTo);
      break;
    case proto_type_t::beginFrequencyScan:
      if(!somfy.transceiver.config.enabled) {
        protoSocket.replyRefused(item.replyTo, proto_error_t::radioUnavailable, "The radio is disabled");
        break;
      }
      somfy.transceiver.beginFrequencyScan();
      protoSocket.beginReply(item.replyTo);
      protoSocket.endReply(item.replyTo);
      break;
    case proto_type_t::endFrequencyScan:
      somfy.transceiver.endFrequencyScan();
      protoSocket.beginReply(item.replyTo);
      protoSocket.endReply(item.replyTo);
      break;
    case proto_type_t::reboot:
      ESP_LOGI(TAG, "Socket [%lu] asked for a reboot", (unsigned long)item.replyTo.client);
      protoSocket.beginReply(item.replyTo);
      protoSocket.endReply(item.replyTo);
      // Time for the reply to go out.
      rebootDelay.rebootTime = millis() + 500;
      rebootDelay.reboot = true;
      break;
    default:
      if(item.save) freeSettingsSave(item.save);
      protoSocket.replyRefused(item.replyTo, proto_error_t::internal, "Not a device op");
      break;
  }
}
