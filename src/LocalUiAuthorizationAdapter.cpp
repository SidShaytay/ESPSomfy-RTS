#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUiAuthorizationAdapter.h"
#include "ConfigSettings.h"
#include <esp_timer.h>
extern ConfigSettings settings;
namespace LocalUi {
namespace {
Authorization authorization;
uint64_t now() { return uint64_t(esp_timer_get_time()) / 1000; }
void sync() {
  const auto &security = settings.Security;
  authorization.configure(AuthMode(uint8_t(security.type)), security.username,
                          security.type == security_types::PinEntry ? security.pin : security.password);
}
}
SecuritySnapshot pollAuthorization() {
  sync();
  const uint64_t time = now();
  const bool allowed = authorization.allowed(time);
  return {authorization.configuredMode(), allowed, authorization.expired(), authorization.generation(),
          authorization.configurationGeneration(), authorization.retryAfter(time)};
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
