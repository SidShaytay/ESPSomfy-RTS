#pragma once
#include "ConfigSettings.h"
#include "ProtoSettings.h"

struct settings_apply_result_t {
  settings_save_result_t persistence;
  uint16_t persistedParts = 0;
  bool reboot = false;
  bool saved() const { return persistence.saved(); }
};
// Runtime integration belongs to loop(), outside persistence/value parsing.
// Each hook runs only after all requested settings have persisted successfully.
class SettingsEffects {
public:
  virtual ~SettingsEffects() = default;
  virtual bool wifiClientsPresent() const = 0;
  virtual void hostnameChanged() = 0;
  virtual void timeChanged() = 0;
  virtual void mqttChanged() = 0;
  // Radio owns its lock and must persist a normalized copy before live apply.
  virtual settings_save_result_t saveRadio(JsonObject request) = 0;
};
// loop() only, after expandSettingsPayload has validated/expanded the request.
// Failed/partial persistence leaves the in-memory settings and runtime effects
// unchanged, but durable keys may have changed. No transactional rollback claim.
settings_apply_result_t applySettings(ConfigSettings &current, settings_section_t section, JsonObject request, SettingsEffects &effects);
const char *settingsSaveFailureDescription(settings_save_state_t state);
