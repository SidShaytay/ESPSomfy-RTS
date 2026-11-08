#include "SettingsApply.h"
#include "Secrets.h"

namespace {
struct SettingsDraft {
  ConfigSettings value;
  explicit SettingsDraft(const ConfigSettings &source) {
    // Only persisted fields belong in the draft; omit boot/status metadata.
    strcpy(value.hostname,source.hostname);
    value.ssdpBroadcast=source.ssdpBroadcast;
    value.checkForUpdate=source.checkForUpdate;
    value.connType=source.connType;
    value.NTP=source.NTP;
    value.WIFI=source.WIFI;
#if HAS_ETHERNET
    value.Ethernet=source.Ethernet;
#endif
    value.IP=source.IP;
    value.MQTT=source.MQTT;
    value.Security=source.Security;
  }
  ~SettingsDraft() {
    wipeSecret(value.WIFI.passphrase);
    wipeSecret(value.MQTT.password);
    wipeSecret(value.Security.password);
    wipeSecret(value.Security.pin);
  }
};
static_assert(sizeof(ConfigSettings) <= 2048, "Keep the one settings draft bounded");
}
const char *settingsSaveFailureDescription(settings_save_state_t state) {
  return state == settings_save_state_t::partial ?
    "Settings may be partly persisted; running settings unchanged. Retry before restart; no rollback was performed." :
    "Settings were not saved; running settings unchanged.";
}
settings_apply_result_t applySettings(ConfigSettings &current, settings_section_t section, JsonObject request, SettingsEffects &effects) {
  settings_apply_result_t out;
  const uint16_t parts = settingsParts(section,request);
  if(!parts) { out.persistence = {settings_save_state_t::failed,"request"}; return out; }
  if(section == settings_section_t::radio) {
    out.persistence = effects.saveRadio(request);
    if(out.saved()) out.persistedParts = parts;
    return out;
  }
  SettingsDraft draft(current);
  auto &next = draft.value;
  if(settingsSets(parts,settings_part_t::general) || settingsSets(parts,settings_part_t::conn)) next.fromJSON(request);
  if(settingsSets(parts,settings_part_t::ntp)) next.NTP.fromJSON(request);
  if(settingsSets(parts,settings_part_t::wifi)) next.WIFI.fromJSON(request);
#if HAS_ETHERNET
  if(settingsSets(parts,settings_part_t::ethernet)) next.Ethernet.fromJSON(request);
#endif
  if(settingsSets(parts,settings_part_t::ip)) next.IP.fromJSON(request);
  if(settingsSets(parts,settings_part_t::mqtt)) next.MQTT.fromJSON(request);
  if(settingsSets(parts,settings_part_t::security)) next.Security.fromJSON(request);
  auto persist = [&](settings_part_t part, auto &value) {
    if(!settingsSets(parts,part)) return true;
    out.persistence = value.save();
    if(!out.saved()) {
      if(out.persistedParts) out.persistence.state = settings_save_state_t::partial;
      return false;
    }
    out.persistedParts |= 1u << static_cast<uint8_t>(part);
    return true;
  };
  if(!persist(settings_part_t::general,next) || !persist(settings_part_t::ntp,next.NTP) ||
     !persist(settings_part_t::conn,next) || !persist(settings_part_t::wifi,next.WIFI) ||
#if HAS_ETHERNET
     !persist(settings_part_t::ethernet,next.Ethernet) ||
#endif
     !persist(settings_part_t::ip,next.IP) || !persist(settings_part_t::mqtt,next.MQTT) ||
     !persist(settings_part_t::security,next.Security)) return out;

  // Commit only selected RAM fields; similarly named fields in other sections
  // stay untouched. External effects happen only after the complete save.
  if(settingsSets(parts,settings_part_t::general)) {
    strcpy(current.hostname,next.hostname);
    current.ssdpBroadcast=next.ssdpBroadcast;
  }
  const bool connectionChanged = next.connType != current.connType;
  if(settingsSets(parts,settings_part_t::conn)) current.connType=next.connType;
  if(settingsSets(parts,settings_part_t::ntp)) current.NTP=next.NTP;
  bool wifiChanged = false;
  if(settingsSets(parts,settings_part_t::wifi)) {
    wifiChanged = strcmp(current.WIFI.ssid,next.WIFI.ssid)!=0 || strcmp(current.WIFI.passphrase,next.WIFI.passphrase)!=0;
    current.WIFI=next.WIFI;
  }
#if HAS_ETHERNET
  if(settingsSets(parts,settings_part_t::ethernet)) current.Ethernet=next.Ethernet;
#endif
  if(settingsSets(parts,settings_part_t::ip)) current.IP=next.IP;
  if(settingsSets(parts,settings_part_t::mqtt)) current.MQTT=next.MQTT;
  if(settingsSets(parts,settings_part_t::security)) current.Security=next.Security;
  if(settingsSets(parts,settings_part_t::general) && !request["hostname"].isNull()) effects.hostnameChanged();
  if(settingsSets(parts,settings_part_t::ntp)) effects.timeChanged();
  if(settingsSets(parts,settings_part_t::mqtt)) effects.mqttChanged();
  out.reboot = settingsSets(parts,settings_part_t::conn) && connectionChanged;
  if(settingsSets(parts,settings_part_t::wifi) && current.connType==conn_types_t::wifi && wifiChanged && !effects.wifiClientsPresent()) out.reboot=true;
#if HAS_ETHERNET
  if(settingsSets(parts,settings_part_t::ethernet) && (current.connType==conn_types_t::ethernet || current.connType==conn_types_t::ethernetpref)) out.reboot=true;
#endif
  return out;
}
