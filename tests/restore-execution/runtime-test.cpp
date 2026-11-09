#include "RestoreExecution.h"
#include "ModelLoadTrust.h"
#include "LoopQueue.h"
#include "ManualUpdateLifecycle.h"
#include "ModelIdleForAdministration.h"
#include "RadioUpdateActivity.h"
#include "esp_timer.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <assert.h>
#include <functional>
#include <string>
#include <cstdio>
#define SOMFY_H
#define configfile_h
#define configsettings_h
struct ConfigSettings{struct {bool trusted=true;bool loadTrusted()const{return trusted;}}Security;}settings;
uint64_t securityConfigurationRevision(){return 1;}
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
static unsigned randomCalls=0;static bool randomFailed=false;
uint32_t restoreTestRandom(){++randomCalls;return randomFailed?0:0x12340000+randomCalls;}
static bool pendingRestart=false;bool restartPending(){return pendingRestart;}
void noteRestartOperationAccepted(){}
struct {bool acceptsMutation(const loop_item_t &)const{return true;}}protoSocket;
LoopQueue loopQueue;static QueueHandle_t capturedQueue=nullptr;
static void *captureCreate(unsigned count,unsigned bytes){capturedQueue=xQueueCreate(count,bytes);return capturedQueue;}
#define xQueueCreate captureCreate
#include "production-queue.inc"
#undef xQueueCreate
static void assertQueueOpen(){
 const auto count=loopQueue.pending();loop_item_t read;read.type=loop_item_type_t::proto_read;assert(loopQueue.submit(read));
 auto &items=static_cast<FakeRtosQueue *>(capturedQueue)->items;assert(items.back().type==loop_item_type_t::proto_read);items.pop_back();assert(loopQueue.pending()==count);
}
struct SomfyShadeController {
 ModelLoadTrust modelLoad{ModelLoadStatus::loaded};
 struct Radio {bool paused=false,busy=false,unavailable=false,stale=false,refuse=false;unsigned pauses=0,resumes=0;uint64_t generation=0;
  bool pauseForRestore(RadioInstallToken &out){++pauses;if(refuse||paused||out.generation)return false;paused=true;out.generation=++generation;return true;}
  bool installPauseHeld(const RadioInstallToken &token)const{return paused&&token.generation==generation;}
  RadioInstallResume resumeAfterInstall(const RadioInstallToken &token){++resumes;if(busy)return RadioInstallResume::busy;if(stale||!installPauseHeld(token))return RadioInstallResume::stale;paused=false;return unavailable?RadioInstallResume::unavailable:RadioInstallResume::resumed;}
 }transceiver;
 struct Shade{uint8_t id=255;bool idle=true;uint8_t getShadeId(){return id;}bool idleForAdministration()const{return idle;}}shades[32];
 struct Group{uint8_t id=255;int8_t direction=0;somfy_frame_t lastFrame;uint8_t linkedShades[32]{};uint8_t getGroupId(){return id;}}groups[16];
 Shade *getShadeById(uint8_t id){for(auto &shade:shades)if(id&&id!=255&&shade.id==id)return &shade;return nullptr;}
}somfy;
struct restore_options_t{bool settings=false,shades=false,network=false,transceiver=false,repeaters=false,mqtt=false;};
static RestoreApplyResult applyResult;static unsigned applies=0;static std::function<void()> duringApply;
struct ShadeConfigFile {static RestoreApplyResult restore(SomfyShadeController *,const char *,restore_options_t &,const GitUpdateOwnerToken &owner,const RadioInstallToken &radio){
 ++applies;assert(gitUpdate.transferRestoreClaimed(owner,uint64_t(stubEspTimerMicros)/1000));assert(somfy.transceiver.installPauseHeld(radio));
 loop_item_t stop;stop.type=loop_item_type_t::shade_request;assert(!loopQueue.submit(stop));if(duringApply)duringApply();return applyResult;
}};
static unsigned notifications=0;
void completeRestoreModel(const RestoreApplyResult &result){assert(result.modelApplied);++notifications;}
static unsigned restartCalls=0;static GitUpdateOwnerToken reservation;
RestartResult requestOwnedRestart(const GitUpdateOwnerToken &source,uint32_t){++restartCalls;assert(!somfy.transceiver.paused);assertQueueOpen();assert(gitUpdate.acquireRestart(reservation,uint64_t(stubEspTimerMicros)/1000,source));return {true,RestartReason::none};}
#include "../../src/GitUpdate.cpp"
#include "../../src/RestoreExecution.cpp"
static void clock(uint64_t now){stubEspTimerMicros=int64_t(now*1000);}
static GitUpdateOwnerToken queued(){
 GitUpdateOwnerToken owner;assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::restore,owner));assert(gitUpdate.beginTransfer(owner,ManualUpdateKind::restore,uint64_t(stubEspTimerMicros)/1000));
 assert(gitUpdate.claimTransfer(owner,uint64_t(stubEspTimerMicros)/1000));assert(gitUpdate.transferResource(owner,true));assert(gitUpdate.queueTransfer(owner));assert(noteRestoreQueued(owner,securityConfigurationRevision()));
 assert(gitUpdate.finishTransfer(owner,uint64_t(stubEspTimerMicros)/1000,true));LittleFS.files["/shades.tmp"]="owned backup";return owner;
}
int main(int argc,char **argv){assert(argc==2);const std::string mode=argv[1];randomFailed=mode=="random";clock(100);loopQueue.begin();
 if(mode=="random"){assert(!beginRestoreExecution()&&!restoreExecutionReady()&&randomCalls==8);assert(!beginRestoreExecution()&&randomCalls==8);return 0;}
 assert(beginRestoreExecution()&&restoreExecutionReady());const auto owner=queued();
 applyResult.complete=true;applyResult.refusal=RestoreRefusal::none;applyResult.modelApplied=true;applyResult.modelFile={ModelCommitState::saved};
 LittleFS.beforeRemove=[&]{assert(!somfy.transceiver.paused&&gitUpdate.ownsExternal(owner));assertQueueOpen();};
 if(mode=="busy"||mode=="stale"||mode=="unavailable")duringApply=[&]{somfy.transceiver.busy=true;};
 if(mode=="pending"){loop_item_t stop;stop.type=loop_item_type_t::shade_request;assert(loopQueue.submit(stop));}
 if(mode=="radio")somfy.transceiver.refuse=true;
 if(mode=="moving"){somfy.shades[0].id=1;somfy.shades[0].idle=false;}
 if(mode=="remove")LittleFS.failRemove=true;
 if(mode=="missingModel")applyResult.modelApplied=false;
 if(mode=="contradict"){applyResult.settings.state=RestoreSettingsState::saved;applyResult.settings.requestedNamespaces=applyResult.settings.verifiedNamespaces=applyResult.settings.appliedNamespaces=1;applyResult.settings.persistence.state=settings_save_state_t::failed;}
 if(mode=="extraNamespace"){applyResult.settings.state=RestoreSettingsState::saved;applyResult.settings.requestedNamespaces=applyResult.settings.verifiedNamespaces=applyResult.settings.appliedNamespaces=settingsNamespaceBit(SettingsNamespace::cfg)|settingsNamespaceBit(SettingsNamespace::ntp)|settingsNamespaceBit(SettingsNamespace::ip)|settingsNamespaceBit(SettingsNamespace::ethernet);}
 if(mode=="partial"){applyResult.complete=false;applyResult.refusal=RestoreRefusal::counterStorage;applyResult.counterWriteAttempted=true;applyResult.modelApplied=false;}
 if(mode=="expired")duringApply=[] {clock(40000);};
 runOwnedRestore(owner,RESTORE_SHADES|RESTORE_REPEATERS|((mode=="missingSettings"||mode=="extraNamespace")?RESTORE_SETTINGS:0)|(mode=="extraNamespace"?RESTORE_NETWORK:0));
 assert(notifications==unsigned(applies&&applyResult.modelApplied));
 if(mode=="remove"){
  assert(restoreExecutionStatus().phase==RestoreExecutionPhase::cleanupFault&&!restartCalls&&gitUpdate.ownsExternal(owner)&&!somfy.transceiver.paused);
  assert(!gitUpdate.claimTransferCleanup(40000).token.generation);const auto calls=LittleFS.removes;serviceRestoreCleanup();assert(LittleFS.removes==calls);return 0;
 }
 if(mode=="busy"||mode=="stale"||mode=="unavailable"){
  auto status=restoreExecutionStatus();assert(status.phase==RestoreExecutionPhase::cleanupPending&&status.restartBlocked&&somfy.transceiver.paused);
  loop_item_t stop;stop.type=loop_item_type_t::shade_request;assert(!loopQueue.submit(stop));clock(40000);
  assert(!gitUpdate.claimTransferCleanup(40000).token.generation);assert(!gitUpdate.releaseExternal(owner)&&!gitUpdate.closeTransfer(owner));
  GitUpdateOwnerToken foreign;assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,foreign));
  somfy.transceiver.busy=false;if(mode=="unavailable")somfy.transceiver.unavailable=true;
  if(mode=="stale"){++ownerGeneration;::owner=Owner::manual;serviceRestoreCleanup();assert(somfy.transceiver.paused&&LittleFS.removes==0&&restoreExecutionStatus().phase==RestoreExecutionPhase::cleanupFault);return 0;}
  serviceRestoreCleanup();assert(!somfy.transceiver.paused&&LittleFS.removes==1&&!gitUpdate.busy()&&!restartCalls);
  const auto resumes=somfy.transceiver.resumes;serviceRestoreCleanup();assert(somfy.transceiver.resumes==resumes&&LittleFS.removes==1);
  const auto next=queued();const auto files=LittleFS.removes;refuseUnpublishedRestore(owner,RestoreRefusal::ownership);assert(restoreExecutionStatus().generation==next.generation&&restoreExecutionStatus().phase==RestoreExecutionPhase::queued);runOwnedRestore(owner,RESTORE_SHADES);assert(gitUpdate.ownsExternal(next)&&LittleFS.removes==files);return 0;
 }
 if(mode=="pending"||mode=="radio"||mode=="moving")assert(!applies&&!restartCalls&&!gitUpdate.busy()&&LittleFS.removes==1);
 else if(mode=="partial"||mode=="expired"||mode=="contradict"||mode=="missingModel"||mode=="missingSettings"||mode=="extraNamespace")assert(applies==1&&!restartCalls&&!gitUpdate.busy()&&LittleFS.removes==1);
 else assert(mode=="complete"&&applies==1&&restartCalls==1&&LittleFS.removes==1&&reservation.generation);
 const auto status=restoreExecutionStatus();assert(status.generation==owner.generation&&status.bootSession);puts("Actual restore execution source/lease/cleanup case passed");
}
