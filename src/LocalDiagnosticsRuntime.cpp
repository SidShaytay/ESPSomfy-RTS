#ifdef ESPSOMFY_LOCAL_UI
#include "LocalDiagnostics.h"
#include "LocalOperationAdmission.h"
#include "esp_timer.h"
namespace LocalUi { namespace Diagnostics {
namespace {
class RuntimeBackend final:public Backend {
public:
  SecuritySnapshot authorization()override{
    auto auth=pollAuthorization();auth.allowed=auth.allowed && !localControlsFrozen();return auth;
  }
  uint64_t now()override{return uint64_t(esp_timer_get_time())/1000;}
  void activity()override{authorizationActivity();}
  ReceiveHub &hub()override{return receiveHub();}
};
}
Backend &runtimeBackend(){static RuntimeBackend backend;return backend;}
} }
#endif
