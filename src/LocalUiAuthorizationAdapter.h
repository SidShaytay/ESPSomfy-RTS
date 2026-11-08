#pragma once
#include "LocalAuthorization.h"
namespace LocalUi {
struct SecuritySnapshot {
  AuthMode mode = AuthMode::unavailable;
  bool allowed = false, stopGrace = false;
  uint32_t generation = 0, configurationGeneration = 0, retryAfterMs = 0;
  // Original active generation captured at inactivity expiry, never inferred
  // from the current revision (failed unlocks also advance that revision).
  uint32_t expiredSessionGeneration = 0;
};
// Loop-owned only. No renderable snapshot contains a configured or typed secret.
SecuritySnapshot pollAuthorization();
AuthResult unlockAuthorization(Credentials &);
void authorizationActivity();
void lockAuthorization();
} // namespace LocalUi
