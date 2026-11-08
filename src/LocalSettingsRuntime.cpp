#ifdef ESPSOMFY_LOCAL_UI
#include "LocalSettingsRuntime.h"
#include "LocalUiAuthorizationAdapter.h"
#include "DeviceOps.h"
#include "LocalOperationAdmission.h"
namespace LocalUi { namespace Settings {
namespace {
class RuntimeBackend final : public Backend {
public:
  Authorization authorization() override {
    const auto state=pollAuthorization();
    return {state.allowed && !localControlsFrozen(),state.generation,state.configurationGeneration};
  }
  uint64_t revision(settings_section_t section) override { return deviceSettingsRevision(section); }
  bool read(settings_section_t section,SettingsSnapshotSink &sink) override {
    return authorization().allowed && readDeviceSettings(section,sink);
  }
  settings_apply_result_t apply(settings_section_t section,JsonObject request,
                                Authorization expected,uint64_t expectedRevision) override {
    const auto current=authorization();
    if(!expected.allowed || !current.allowed) {
      return settings_apply_result_t::refused(settings_apply_refusal_t::unauthorized);
    }
    if(!expected.generation || !expected.configurationGeneration || !expectedRevision ||
       current.generation!=expected.generation || current.configurationGeneration!=expected.configurationGeneration ||
       revision(section)!=expectedRevision) {
      return settings_apply_result_t::refused(settings_apply_refusal_t::stale);
    }
    // Form supplied the shared validator's expanded request. Recheck ownership
    // immediately before the loop-owned domain call; no HTTP/socket loopback.
    return applyDeviceSettings(section,request);
  }
  bool hasEthernet() const override { return HAS_ETHERNET; }
  bool fixedRadioPins() const override {
#ifdef ESPSOMFY_LILYGO_T_EMBED_CC1101
    return true;
#else
    return false;
#endif
  }
  void activity() override { authorizationActivity(); }
};
}
Backend &runtimeBackend() { static RuntimeBackend backend; return backend; }
} }
#endif
