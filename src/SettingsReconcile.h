#pragma once
#include "SettingsApply.h"
#include "SettingsDurability.h"
struct transceiver_config_t;
struct settings_reconcile_result_t {
  settings_save_result_t persistence;
  uint8_t requested=0,reconciled=0;
  SettingsDurabilityStatus durability;
  bool saved() const { return persistence.saved() && durability.restartAllowed(); }
};
// Loop owned, after fresh administrator admission. Persists CURRENT RUNNING
// values only, not a draft/backup. No runtime effects or restart; successful
// namespace rewrites notify completion/history even if another write fails.
settings_reconcile_result_t reconcileRunningSettings(ConfigSettings &current, const transceiver_config_t &radio, SettingsEffects &effects);
