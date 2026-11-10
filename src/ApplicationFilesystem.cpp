#include "ApplicationFilesystem.h"
#if defined(ESPSOMFY_BUNDLED_WEB)
bool completePreparedApplication(const GitUpdateOwnerToken &,const ManualApplicationSource &){return false;}
bool noteApplicationPreparation(const GitUpdateOwnerToken &,const ManualApplicationSource &){return false;}
bool requestApplicationFilesystemRecovery(const GitUpdateOwnerToken &,const ManualApplicationSource &){return false;}
void runApplicationPreparation(const GitUpdateOwnerToken &){}
void serviceApplicationFilesystem(){}
ApplicationFilesystemSnapshot applicationFilesystemStatus(){return {};}
bool applicationFilesystemReady(const GitUpdateOwnerToken &,const ManualApplicationSource &,int &){return false;}
#else
#include "ConfigSettings.h"
#include "Somfy.h"
#include "LoopQueue.h"
#include "ModelCommitService.h"
#include "ModelIdleForAdministration.h"
#include "MountedFilesystemGate.h"
#include "SafeRestart.h"
#include "SettingsRevisions.h"
#include <LittleFS.h>
#include <Update.h>
#include <esp_littlefs.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include <cstring>
#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#else
#include <mutex>
#endif
extern SomfyShadeController somfy;
extern ConfigSettings settings;
extern LoopQueue loopQueue;
namespace ApplicationFilesystemDetail {
#if defined(ESP_PLATFORM)
portMUX_TYPE mutex=portMUX_INITIALIZER_UNLOCKED;
struct Guard{Guard(){portENTER_CRITICAL(&mutex);}~Guard(){portEXIT_CRITICAL(&mutex);}};
#else
std::mutex mutex;
struct Guard{std::lock_guard<std::mutex> lock;Guard():lock(mutex){}};
#endif
ApplicationFilesystemSnapshot view;
bool recoveryRetry=false;
struct Pending {
  GitUpdateOwnerToken owner;
  ManualApplicationSource source;
  RadioInstallToken radio;
  bool claimed=false,queueSealed=false,filesystemSealed=false,recovering=false,faulted=false,restartWanted=false;
} pending;
static_assert(sizeof(Pending)<=64,"Bounded loop resource context, no image/request storage");
uint64_t now(){const auto value=esp_timer_get_time();return value<0?UINT64_MAX:uint64_t(value)/1000;}
bool same(const GitUpdateOwnerToken &a,const GitUpdateOwnerToken &b){return a.generation&&a.generation==b.generation&&a.kind==b.kind;}
void publish(ApplicationFilesystemPhase phase,ApplicationFilesystemFault fault=ApplicationFilesystemFault::none){
  Guard guard;if(!same(view.owner,pending.owner))return;view.phase=phase;if(fault!=ApplicationFilesystemFault::none)view.fault=fault;
}
bool secure(){return pending.source.securityRevision==securityConfigurationRevision()&&settings.Security.loadTrusted();}
bool layout(uint32_t bytes,int &command){
  const auto *mounted=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"spiffs");
  if(!mounted||!esp_littlefs_mounted("spiffs")||mounted->size!=bytes)return false;
  if(mounted->subtype==ESP_PARTITION_SUBTYPE_DATA_SPIFFS)command=U_SPIFFS;
  else if(mounted->subtype==ESP_PARTITION_SUBTYPE_DATA_LITTLEFS)command=U_LITTLEFS;
  else return false;
  // The pinned Update SDK ignores a label for these commands. Prove that its
  // first subtype partition is the exact filesystem mounted by LittleFS.
  const auto *selected=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,mounted->subtype,nullptr);
  return selected&&selected->address==mounted->address&&selected->size==mounted->size&&
    selected->type==mounted->type&&selected->subtype==mounted->subtype&&!strcmp(selected->label,mounted->label);
}
void attention(ApplicationFilesystemFault fault){pending.restartWanted=false;pending.faulted=true;publish(ApplicationFilesystemPhase::attention,fault);}
void recover(){
  if(!pending.claimed||pending.faulted)return;
  pending.recovering=true;publish(ApplicationFilesystemPhase::recovering);
  if(pending.queueSealed){loopQueue.reopenAfterRefusedRestart();pending.queueSealed=false;}
  bool mounted=true;
  if(pending.filesystemSealed&&!esp_littlefs_mounted("spiffs"))
    mounted=LittleFS.begin(false)&&esp_littlefs_mounted("spiffs");
  if(pending.radio.generation){
    const auto result=somfy.transceiver.resumeAfterInstall(pending.radio);
    if(result==RadioInstallResume::busy)return;
    if(result==RadioInstallResume::stale){attention(ApplicationFilesystemFault::radio);return;}
    pending.radio={}; // unavailable is a valid restoration of a disabled radio
  }
  if(!mounted){attention(ApplicationFilesystemFault::mount);return;}
  if(pending.filesystemSealed){
    // Mounted proof precedes writer admission. Recover CURRENT RAM, never load
    // the uploaded model/counters and never format a failed mount.
    mountedFilesystemGate().resume();somfy.isDirty=true;
    if(!applyModelCommit().file.saved()){attention(ApplicationFilesystemFault::save);return;}
    if(!gitUpdate.transferFilesystem(pending.owner,false)){attention(ApplicationFilesystemFault::source);return;}
    pending.filesystemSealed=false;
  }
  const bool uploaded=applicationFilesystemStatus().uploaded;
  if(pending.restartWanted&&secure()&&gitUpdate.transferContinuing(pending.owner,now())&&gitUpdate.holdTransferRestart(pending.owner)){
    const auto owner=pending.owner;
    if(!gitUpdate.finishTransfer(owner,now(),false)){attention(ApplicationFilesystemFault::source);return;}
    const auto restart=requestOwnedRestart(owner,500);
    {Guard guard;view.phase=ApplicationFilesystemPhase::completed;view.restartScheduled=restart.admitted;view.restartReason=restart.reason;}
    if(!restart.admitted)gitUpdate.releaseRestartSource(owner,now());
    pending={};return;
  }
  publish(uploaded?ApplicationFilesystemPhase::completed:ApplicationFilesystemPhase::refused);
  if(uploaded){Guard guard;view.restartReason=RestartReason::stale;}
  if(!gitUpdate.closeTransfer(pending.owner)||!gitUpdate.releaseExternal(pending.owner)){attention(ApplicationFilesystemFault::source);return;}
  pending={};
}
void refuse(ApplicationFilesystemFault fault){publish(ApplicationFilesystemPhase::recovering,fault);recover();}
}
ApplicationFilesystemSnapshot applicationFilesystemStatus(){ApplicationFilesystemDetail::Guard guard;return ApplicationFilesystemDetail::view;}
bool noteApplicationPreparation(const GitUpdateOwnerToken &owner,const ManualApplicationSource &source){
  if(owner.kind!=GitUpdateExternalOwner::manual||!source.valid()||!gitUpdate.ownsExternal(owner)||
     source.securityRevision!=securityConfigurationRevision())return false;
  ApplicationFilesystemDetail::Guard guard;auto &view=ApplicationFilesystemDetail::view;
  if(owner.generation<=view.owner.generation)return false;
  ApplicationFilesystemDetail::recoveryRetry=false;view={};view.owner=owner;view.source=source;view.phase=ApplicationFilesystemPhase::queued;return true;
}
void runApplicationPreparation(const GitUpdateOwnerToken &owner){
  using namespace ApplicationFilesystemDetail;
  const auto state=applicationFilesystemStatus();
  if(pending.owner.generation||!same(state.owner,owner)||state.phase!=ApplicationFilesystemPhase::queued)return;
  if(!gitUpdate.claimApplicationPreparation(owner,state.source,now())){
    Guard guard;if(same(view.owner,owner)){view.phase=ApplicationFilesystemPhase::refused;view.fault=ApplicationFilesystemFault::source;}return;
  }
  pending.owner=owner;pending.source=state.source;pending.claimed=true;
  if(!secure()){refuse(ApplicationFilesystemFault::source);return;}
  if(restartTransferBlocked()||restartPending()||!somfy.modelLoad.usable()||!modelIdleForAdministration(somfy)){
    refuse(ApplicationFilesystemFault::busy);return;
  }
  int command=0;if(!layout(state.source.bytes,command)){refuse(ApplicationFilesystemFault::layout);return;}
  if(!applyModelCommit().file.saved()){refuse(ApplicationFilesystemFault::save);return;}
  if(!loopQueue.sealForRestart()){refuse(ApplicationFilesystemFault::busy);return;}
  pending.queueSealed=true;
  if(!somfy.transceiver.pauseForRestore(pending.radio)){refuse(ApplicationFilesystemFault::radio);return;}
  if(!secure()||!gitUpdate.transferContinuing(owner,now())){refuse(ApplicationFilesystemFault::source);return;}
  if(!mountedFilesystemGate().seal()){refuse(ApplicationFilesystemFault::busy);return;}
  pending.filesystemSealed=true;
  if(!gitUpdate.transferFilesystem(owner,true)){attention(ApplicationFilesystemFault::source);return;}
  {Guard guard;view.command=command;view.phase=ApplicationFilesystemPhase::draining;}
  loopQueue.reopenAfterRefusedRestart();pending.queueSealed=false;
}
void serviceApplicationFilesystem(){
  using namespace ApplicationFilesystemDetail;
  if(!pending.owner.generation)return;
  if(pending.faulted){
    Guard guard;if(!recoveryRetry)return;recoveryRetry=false;pending.faulted=false;
  }
  if(!gitUpdate.ownsExternal(pending.owner)){attention(ApplicationFilesystemFault::source);return;}
  if(pending.recovering){recover();return;}
  if(!pending.claimed){
    const bool uploaded=applicationFilesystemStatus().uploaded;
    ManualUpdateCleanup cleanup;
    if(uploaded){
      if(!gitUpdate.claimTransferRecovery(pending.owner,now()))return;
      pending.restartWanted=true;
    }else{
      cleanup=gitUpdate.claimTransferCleanup(pending.owner,now());
      if(!cleanup.token.generation)return;
    }
    pending.claimed=true;
    if(cleanup.resourceOpen){
      Update.abort();if(Update.isRunning()){gitUpdate.finishTransfer(pending.owner,now(),false);pending.claimed=false;return;}
      if(!gitUpdate.transferResource(pending.owner,false)){attention(ApplicationFilesystemFault::source);return;}
    }
    if(uploaded)recover();else refuse(ApplicationFilesystemFault::source);return;
  }
  if(!secure()||!gitUpdate.transferContinuing(pending.owner,now())){refuse(ApplicationFilesystemFault::source);return;}
  if(!mountedFilesystemGate().drained())return;
  LittleFS.end();
  if(esp_littlefs_mounted("spiffs")){refuse(ApplicationFilesystemFault::unmount);return;}
  if(!secure()||!gitUpdate.transferContinuing(pending.owner,now())||!gitUpdate.applicationPrepared(pending.owner)){
    refuse(ApplicationFilesystemFault::source);return;
  }
  publish(ApplicationFilesystemPhase::ready);
  if(!gitUpdate.finishTransfer(pending.owner,now(),true)){attention(ApplicationFilesystemFault::source);return;}
  pending.claimed=false;
}
bool completePreparedApplication(const GitUpdateOwnerToken &owner,const ManualApplicationSource &source){
  int command=0;
  if(!applicationFilesystemReady(owner,source,command)||!gitUpdate.transferContinuing(owner,ApplicationFilesystemDetail::now())||
     !gitUpdate.holdTransferRecovery(owner,ApplicationFilesystemDetail::now()))return false;
  ApplicationFilesystemDetail::Guard guard;
  if(!ApplicationFilesystemDetail::same(ApplicationFilesystemDetail::view.owner,owner))return false;
  ApplicationFilesystemDetail::view.uploaded=true;return true;
}
bool requestApplicationFilesystemRecovery(const GitUpdateOwnerToken &owner,const ManualApplicationSource &source){
  using namespace ApplicationFilesystemDetail;
  Guard guard;
  if(view.phase!=ApplicationFilesystemPhase::attention||!same(view.owner,owner)||!view.source.same(source))return false;
  recoveryRetry=true;return true;
}
bool applicationFilesystemReady(const GitUpdateOwnerToken &owner,const ManualApplicationSource &source,int &command){
  const auto state=applicationFilesystemStatus();
  if(state.phase!=ApplicationFilesystemPhase::ready||!ApplicationFilesystemDetail::same(state.owner,owner)||
     !state.source.same(source)||source.securityRevision!=securityConfigurationRevision()||!gitUpdate.ownsExternal(owner))return false;
  command=state.command;return true;
}
#endif
