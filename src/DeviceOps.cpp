#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <new>
#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#else
#include <mutex>
#endif
#include "esp_log.h"
#include "DeviceOps.h"
#include "SettingsApply.h"
#include "SelectedSettingsRestore.h"
#include "PinOwnership.h"
#include "SettingsCompletion.h"
#include "SafeRestart.h"
#include "SettingsDurability.h"
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

static std::atomic<bool> saveBusy(false);
// Synchronized settings history: bounded memory only, never settings/SDK I/O.
namespace SettingsHistoryDetail {
SettingsRevisions revisions;
#if defined(ESP_PLATFORM)
portMUX_TYPE mutex=portMUX_INITIALIZER_UNLOCKED;
struct Guard { Guard(){portENTER_CRITICAL(&mutex);}~Guard(){portEXIT_CRITICAL(&mutex);} };
#else
std::mutex mutex;
struct Guard { std::lock_guard<std::mutex> lock;Guard():lock(mutex){} };
#endif
}
uint64_t deviceSettingsRevision(settings_section_t section) {
  SettingsHistoryDetail::Guard guard;return SettingsHistoryDetail::revisions.get(section);
}
uint64_t securityConfigurationRevision() { return deviceSettingsRevision(settings_section_t::security); }
void invalidateDeviceSettingsRevisions() {
  SettingsHistoryDetail::Guard guard;SettingsHistoryDetail::revisions.invalidateAll();
}
void noteDeviceSettingsCompleted(settings_section_t section) {
  SettingsHistoryDetail::Guard guard;SettingsHistoryDetail::revisions.completed(section);
}
// End synchronized settings history.

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
class ProtocolSettingsSink : public SettingsSnapshotSink {
  ProtoWriter &writer;
public:
  explicit ProtocolSettingsSink(ProtoWriter &writer) : writer(writer) {}
  void field(proto_field_t field, const settings_value_t &value) override {
    switch(value.kind) {
      case 'b': writer.add(field,value.boolean); break;
      case 'i': writer.add(field,value.integer); break;
      case 'n': writer.add(field,value.number); break;
      case 's': writer.add(field,value.text); break;
    }
  }
};
bool readDeviceSettings(settings_section_t section, SettingsSnapshotSink &sink) {
  return readSettingsSnapshot(section,settings,somfy.transceiver.config,sink);
}

/*********************************************************************
 * saveSettings: what the settings routes do with their request
 ********************************************************************/
class RuntimeSettingsEffects : public SettingsEffects {
  const security_types securityBefore;
  const uint32_t originSocketClient;
public:
  explicit RuntimeSettingsEffects(uint32_t origin) : securityBefore(settings.Security.type), originSocketClient(origin) {}
  RestartResult completed(settings_section_t section, bool reboot) override {
    noteDeviceSettingsCompleted(section);
    return completeSettingsSave({section,securityBefore,settings.Security.type,originSocketClient,reboot},protoSocket,requestRestart);
  }
  bool networkPinsAvailable(const ConfigSettings &candidate) const override {return networkPinConfigurationAvailable(candidate);}
  bool wifiClientsPresent() const override { return WiFi.softAPgetStationNum() != 0; }
  void hostnameChanged() override { net.updateHostname(); }
  void timeChanged() override { settings.NTP.apply(); }
  void mqttChanged() override { mqtt.disconnect();mqtt.lastConnect=0; }
  settings_save_result_t saveRadio(JsonObject request) override {
    transceiver_config_t next = somfy.transceiver.config;
    next.fromJSON(request);
    const auto result=somfy.transceiver.setConfig(next);
    if(result.liveApplied && !result.saved())noteDeviceSettingsCompleted(settings_section_t::radio);
    return result;
  }
};
RestoreSettingsResult applyPreparedSettingsRestore(SelectedSettingsRestore &stage,Transceiver &radio,
    const GitUpdateOwnerToken &owner,const RadioInstallToken &pause) {
  RuntimeSettingsEffects effects(0);return stage.apply(settings,radio,owner,pause,effects);
}
settings_apply_result_t applyDeviceSettings(settings_section_t section, JsonObject request, uint32_t originSocketClient) {
  RuntimeSettingsEffects effects(originSocketClient);
  return applySettings(settings,section,request,effects);
}
settings_reconcile_result_t reconcileRunningDeviceSettings(uint32_t originSocketClient) {
  RuntimeSettingsEffects effects(originSocketClient);
  return reconcileRunningSettings(settings,somfy.transceiver.config,effects);
}
static void runSave(const proto_reply_to_t &to, settings_save_t *save) {
  const settings_section_t section = save->section;
  const uint32_t ip = save->ip;
  const auto result = applyDeviceSettings(section,save->request.as<JsonObject>(),to.client);
  freeSettingsSave(save);
  if(!result.saved()) {
    const auto code = result.refusal == settings_apply_refusal_t::unauthorized ? proto_error_t::unauthorized :
      result.refusal == settings_apply_refusal_t::stale ? proto_error_t::busy :
      result.refusal != settings_apply_refusal_t::none ? proto_error_t::badRequest : proto_error_t::internal;
    protoSocket.replyRefused(to,code,settingsApplyFailureDescription(result));
    ESP_LOGE(TAG,"Socket [%lu] %s settings save refused or failed%s",(unsigned long)to.client,settingsSectionName(section),
      result.persistence.state == settings_save_state_t::partial ? "; durable state may be partial" : "");
    return;
  }
  char token[API_TOKEN_SIZE] = "";
  if(section == settings_section_t::security && settings.requiresAuth()) webServer.createAPIToken(IPAddress(ip),token);
  const bool reboot = result.reboot;
  ProtoWriter w = protoSocket.beginReply(to);
  w.add(proto_field_t::apiKey, token);
  w.add(proto_field_t::rebooting, reboot);
  if(result.restartBlocked) w.add(proto_field_t::desc,restartReason(result.restartReason));
  protoSocket.endReply(to);
  ESP_LOGI(TAG, "Socket [%lu] saved the %s settings%s", (unsigned long)to.client, settingsSectionName(section), reboot ? ", rebooting" : "");
  // Completion is shared with local saves. closeAll(origin) defers this
  // socket until its reply above drains, so its replacement token is delivered.
}

void runDeviceOp(const loop_item_t &item) {
  switch(static_cast<proto_type_t>(item.op)) {
    case proto_type_t::getSettings: {
      ProtoWriter w = protoSocket.beginReply(item.replyTo);
      ProtocolSettingsSink sink(w);
      readDeviceSettings(static_cast<settings_section_t>(item.section),sink);
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
      if(!somfy.transceiver.beginFrequencyScan()) {
        protoSocket.replyRefused(item.replyTo,proto_error_t::busy,"Radio scan admission refused; accepted work was preserved");
        break;
      }
      protoSocket.beginReply(item.replyTo);
      protoSocket.endReply(item.replyTo);
      break;
    case proto_type_t::endFrequencyScan:
      somfy.transceiver.endFrequencyScan();
      protoSocket.beginReply(item.replyTo);
      protoSocket.endReply(item.replyTo);
      break;
    case proto_type_t::reboot: {
      const auto admission=requestRestart(500);
      if(!admission.admitted) {
        protoSocket.replyRefused(item.replyTo,proto_error_t::busy,restartReason(admission.reason));break;
      }
      protoSocket.beginReply(item.replyTo);protoSocket.endReply(item.replyTo);break;
    }
    default:
      if(item.save) freeSettingsSave(item.save);
      protoSocket.replyRefused(item.replyTo, proto_error_t::internal, "Not a device op");
      break;
  }
}
