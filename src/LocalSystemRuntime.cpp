#ifdef ESPSOMFY_LOCAL_UI
#include "LocalSystem.h"
#include "LocalOperationAdmission.h"
#include "SafeRestart.h"
namespace LocalUi { namespace System {
namespace {
class RuntimeBackend final : public Backend {
public:
  SecuritySnapshot authorization() override {
    auto current=pollAuthorization();current.allowed=current.allowed && !localControlsFrozen();return current;
  }
  void activity() override {authorizationActivity();}
  RestartResult restart(uint32_t session,uint32_t configuration) override {
    const auto current=authorization();
    if(!current.allowed || !session || current.generation!=session ||
       !configuration || current.configurationGeneration!=configuration)
      return {false,RestartReason::stale};
    return requestRestart();
  }
  RestartSnapshot status() override {return restartStatus();}
};
}
Backend &runtimeBackend(){static RuntimeBackend backend;return backend;}
} }
#endif
