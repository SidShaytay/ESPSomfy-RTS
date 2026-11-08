#include "SettingsDurability.h"
#include "ProtoSettings.h"
SettingsDurability &settingsDurability() { static SettingsDurability ledger;return ledger; }
SettingsNamespace settingsNamespace(settings_part_t part) {
  switch(part) {
    case settings_part_t::ntp: return SettingsNamespace::ntp;
    case settings_part_t::wifi: return SettingsNamespace::wifi;
    case settings_part_t::ethernet: return SettingsNamespace::ethernet;
    case settings_part_t::ip: return SettingsNamespace::ip;
    case settings_part_t::mqtt: return SettingsNamespace::mqtt;
    case settings_part_t::security: return SettingsNamespace::security;
    case settings_part_t::radio: return SettingsNamespace::radio;
    default: return SettingsNamespace::cfg;
  }
}
uint8_t settingsNamespaces(uint16_t parts) {
  uint8_t result=0;
  for(uint8_t part=0;part<=static_cast<uint8_t>(settings_part_t::radio);++part)
    if(parts&(1u<<part)) result|=settingsNamespaceBit(settingsNamespace(static_cast<settings_part_t>(part)));
  return result;
}
const char *settingsDurabilityDescription() {
  return "Settings saved; restart blocked. Save current RUNNING settings to reconcile uncertain storage; no rollback was performed.";
}
