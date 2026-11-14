#ifdef ESPSOMFY_LOCAL_UI
#include "LocalCalibration.h"
#include "LocalEntityRuntime.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LocalOperationAdmission.h"
#include "LocalInputSource.h"
#include "LiftCalibrationService.h"
#include <Arduino.h>
namespace LocalUi { namespace Calibration {
namespace {
class RuntimeBackend final : public Backend {
public:
  EntityAdmin::Authorization authorization() override {
    const auto auth = pollAuthorization();
    return {auth.allowed && !localControlsFrozen(),auth.generation,auth.configurationGeneration};
  }
  uint64_t revision() override { return EntityAdmin::runtimeBackend().revision(); }
  void activity() override { authorizationActivity(); }
  bool available(uint8_t id) override { return LiftCal::available(id); }
  bool begin(uint8_t id, LiftKind kind, EntityAdmin::Authorization auth, uint32_t &token) override {
    return LiftCal::begin(LiftCal::localSource(auth.generation,auth.configurationGeneration),id,kind,token);
  }
  bool act(uint32_t token, LiftGuideAction action, EntityAdmin::Authorization auth, bool human) override {
    return LiftCal::act(LiftCal::localSource(auth.generation,auth.configurationGeneration),token,action,
      human && !automatedInput(uint32_t(millis())));
  }
  bool snapshot(uint32_t token, EntityAdmin::Authorization auth, LiftGuideSnapshot &out) override {
    return LiftCal::snapshot(LiftCal::localSource(auth.generation,auth.configurationGeneration),token,out);
  }
};
}
Backend &runtimeBackend() { static RuntimeBackend backend; return backend; }
} }
#endif
