#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUiAuthorizationAdapter.h"
#include "ConfigSettings.h"
#include "SettingsRevisions.h"
#include <esp_timer.h>
#include <string.h>
extern ConfigSettings settings;
namespace LocalUi {
namespace {
Authorization authorization;
// Detect every security-setting replacement, including inactive credentials.
// This private bounded cache is never returned, hashed, rendered or logged.
struct CachedSecurity {
  uint8_t type=0;
  char username[33]={}, password[33]={}, pin[5]={};
  void clear() { wipe(this,sizeof(*this)); }
  ~CachedSecurity() { clear(); }
} previous;
bool initialized=false;
uint64_t previousRevision=0;
uint64_t now() { return uint64_t(esp_timer_get_time()) / 1000; }
void sync() {
  const auto &security = settings.Security;
  const uint64_t revision=securityConfigurationRevision();
  uint8_t different=previous.type ^ uint8_t(security.type);
  for(size_t i=0;i<sizeof(previous.username);i++) {
    different|=uint8_t(previous.username[i]^security.username[i]);
    different|=uint8_t(previous.password[i]^security.password[i]);
  }
  for(size_t i=0;i<sizeof(previous.pin);i++) different|=uint8_t(previous.pin[i]^security.pin[i]);
  if(!initialized || different || revision!=previousRevision) {
    authorization.configurationChanged();
    previous.clear();
    previous.type=uint8_t(security.type);
    memcpy(previous.username,security.username,sizeof(previous.username));
    memcpy(previous.password,security.password,sizeof(previous.password));
    memcpy(previous.pin,security.pin,sizeof(previous.pin));
    previousRevision=revision;
    initialized=true;
  }
  authorization.configure(AuthMode(uint8_t(security.type)), security.username,
                          security.type == security_types::PinEntry ? security.pin : security.password);
}
}
SecuritySnapshot pollAuthorization() {
  sync();
  const uint64_t time = now();
  const bool allowed = authorization.allowed(time);
  return {authorization.configuredMode(), allowed, authorization.stopGraceAllowed(), authorization.generation(),
          authorization.configurationGeneration(), authorization.retryAfter(time), authorization.expiredSessionGeneration()};
}
AuthResult unlockAuthorization(Credentials &given) {
  sync();
  const auto result = authorization.unlock(given, now());
  given.clear();
  return result;
}
void authorizationActivity() { sync(); authorization.activity(now()); }
void lockAuthorization() { authorization.lock(); }
} // namespace LocalUi
#endif
