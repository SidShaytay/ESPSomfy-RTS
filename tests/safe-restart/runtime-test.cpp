#include "ModelLoadTrust.h"
#include "Arduino.h"
#include "Utils.h"
#include "GitUpdate.h"
#include "ManualUpdateLifecycle.h"
#include "SafeRestart.h"
#include "esp_timer.h"
#include "LoopQueue.h"
#include "SettingsDurability.h"
#include "ModelCommitResult.h"
#include "SomfyFrame.h"
#include "SettingsLoadState.h"
#include <assert.h>
#include <string>
#include <thread>
#include <atomic>
#include <future>
#include <chrono>
#define configsettings_h
struct ConfigSettings { bool namespaces=true;struct { bool trusted=true;bool loadTrusted()const{return trusted;} } Security;bool settingsUsable()const{return namespaces && Security.trusted;} } settings;
#define SOMFY_H
namespace SafeRestartDetail { extern thread_local unsigned criticalDepth; }
static unsigned commits=0;
static void (*duringCommit)()=nullptr;
static ModelCommitResult fileResult{ModelCommitState::saved};
static uint32_t queued=0;
static bool queueSealed=false,queueSealRefused=false;
LoopQueue loopQueue;
uint32_t LoopQueue::pending() const {assert(!SafeRestartDetail::criticalDepth);return queued;}
bool LoopQueue::sealForRestart() {if(queued || queueSealRefused)return false;queueSealed=true;return true;}
void LoopQueue::reopenAfterRefusedRestart() {queueSealed=false;}
struct SomfyShadeController {
  ModelLoadTrust modelLoad{ModelLoadStatus::loaded};
  uint64_t configurationRevision=1;
  bool isDirty=false;
  struct {
    struct {SettingsLoadStatus loadStatus{SettingsLoadState::loaded};}config;
    bool uncertain=false,work=false,sealed=false,sealRefused=false;
    bool counterEditUncertain() const{return uncertain;}
    bool restartQuiescent() const{return !work;}
    bool sealForRestart(){if(work || sealRefused)return false;sealed=true;return true;}
    void reopenAfterRefusedRestart(){sealed=false;}
  } transceiver;
  struct Shade {uint8_t id=255;bool idle=true,pending=false;uint32_t gpioRelease=0;uint8_t getShadeId(){return id;}bool isIdle(){return idle;}bool idleForAdministration(){return idle&&!pending&&!gpioRelease;}} shades[32];
  struct Group {uint8_t id=255;int8_t direction=0;somfy_frame_t lastFrame;uint8_t linkedShades[32]{};uint8_t getGroupId(){return id;}} groups[16];
  Shade *getShadeById(uint8_t id){for(auto &shade:shades)if(id && id!=255 && shade.id==id)return &shade;return nullptr;}
  ModelCommitResult commit(){assert(!SafeRestartDetail::criticalDepth);++commits;if(duringCommit)duringCommit();isDirty=!fileResult.saved();noteModelCommit(fileResult);return fileResult;}
} somfy;

static unsigned shutdowns=0;
void performLegacyShutdown() {
  ++shutdowns;assert(gitUpdate.busy());assert(commits && queueSealed && somfy.transceiver.sealed);
  GitUpdateOwnerToken other;assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,other));
}
static unsigned clockReads=0;
static int64_t readClock(){assert(!SafeRestartDetail::criticalDepth);++clockReads;return stubEspTimerMicros;}
#define esp_timer_get_time readClock
#include "../../src/SafeRestart.cpp"
#undef esp_timer_get_time
static void tick(uint64_t now) {stubEspTimerMicros=int64_t(now*1000);}
static GitUpdateOwnerToken completedUpload(bool keepClaim=false) {
  GitUpdateOwnerToken source;assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,source));
  assert(gitUpdate.beginTransfer(source,ManualUpdateKind::firmware,100));
  assert(gitUpdate.claimTransfer(source,100));assert(gitUpdate.transferResource(source,true));
  assert(gitUpdate.transferResource(source,false));assert(gitUpdate.holdTransferRestart(source));
  if(!keepClaim)assert(gitUpdate.finishTransfer(source,100,true));return source;
}
int main(int argc,char **argv) {
  if(argc>1 && std::string(argv[1])=="badLoad") {
    somfy.modelLoad.fail();somfy.isDirty=true;tick(100);restartLoop();
    assert(!requestRestart(500).admitted && commits==0);
    assert(restartTransferBlocked() && !restartRestoreStagingBlocked());
    assert(restartStatus().reason==RestartReason::modelUnavailable && !shutdowns);
    return 0;
  }

  assert(argc==2);const std::string mode=argv[1];tick(100);
  assert(submitRestart()==RestartSubmission::refused);assert(!restartPending());assert(!clockReads);
  assert(!restartLoop());
  if(mode=="invalidCache") {
    somfy.configurationRevision=0;assert(!restartLoop());
    const auto reads=clockReads;assert(restartTransferBlocked());assert(clockReads==reads);
    assert(submitRestart()==RestartSubmission::refused && !commits);return 0;
  }
  if(mode=="readWaiting" || mode=="readFlood") {
    assert(requestRestart(500).admitted);queued=1;tick(200);
    assert(!restartLoop() && restartPending() && !commits);
    const auto reads=clockReads;assert(requestRestart().reason==RestartReason::pending);assert(clockReads==reads);
    tick(600);assert(!restartLoop() && restartPending() && !commits);
    if(mode=="readWaiting") {queued=0;assert(restartLoop() && shutdowns==1);return 0;}
    tick(5599);assert(!restartLoop() && restartPending());tick(5600);
    assert(!restartLoop() && !restartPending() && !gitUpdate.busy() && !commits);
    queued=0;
  } else if(mode=="asyncStale") {
    const auto reads=clockReads;assert(submitRestart()==RestartSubmission::queued);assert(clockReads==reads);++somfy.configurationRevision;
    tick(101);assert(!restartLoop() && !commits && !restartPending());
  } else if(mode=="asyncAge") {
    assert(submitRestart()==RestartSubmission::queued);tick(60101);
    assert(!restartLoop() && !commits && !restartPending());
  } else if(mode=="ownedCancel" || mode=="ownedExpired" || mode=="ownedHandoff" || mode=="claimed") {
    auto source=completedUpload(mode=="claimed");
    if(mode=="ownedCancel") {
      assert(submitOwnedRestart(source)==RestartSubmission::queued);cancelRestart();
      assert(!restartLoop());assert(!gitUpdate.busy());
    } else if(mode=="ownedExpired") {
      assert(submitOwnedRestart(source)==RestartSubmission::queued);tick(30100);
      assert(!restartLoop() && !commits && !restartPending());
      assert(gitUpdate.ownsExternal(source)); // existing transfer cleanup, never generic fallback
    } else if(mode=="claimed") {
      assert(!requestOwnedRestart(source).admitted);assert(!commits && !restartPending());
      assert(gitUpdate.ownsExternal(source));
    } else {
      assert(requestOwnedRestart(source).admitted);assert(!gitUpdate.ownsExternal(source));
      tick(600);assert(restartLoop() && shutdowns==1);return 0;
    }
  } else {
    const uint64_t base=mode=="clockWrap" ? uint64_t(UINT32_MAX)-100 : 100;
    tick(base);
    if(mode=="dirty" || mode=="initialFailure" || mode=="cancelIO")somfy.isDirty=true;
    if(mode=="initialFailure")fileResult={ModelCommitState::failed,ModelCommitStage::write};
    if(mode=="cancelIO")duringCommit=[]{cancelRestart();};
    if(mode=="counter")somfy.transceiver.uncertain=true;
    if(mode=="settings")settingsDurability().markUncertain(1);
    if(mode=="settingsLoad")settings.namespaces=false;
    if(mode=="radioLoad")somfy.transceiver.config.loadStatus={SettingsLoadState::failed};
    if(mode=="security"){
      settings.Security.trusted=false;assert(!restartLoop());
      const auto reads=clockReads;assert(restartTransferBlocked());assert(clockReads==reads);
    }
    if(mode=="moving"){somfy.shades[31].id=7;somfy.shades[31].idle=false;}
    if(mode=="held"){somfy.shades[31].id=7;somfy.shades[31].pending=true;}
    if(mode=="pulse"){somfy.shades[31].id=7;somfy.shades[31].gpioRelease=1;}
    if(mode=="groupUnknown"){somfy.groups[15].id=4;somfy.groups[15].lastFrame.valid=true;somfy.groups[15].lastFrame.cmd=somfy_commands::My;}
    if(mode=="queue")queued=1;
    if(mode=="radio")somfy.transceiver.work=true;
    const auto result=requestRestart(500);
    const bool initial=mode!="settingsLoad" && mode!="radioLoad" && mode!="security" && mode!="initialFailure" && mode!="cancelIO" && mode!="counter" && mode!="settings" && mode!="moving" && mode!="held" && mode!="pulse" && mode!="groupUnknown" && mode!="queue" && mode!="radio";
    if(mode=="settingsLoad" || mode=="radioLoad")assert(!result.admitted && result.reason==RestartReason::settingsUnavailable && !commits && restartTransferBlocked());
    if(mode=="security")assert(!result.admitted && result.reason==RestartReason::securityUnavailable && !commits);
    assert(result.admitted==initial);
    if(initial) {
      if(mode=="finalFailure")fileResult={ModelCommitState::uncertain,ModelCommitStage::replace};
      if(mode=="lateSettingsLoad")duringCommit=[]{settings.namespaces=false;};
      if(mode=="lateSecurity")duringCommit=[]{settings.Security.trusted=false;};
      if(mode=="beforeFinalSecurity")settings.Security.trusted=false;
      if(mode=="lateSettings")duringCommit=[]{settingsDurability().markUncertain(1);settingsDurability().fullSave(SettingsNamespace::cfg,{settings_save_state_t::saved});};
      if(mode=="lateHistory")duringCommit=[]{++somfy.configurationRevision;};
      if(mode=="cancelFinal")duringCommit=[]{cancelRestart();};
      if(mode=="queueSealRace")queueSealRefused=true;
      if(mode=="radioSealRace")somfy.transceiver.sealRefused=true;
      if(mode=="operation")noteRestartOperationAccepted();
      if(mode=="failureRetry") {noteModelCommit({ModelCommitState::failed});noteModelCommit({ModelCommitState::saved});}
      if(mode=="revision")++somfy.configurationRevision;
      if(mode=="rollback")tick(99);else tick(base+500);
      const bool expected=mode=="allowed" || mode=="dirty" || mode=="clockWrap";
      assert(restartLoop()==expected && shutdowns==unsigned(expected));
      if(expected)return 0;
    }
    assert(!restartPending() && !gitUpdate.busy());assert(!queueSealed && !somfy.transceiver.sealed);
  }
  const auto attempts=commits;duringCommit=nullptr;fileResult={ModelCommitState::saved};
  tick(100000);assert(!restartLoop() && commits==attempts && !shutdowns);
  if(mode=="cancelIO" || mode=="finalFailure" || mode=="failureRetry") {
    // Recovery alone did not revive the old intent; only this explicit new one can admit.
    assert(requestRestart().admitted);cancelRestart();assert(!restartLoop() && !gitUpdate.busy());
  }
}
