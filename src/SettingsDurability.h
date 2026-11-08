#pragma once
#include <stdint.h>
#include "SettingsSaveResult.h"

enum class settings_part_t : uint8_t;
// Persisted namespaces, not UI sections. General and connection share CFG.
enum class SettingsNamespace : uint8_t { cfg, ntp, wifi, ethernet, ip, mqtt, security, radio };
constexpr uint8_t settingsNamespaceBit(SettingsNamespace space) { return 1u<<static_cast<uint8_t>(space); }
SettingsNamespace settingsNamespace(settings_part_t part);
uint8_t settingsNamespaces(uint16_t parts);
struct SettingsDurabilityStatus {
  uint8_t uncertain=0;
  uint64_t epoch=1;
  bool exhausted=false;
  bool restartAllowed() const { return !uncertain && !exhausted; }
};
// Loop/startup owned. Contains no stored settings, credentials or NVS handles.
// This records known partial writes during this boot, not crash recovery.
class SettingsDurability {
  SettingsDurabilityStatus value;
public:
  SettingsDurabilityStatus status() const { return value; }
  void markUncertain(uint8_t spaces) {
    if(!spaces) return;
    value.uncertain|=spaces;
    if(value.epoch==UINT64_MAX) value.exhausted=true;
    else ++value.epoch;
  }
  // Only the complete checked writer may declare a namespace durable.
  void fullSave(SettingsNamespace space,settings_save_result_t result) {
    if(result.saved()) value.uncertain&=~settingsNamespaceBit(space);
    else if(result.state==settings_save_state_t::partial) markUncertain(settingsNamespaceBit(space));
  }
};
SettingsDurability &settingsDurability();
const char *settingsDurabilityDescription();
