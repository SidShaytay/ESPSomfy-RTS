#ifdef ESPSOMFY_LOCAL_UI
#include "LocalOperationAdmission.h"
#include "LocalWifiScan.h"
#include "ProtoSettings.h"
#include "SettingsRevisions.h"
#include "esp_timer.h"
namespace LocalUi {
namespace WifiScan {
namespace {
class RuntimeBackend final : public Backend {
public:
  SecuritySnapshot authorization() override {
    auto a = pollAuthorization();
    a.allowed = a.allowed && !localControlsFrozen();
    return a;
  }
  uint64_t networkRevision() override {
    return deviceSettingsRevision(settings_section_t::network);
  }
  uint64_t now() override { return uint64_t(esp_timer_get_time()) / 1000; }
  void activity() override { authorizationActivity(); }
  WifiScanFeed &feed() override { return wifiScanFeed(); }
};
} // namespace
Backend &runtimeBackend() {
  static RuntimeBackend value;
  return value;
}
} // namespace WifiScan
} // namespace LocalUi
#endif
