#if defined(ESPSOMFY_BUNDLED_WEB)
#include "WebRoutes.h"
namespace {
void refuseSeparateApplication(AsyncWebServerRequest *request) {
  if(request->getAttribute("bundledApplicationRefused",false))return;
  request->setAttribute("bundledApplicationRefused",true);
  request->send(410,_encoding_json,"{\"status\":\"ERROR\",\"started\":false,\"desc\":\"Web UI is bundled with this board's firmware. Upload the matching firmware image; separate Application images are retired. Nothing was written or restarted.\"}");
}
}
void registerApplicationUploadRoute(AsyncWebServer &server) {
  server.on(AsyncURIMatcher::exact("/updateApplication"),HTTP_POST,
    [](AsyncWebServerRequest *request){refuseSeparateApplication(request);},
    [](AsyncWebServerRequest *request,String,size_t index,uint8_t *,size_t,bool){
      if(uploadAllowed(request,index))refuseSeparateApplication(request);
    });
}
#else
#include "ApplicationFilesystem.h"
#include "ManualUploadSession.h"
#include "PreparedApplicationUpload.h"
#include "ModelCommitService.h"
#include "SafeRestart.h"
#include "SettingsRevisions.h"
#include "TaskWdt.h"
#include "Web.h"
#include "WebRoutes.h"
#include <esp_random.h>
#include <esp_timer.h>
#include <cstdio>
extern Web webServer;
namespace {
uint64_t applicationRouteNow(){const auto value=esp_timer_get_time();return value<0?UINT64_MAX:uint64_t(value)/1000;}
bool applicationIdentity(const String &text,uint64_t &out){
  if(!text.length()||text.length()>20||text.c_str()[0]=='0')return false;
  uint64_t value=0;
  for(unsigned i=0;i<text.length();++i){const char c=text.c_str()[i];
    if(c<'0'||c>'9'||value>(UINT64_MAX-uint64_t(c-'0'))/10)return false;
    value=value*10+uint64_t(c-'0');
  }
  out=value;return value!=0;
}
void applicationError(AsyncWebServerRequest *request,int code,const char *description){
  char body[384];snprintf(body,sizeof(body),"{\"status\":\"ERROR\",\"desc\":\"%s\"}",description);
  request->send(code,_encoding_json,body);
}
bool applicationAuthenticated(AsyncWebServerRequest *request){
  if(webServer.isAuthenticated(request))return true;
  applicationError(request,401,"Authentication required.");return false;
}
const char *applicationPhase(ApplicationFilesystemPhase phase){
  switch(phase){
    case ApplicationFilesystemPhase::queued:return "queued";
    case ApplicationFilesystemPhase::draining:return "draining";
    case ApplicationFilesystemPhase::ready:return "ready";
    case ApplicationFilesystemPhase::recovering:return "recovering";
    case ApplicationFilesystemPhase::refused:return "refused";
    case ApplicationFilesystemPhase::attention:return "attention";
    case ApplicationFilesystemPhase::completed:return "completed";
    default:return "idle";
  }
}
const char *applicationDescription(const ApplicationFilesystemSnapshot &state){
  if(state.phase==ApplicationFilesystemPhase::attention){
    if(state.fault==ApplicationFilesystemFault::mount)return "Storage could not be mounted. Current settings remain in memory. Avoid restart until recovery succeeds; retry recovery after resolving the storage problem. No formatting or automatic restart was requested.";
    if(state.fault==ApplicationFilesystemFault::save)return "Storage is mounted, but current settings could not be verified as saved. Resolve the storage problem, then retry recovery. No automatic restart will follow that retry.";
    return "Recovery needs attention. Resolve the reported problem, then retry recovery. No automatic restart will follow that retry.";
  }
  if(state.phase==ApplicationFilesystemPhase::completed)return state.restartScheduled?
    "Application image received, filesystem remounted and current state saved. Restart scheduled.":
    "Application image received and current state saved. No automatic restart is scheduled; review status before explicitly requesting Restart.";
  if(state.phase==ApplicationFilesystemPhase::ready)return "Storage is prepared for this exact image. Waiting for the file; RF is paused and independent wired controls remain available.";
  if(state.phase==ApplicationFilesystemPhase::draining)return "Current state saved; waiting for open web files to close before unmounting storage.";
  if(state.phase==ApplicationFilesystemPhase::recovering)return "Recovering storage and saving current state. No completion or restart is confirmed yet.";
  if(state.phase==ApplicationFilesystemPhase::refused){
    switch(state.fault){
      case ApplicationFilesystemFault::busy:return "Preparation refused: movement or accepted work is pending. Wait for idle before starting a new upload. No image was written.";
      case ApplicationFilesystemFault::layout:return "Preparation refused: image size or partition layout does not match this device. Select a matching full-size filesystem image. No image was written.";
      case ApplicationFilesystemFault::save:return "Preparation refused: current settings could not be verified as saved. Resolve the storage problem before retrying. No image was written.";
      case ApplicationFilesystemFault::unmount:return "Storage could not be unmounted. No image was written; current settings were saved during recovery. No restart is scheduled.";
      case ApplicationFilesystemFault::radio:return "Radio admission or restoration could not be verified. Review its configuration before retrying. No automatic restart is scheduled.";
      default:return "Preparation or upload was cancelled, expired or refused. Storage recovery completed; no automatic restart is scheduled. Sign in again if security changed, and review before starting a new upload.";
    }
  }
  return "Preparation queued behind accepted work. No image has been written.";
}
void applicationStatus(AsyncWebServerRequest *request,const ApplicationFilesystemSnapshot &state,int code){
  char body[896];const int count=snprintf(body,sizeof(body),
    "{\"status\":\"%s\",\"nonce\":\"%llu\",\"generation\":\"%llu\",\"bytes\":%lu,\"phase\":\"%s\",\"fault\":%u,\"uploaded\":%s,\"restartScheduled\":%s,\"restartReason\":%u,\"desc\":\"%s\"}",
    code==202?"QUEUED":"OK",static_cast<unsigned long long>(state.source.nonce),static_cast<unsigned long long>(state.owner.generation),
    static_cast<unsigned long>(state.source.bytes),applicationPhase(state.phase),unsigned(state.fault),state.uploaded?"true":"false",
    state.restartScheduled?"true":"false",unsigned(state.restartReason),applicationDescription(state));
  if(count<0||size_t(count)>=sizeof(body)){applicationError(request,503,"Application status unavailable.");return;}
  request->send(code,_encoding_json,body);
}
void retireAdoptedApplication(AsyncWebServerRequest *request){
  const auto *state=manualUploadState(request);
  if(state&&state->kind==ManualUpdateKind::application&&state->applicationAdopted)gitUpdate.retireTransfer(state->owner);
}
bool applicationSource(AsyncWebServerRequest *request,ApplicationFilesystemSnapshot &state){
  uint64_t nonce=0,generation=0;
  if(!applicationIdentity(asyncParam(request,"nonce"),nonce)||!applicationIdentity(asyncParam(request,"generation"),generation)){
    applicationError(request,400,"A valid prepared application identity is required. No image was admitted by this check.");return false;
  }
  state=applicationFilesystemStatus();
  if(state.source.nonce!=nonce||state.owner.generation!=generation){applicationError(request,409,"Application source is stale. Do not resend automatically.");return false;}
  return true;
}
}
void registerApplicationUploadRoute(AsyncWebServer &server){
  server.on(AsyncURIMatcher::exact("/prepareApplication"),HTTP_POST,[](AsyncWebServerRequest *request){
    if(!applicationAuthenticated(request))return;
    const auto revision=securityConfigurationRevision();uint64_t bytes=0;
    if(!revision||!applicationIdentity(asyncParam(request,"bytes"),bytes)||bytes>UINT32_MAX){applicationError(request,400,"Specify the exact nonzero filesystem image size.");return;}
    uint64_t nonce=0;for(unsigned n=0;n<4&&!nonce;++n)nonce=(uint64_t(esp_random())<<32)|esp_random();
    if(!nonce){applicationError(request,503,"Application preparation entropy is unavailable.");return;}
    GitUpdateOwnerToken owner;const ManualApplicationSource source{nonce,revision,uint32_t(bytes)};
    if(!gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,owner)){applicationError(request,409,"Another owned operation is active.");return;}
    if(restartTransferBlocked()||securityConfigurationRevision()!=revision||!webServer.isAuthenticated(request)||
       !gitUpdate.beginTransfer(owner,ManualUpdateKind::application,applicationRouteNow())){
      gitUpdate.releaseExternal(owner);applicationError(request,409,"Application preparation is unavailable or its authentication changed.");return;
    }
    if(!gitUpdate.prepareApplication(owner,source,applicationRouteNow())||!noteApplicationPreparation(owner,source)||
       !submitModelCommit({},ModelCommitMode::applicationPreparation,owner)){
      refuseUnpublishedApplication(owner);gitUpdate.cancelApplicationPreparation(owner,source);gitUpdate.retireTransfer(owner);
      applicationError(request,503,"Preparation was not queued. Nothing was unmounted or written.");return;
    }
    // This metadata-only request never installs an upload-disconnect hook.
    applicationStatus(request,applicationFilesystemStatus(),202);
  });
  server.on(AsyncURIMatcher::exact("/applicationUpdateStatus"),HTTP_GET,[](AsyncWebServerRequest *request){
    if(!applicationAuthenticated(request))return;
    ApplicationFilesystemSnapshot state;
    if(applicationSource(request,state))applicationStatus(request,state,200);
  });
  server.on(AsyncURIMatcher::exact("/applicationUpdate"),HTTP_POST,[](AsyncWebServerRequest *request){
    if(!applicationAuthenticated(request))return;
    ApplicationFilesystemSnapshot state;
    if(!applicationSource(request,state))return;
    const auto action=asyncParam(request,"action");bool accepted=false;
    if(action=="retry")accepted=requestApplicationFilesystemRecovery(state.owner,state.source);
    else if(action=="cancel")accepted=gitUpdate.cancelApplicationPreparation(state.owner,state.source)||gitUpdate.retireTransfer(state.owner);
    else {applicationError(request,400,"Choose cancel or retry recovery.");return;}
    if(!accepted){applicationError(request,409,"This source cannot accept that action. Nothing was replayed.");return;}
    applicationStatus(request,applicationFilesystemStatus(),202);
  });
  server.on(AsyncURIMatcher::exact("/updateApplication"),HTTP_POST,
    [](AsyncWebServerRequest *request){
      if(request->getAttribute("applicationUploadRejected",false))return;
      if(!applicationAuthenticated(request)){retireAdoptedApplication(request);return;}
      ApplicationFilesystemSnapshot state;
      if(!applicationSource(request,state)){retireAdoptedApplication(request);return;}
      if(!finishPreparedApplication(request,state.owner,state.source)){
        const auto *upload=manualUploadState(request);
        if(upload&&upload->applicationAdopted)gitUpdate.retireTransfer(upload->owner);
        applicationError(request,409,"No completed authenticated single-file application upload was admitted. Recovery may still be pending; do not resend automatically.");return;
      }
      applicationStatus(request,applicationFilesystemStatus(),202);
    },
    [](AsyncWebServerRequest *request,String,size_t index,uint8_t *data,size_t len,bool final){
      if(request->getAttribute("applicationUploadRejected",false))return;
      ApplicationFilesystemSnapshot state;
      if(!applicationAuthenticated(request)||!applicationSource(request,state)){
        retireAdoptedApplication(request);request->setAttribute("applicationUploadRejected",true);return;
      }
      receivePreparedApplication(request,state.owner,state.source,index,data,len,final);resetTaskWdt();
    });
}
#endif
