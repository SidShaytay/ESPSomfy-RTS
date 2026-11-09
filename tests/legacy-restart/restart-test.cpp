#include "Arduino.h"
#include "Utils.h"
#include "GitUpdate.h"
#include "ManualUpdateLifecycle.h"
#include "LegacyRestartSource.h"
#include "esp_timer.h"
#include <assert.h>
#include <string>
#include <thread>
#include <atomic>
#include <future>
#include <chrono>
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
  if(mode=="record") {
    std::atomic<bool> done{false};std::atomic<unsigned> samples{0};
    std::thread writer([&]{for(uint64_t n=1;n<=200000;++n) {
      const auto before=snapshotLegacyRestart(rebootDelay);
      clearLegacyRestart(rebootDelay,before);
      assert(publishExternalLegacyRestart(rebootDelay,uint32_t(n),n<<32|n,uint8_t(n%3)).admitted);
    }done=true;});
    while(!done) {
      const auto value=snapshotLegacyRestart(rebootDelay);
      if(value.reboot) {
        const auto n=value.rebootTime;
        assert(value.externalOwnerGeneration==(uint64_t(n)<<32|uint32_t(n)));
        assert(value.externalOwnerKind==n%3);++samples;
      }
    }
    writer.join();assert(samples);
    // Publication, snapshot and clear share one boundary. Hold it while a
    // competing task enters each API; none may return a partial tuple.
    for(unsigned operation=0;operation<3;++operation) {
      std::promise<void> entering,finished;auto ready=entering.get_future(),done=finished.get_future();
      std::thread task;
      {
        LegacyRestartDetail::Guard held;
        task=std::thread([&]{entering.set_value();
          if(operation==0) publishGenericLegacyRestart(rebootDelay,1);
          else if(operation==1) (void)snapshotLegacyRestart(rebootDelay);
          else {rebootDelay_t stale;clearLegacyRestart(rebootDelay,stale);}
          finished.set_value();});
        ready.wait();assert(done.wait_for(std::chrono::milliseconds(20))==std::future_status::timeout);
      }
      task.join();
    }

    const auto old=snapshotLegacyRestart(rebootDelay);assert(clearLegacyRestart(rebootDelay,old));
    assert(publishExternalLegacyRestart(rebootDelay,1,99,1).admitted);
    assert(!clearLegacyRestart(rebootDelay,old));
    assert(snapshotLegacyRestart(rebootDelay).externalOwnerGeneration==99);return 0;
  }
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
