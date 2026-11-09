#pragma once
#include <stdint.h>

// NVS updates use the existing per-key format. They are not transactions:
// a failed/partial result may leave durable values for the next retry/reboot.
enum class settings_save_state_t : uint8_t { saved, failed, partial };
struct settings_save_result_t {
  settings_save_state_t state;
  bool liveApplied; // A later radio-marker failure cannot claim RAM stayed unchanged.
  bool radioCleanupFault; // Exact resource capability became stale; no automatic retry.
  bool radioNamespaceVerified; // Full radio namespace verified, independently of live adoption.
  const char *failedKey; // schema key only; never a stored value
  constexpr settings_save_result_t(settings_save_state_t value=settings_save_state_t::saved,
      const char *key=nullptr,bool applied=false,bool cleanupFault=false,bool radioVerified=false):
      state(value),liveApplied(applied),radioCleanupFault(cleanupFault),radioNamespaceVerified(radioVerified),failedKey(key) {}
  bool saved() const { return state == settings_save_state_t::saved && !radioCleanupFault; }
};

static_assert(sizeof(settings_save_result_t)<=2*sizeof(const char *),"Keep persistence results within their original pointer-aligned budget");

// Legacy source cleanup is optional after the destination is verified. Failure
// to remove obsolete source keys does not make the saved CFG uncertain.
struct settings_migration_result_t {
  settings_save_result_t persistence;
  bool sourceCleaned=false;
  bool saved() const { return persistence.saved(); }
};
