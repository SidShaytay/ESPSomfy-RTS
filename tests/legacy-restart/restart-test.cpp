#include "Arduino.h"
#include "Utils.h"
#include "GitUpdate.h"
#include "ManualUpdateLifecycle.h"
#include "LegacyRestartSource.h"
#include "esp_timer.h"
#include <assert.h>
#include <string>
#define SOMFY_H
struct SomfyShadeController {struct {bool uncertain=false;bool counterEditUncertain() const{return uncertain;}} transceiver;} somfy;
rebootDelay_t rebootDelay;
static unsigned shutdowns=0;
static unsigned long at=100;
static unsigned long clockNow(){return at;}
void performLegacyShutdown() {
  ++shutdowns;assert(gitUpdate.busy());GitUpdateOwnerToken competing;
  assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,competing));
}
#define millis clockNow
#include "../../src/LegacyRestartGuard.cpp"
#undef millis
int main(int argc,char **argv) {
  assert(argc==2);std::string mode=argv[1];stubEspTimerMicros=100000;
  GitUpdateOwnerToken source;
  if(mode=="manual" || mode=="expired" || mode=="handoff") {
    assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,source));
    assert(gitUpdate.beginTransfer(source,ManualUpdateKind::firmware,100));
    assert(gitUpdate.claimTransfer(source,100));assert(gitUpdate.transferResource(source,true));
    if(mode!="manual") {
      assert(gitUpdate.transferResource(source,false));assert(gitUpdate.holdTransferRestart(source));
      assert(gitUpdate.finishTransfer(source,100,true));
    }
  }
  if(mode=="expired") {
    const auto cleanup=gitUpdate.claimTransferCleanup(30100);
    assert(gitUpdate.closeTransfer(cleanup.token));assert(gitUpdate.releaseExternal(cleanup.token));
    assert(publishExternalLegacyRestart(rebootDelay,0,source.generation,uint8_t(source.kind)).admitted);
  } else if(mode=="handoff") assert(publishExternalLegacyRestart(rebootDelay,0,source.generation,uint8_t(source.kind)).admitted);
  else assert(publishGenericLegacyRestart(rebootDelay,0).admitted);
  if(mode=="counter") somfy.transceiver.uncertain=true;
  const bool restarted=legacyRestartLoop();
  const bool expected=mode=="allowed" || mode=="handoff";
  assert(restarted==expected && shutdowns==unsigned(expected));
  if(!expected) assert(!rebootDelay.reboot);
  if(mode=="manual") assert(gitUpdate.ownsExternal(source) && gitUpdate.transferContinuing(source));
  if(mode=="expired" || mode=="counter") assert(!gitUpdate.busy());
  if(mode=="handoff") {
    assert(!gitUpdate.releaseExternal(source) && !gitUpdate.retireTransfer(source));
    assert(!gitUpdate.claimTransferCleanup(UINT64_MAX).token.generation && gitUpdate.busy());
  }
}
