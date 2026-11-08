#pragma once
#include "ConfigSettings.h"
#include "ProtoSettings.h"

enum class settings_apply_refusal_t : uint8_t {
  none, invalidPin, usernameRequired, passwordRequired, unauthorized, stale
};
struct settings_apply_result_t {
  settings_save_result_t persistence;
  uint16_t persistedParts = 0;
  bool reboot = false;
  settings_apply_refusal_t refusal = settings_apply_refusal_t::none;
  bool saved() const { return refusal == settings_apply_refusal_t::none && persistence.saved(); }
  static settings_apply_result_t refused(settings_apply_refusal_t reason) {
    settings_apply_result_t result;
    result.persistence.state = settings_save_state_t::failed;
    result.refusal = reason;
    return result;
  }
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
  // Runs once after complete persistence/live apply and the reboot decision.
  // Failed or partial saves never notify, revoke clients or schedule restart.
  virtual void completed(settings_section_t section, bool reboot) = 0;
};
// loop() only, after expandSettingsPayload has validated/expanded the request.
// Failed/partial persistence leaves the in-memory settings and runtime effects
// unchanged, but durable keys may have changed. No transactional rollback claim.
settings_apply_result_t applySettings(ConfigSettings &current, settings_section_t section, JsonObject request, SettingsEffects &effects);
const char *settingsSaveFailureDescription(settings_save_state_t state);
const char *settingsApplyFailureDescription(const settings_apply_result_t &result);
