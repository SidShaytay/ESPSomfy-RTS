#include "SettingsApply.h"
#include "Secrets.h"
#include "SettingsDurability.h"

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
    wipeSecret(value.MQTT.username);
    wipeSecret(value.MQTT.password);
    wipeSecret(value.Security.username);
    wipeSecret(value.Security.password);
    wipeSecret(value.Security.pin);
  }
};
static_assert(sizeof(ConfigSettings) <= 2048, "Keep the one settings draft bounded");
settings_apply_refusal_t securityRefusal(const SecuritySettings &before, const SecuritySettings &next) {
  // Preserve an unchanged legacy configuration; newly enabling/changing active
  // authentication must have usable effective credentials. Do not trim bytes.
  const bool modeChanged = before.type != next.type;
  if(next.type == security_types::PinEntry &&
     (modeChanged || strcmp(before.pin,next.pin) != 0) && strlen(next.pin) != 4)
    return settings_apply_refusal_t::invalidPin;
  if(next.type == security_types::Password &&
     (modeChanged || strcmp(before.username,next.username) != 0 || strcmp(before.password,next.password) != 0)) {
    if(!hasSecret(next.username)) return settings_apply_refusal_t::usernameRequired;
    if(!hasSecret(next.password)) return settings_apply_refusal_t::passwordRequired;
  }
  return settings_apply_refusal_t::none;
}
}
const char *settingsSaveFailureDescription(settings_save_state_t state) {
  return state == settings_save_state_t::partial ?
    "Settings may be partly persisted; running settings unchanged. Retry before restart; no rollback was performed." :
    "Settings were not saved; running settings unchanged.";
}
const char *settingsApplyFailureDescription(const settings_apply_result_t &result) {
  switch(result.refusal) {
    case settings_apply_refusal_t::invalidPin: return "PIN must contain exactly four bytes; nothing was saved.";
    case settings_apply_refusal_t::usernameRequired: return "Password security requires a username; nothing was saved.";
    case settings_apply_refusal_t::passwordRequired: return "Password security requires a password; nothing was saved.";
    case settings_apply_refusal_t::unauthorized: return "Unlock again before saving; nothing was saved.";
    case settings_apply_refusal_t::stale: return "Settings changed; reopen the form before saving.";
    case settings_apply_refusal_t::busy: return "Actions are queued; retry Save when they finish. Nothing was saved.";
    default: return settingsSaveFailureDescription(result.persistence.state);
  }
}
settings_apply_result_t applySettings(ConfigSettings &current, settings_section_t section, JsonObject request, SettingsEffects &effects) {
  settings_apply_result_t out;
  const uint16_t parts = settingsParts(section,request);
  if(!parts) { out.persistence = {settings_save_state_t::failed,"request"}; return out; }
  if(section == settings_section_t::radio) {
    out.persistence = effects.saveRadio(request);
    if(out.saved()) {
      out.persistedParts = parts;
      out.restartBlocked=!settingsDurability().status().restartAllowed();
      if(out.restartBlocked) out.restartReason=RestartReason::settingsUncertain;
      effects.completed(section,false);
    }
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
  if(settingsSets(parts,settings_part_t::security)) {
    const auto refusal = securityRefusal(current.Security,next.Security);
    if(refusal != settings_apply_refusal_t::none) return settings_apply_result_t::refused(refusal);
  }
  auto persist = [&](settings_part_t part, auto &value) {
    if(!settingsSets(parts,part)) return true;
    out.persistence = value.save();
    if(!out.saved()) {
      if(out.persistedParts) {
        out.persistence.state = settings_save_state_t::partial;
        // Earlier complete draft writes differ from unchanged running values.
        settingsDurability().markUncertain(settingsNamespaces(out.persistedParts));
      }
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
  out.restartBlocked=!settingsDurability().status().restartAllowed();
  if(out.restartBlocked) { out.reboot=false;out.restartReason=RestartReason::settingsUncertain; }
  const bool requested=out.reboot;
  const auto completion=effects.completed(section,requested);
  if(requested) {
    out.reboot=completion.admitted;
    if(!out.reboot) {
      out.restartBlocked=true;
      out.restartReason=completion.reason==RestartReason::none ? RestartReason::notReady : completion.reason;
    }
  }
  return out;
}
