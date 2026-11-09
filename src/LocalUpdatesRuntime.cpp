#ifdef ESPSOMFY_LOCAL_UI
#include "LocalUpdates.h"
#include "GitUpdateController.h"
#include "LocalOperationAdmission.h"
namespace LocalUi {namespace Updates {namespace {
class RuntimeBackend final:public Backend {
public:
  SecuritySnapshot authorization()override{auto value=pollAuthorization();value.allowed=value.allowed && !localControlsFrozen();return value;}
  void activity()override{authorizationActivity();}
  GitUpdateSnapshot status()override{return gitUpdate.snapshot();}
  GitUpdateBootStatus boot()override{return gitUpdateBootStatus();}
  GitUpdateRequest check(uint32_t session,uint32_t security,uint64_t &operation)override{return requestLocalGitUpdateCheck(session,security,operation);}
  GitUpdateRequest install(uint64_t candidate,uint32_t session,uint32_t security,uint64_t &operation)override{
    const auto current=gitUpdate.snapshot();
    if(!candidate || candidate!=current.candidate.generation)return GitUpdateRequest::invalidCandidate;
    return requestLocalGitUpdateInstall(current.candidate,session,security,operation);
  }
  bool cancel(uint64_t operation,uint32_t session,uint32_t security)override{return cancelLocalGitUpdate(session,security,operation);}
};
}
Backend &runtimeBackend(){static RuntimeBackend backend;return backend;}
}}
#endif
