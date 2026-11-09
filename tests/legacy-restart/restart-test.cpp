#include "Arduino.h"
#include "Utils.h"
#include "GitUpdate.h"
#include "ManualUpdateLifecycle.h"
#include "LegacyRestartSource.h"
#include "esp_timer.h"
#include "LoopQueue.h"
#include "SettingsDurability.h"
#include "ModelCommitResult.h"
#include <assert.h>
#include <string>
#include <thread>
#include <atomic>
#include <future>
#include <chrono>
#define SOMFY_H
static unsigned commits=0;
static void (*duringCommit)()=nullptr;
static ModelCommitResult fileResult{ModelCommitState::saved};
static uint32_t queued=0;
static bool queueSealed=false,queueSealRefused=false;
LoopQueue loopQueue;
uint32_t LoopQueue::pending() const {return queued;}
bool LoopQueue::sealForRestart() {if(queued || queueSealRefused)return false;queueSealed=true;return true;}
void LoopQueue::reopenAfterRefusedRestart() {queueSealed=false;}
struct SomfyShadeController {
  uint64_t configurationRevision=1;
  struct {
    bool uncertain=false,work=false,sealed=false,sealRefused=false;
    bool counterEditUncertain() const{return uncertain;}
    bool restartQuiescent() const{return !work;}
    bool sealForRestart(){if(work || sealRefused)return false;sealed=true;return true;}
    void reopenAfterRefusedRestart(){sealed=false;}
  } transceiver;
  struct Shade {uint8_t id=255;bool idle=true,pending=false;uint32_t gpioRelease=0;uint8_t getShadeId(){return id;}bool isIdle(){return idle;}bool idleForAdministration(){return idle&&!pending&&!gpioRelease;}} shades[32];
  struct Group {uint8_t id=255;int8_t direction=0;uint8_t getGroupId(){return id;}} groups[16];
  ModelCommitResult commit(){++commits;if(duringCommit)duringCommit();return fileResult;}
} somfy;
rebootDelay_t rebootDelay;
static unsigned shutdowns=0;
static unsigned long at=100;
static unsigned long clockNow(){return at;}
void performLegacyShutdown() {
  ++shutdowns;assert(gitUpdate.busy());
  assert(commits==1 && queueSealed && somfy.transceiver.sealed);
  GitUpdateOwnerToken competing;
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
  if(mode=="modelFailed")fileResult={ModelCommitState::failed,ModelCommitStage::write};
  if(mode=="modelUncertain")fileResult={ModelCommitState::uncertain,ModelCommitStage::verifyRead};
  if(mode=="settings")settingsDurability().markUncertain(1);
  if(mode=="queue")queued=1;
  if(mode=="moving"){somfy.shades[31].id=7;somfy.shades[31].idle=false;}
  if(mode=="held"){somfy.shades[0].id=3;somfy.shades[0].pending=true;}
  if(mode=="pulse"){somfy.shades[0].id=3;somfy.shades[0].gpioRelease=1;}
  if(mode=="groupMoving"){somfy.groups[15].id=8;somfy.groups[15].direction=-1;}
  if(mode=="radio")somfy.transceiver.work=true;
  if(mode=="queueSealRace")queueSealRefused=true;
  if(mode=="radioSealRace")somfy.transceiver.sealRefused=true;
  if(mode=="lateSettings")duringCommit=[]{settingsDurability().markUncertain(1);settingsDurability().fullSave(SettingsNamespace::cfg,{settings_save_state_t::saved});};
  if(mode=="lateHistory")duringCommit=[]{++somfy.configurationRevision;};
  const bool restarted=legacyRestartLoop();
  const bool expected=mode=="allowed" || mode=="handoff";
  assert(restarted==expected && shutdowns==unsigned(expected));
  if(!expected) {
    assert(!snapshotLegacyRestart(rebootDelay).reboot);
    assert(!queueSealed && !somfy.transceiver.sealed);
    const auto attempts=commits;fileResult={ModelCommitState::saved};duringCommit=nullptr;queued=0;
    assert(!legacyRestartLoop() && commits==attempts); // no silent replay
  }
  if(mode=="manual") assert(gitUpdate.ownsExternal(source) && gitUpdate.transferContinuing(source));
  if(mode=="expired" || mode=="counter") assert(!gitUpdate.busy());
  if(mode=="handoff") {
    assert(!gitUpdate.releaseExternal(source) && !gitUpdate.retireTransfer(source));
    assert(!gitUpdate.claimTransferCleanup(UINT64_MAX).token.generation && gitUpdate.busy());
  }
}
