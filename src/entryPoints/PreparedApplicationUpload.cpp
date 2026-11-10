#include "PreparedApplicationUpload.h"
#if !defined(ESPSOMFY_BUNDLED_WEB)
#include "ApplicationFilesystem.h"
#include "ManualUploadSession.h"
#include "SettingsRevisions.h"
#include "Web.h"
#include <Update.h>
#include <esp_timer.h>
#include <new>
#include <cstdlib>
extern Web webServer;
namespace {
uint64_t applicationNow(){const auto value=esp_timer_get_time();return value<0?UINT64_MAX:uint64_t(value)/1000;}
ManualApplicationSource sourceOf(const ManualUploadState &state){return {state.applicationNonce,state.securityRevision,state.expectedBytes};}
bool sameOwner(const GitUpdateOwnerToken &a,const GitUpdateOwnerToken &b){return a.generation&&a.generation==b.generation&&a.kind==b.kind;}
bool authenticated(AsyncWebServerRequest *request,const ManualApplicationSource &source){
  return source.valid()&&source.securityRevision==securityConfigurationRevision()&&webServer.isAuthenticated(request)&&
    source.securityRevision==securityConfigurationRevision();
}
void failApplication(ManualUploadState &state,bool claimed){
  state.failed=true;
  if(!state.applicationAdopted)return;
  gitUpdate.retireTransfer(state.owner);
  if(claimed)gitUpdate.finishTransfer(state.owner,applicationNow(),false);
  // Only loop's exact-source cleanup may abort/remount/save/resume.
}
}
void receivePreparedApplication(AsyncWebServerRequest *request,const GitUpdateOwnerToken &owner,const ManualApplicationSource &source,
                                size_t index,uint8_t *data,size_t len,bool final){
  auto *state=manualUploadState(request);bool claimed=false;
  if(index==0){
    if(request->getAttribute("manualAttempted",false)){
      if(state&&state->kind==ManualUpdateKind::application)failApplication(*state,false);
      return; // a second file can never become a second transfer
    }
    request->setAttribute("manualAttempted",true);
    if(request->_tempObject||!authenticated(request,source))return;
    int command=0;
    if(!applicationFilesystemReady(owner,source,command))return;
    auto *memory=malloc(sizeof(ManualUploadState));if(!memory)return;
    state=new(memory)ManualUploadState{};request->_tempObject=state;request->setAttribute("manualState",true);
    state->owner=owner;state->kind=ManualUpdateKind::application;state->securityRevision=source.securityRevision;
    state->applicationNonce=source.nonce;state->expectedBytes=source.bytes;
    if(!gitUpdate.claimPreparedApplication(owner,source,applicationNow())){state->failed=true;return;}
    claimed=true;state->applicationAdopted=true;
    request->onDisconnect([owner](){gitUpdate.retireTransfer(owner);});
    if(!authenticated(request,source)||!gitUpdate.transferContinuing(owner,applicationNow())){failApplication(*state,true);return;}
    if(!Update.begin(source.bytes,command)){
      // No SDK ownership: never inspect or abort a potentially foreign object.
      failApplication(*state,true);return;
    }
    state->started=true;
    if(!gitUpdate.transferResource(owner,true)){failApplication(*state,true);return;}
  }
  if(!state||state->kind!=ManualUpdateKind::application)return;
  if(state->failed)return;
  if(state->finalReceived||!sameOwner(state->owner,owner)||!sourceOf(*state).same(source)||
     index!=state->received||state->received>source.bytes||len>source.bytes-state->received||
     (final&&len!=source.bytes-state->received)||!authenticated(request,source)){
    failApplication(*state,claimed);return;
  }
  if(!claimed&&!gitUpdate.claimTransfer(owner,applicationNow())){state->failed=true;return;}
  if(!gitUpdate.transferContinuing(owner,applicationNow())||!authenticated(request,source)||
     (len&&Update.write(data,len)!=len)||!authenticated(request,source)||!gitUpdate.transferContinuing(owner,applicationNow())){
    failApplication(*state,true);return;
  }
  state->received+=len;
  if(final){
    state->finalReceived=true;
    if(state->received!=source.bytes){failApplication(*state,true);return;}
    const bool ended=Update.end(false);
    if(Update.isRunning()){failApplication(*state,true);return;}
    if(!gitUpdate.transferResource(owner,false)){failApplication(*state,true);return;}
    state->uploaded=ended&&!Update.hasError()&&authenticated(request,source)&&gitUpdate.transferContinuing(owner,applicationNow());
    if(!state->uploaded){failApplication(*state,true);return;}
  }
  if(!gitUpdate.finishTransfer(owner,applicationNow(),len!=0||final))state->failed=true;
}
bool finishPreparedApplication(AsyncWebServerRequest *request,const GitUpdateOwnerToken &owner,const ManualApplicationSource &source){
  auto *state=manualUploadState(request);
  if(!state||state->kind!=ManualUpdateKind::application||state->modelAttempted)return false;
  state->modelAttempted=true;
  if(state->failed||!state->uploaded||!state->finalReceived||state->received!=source.bytes||!sameOwner(state->owner,owner)||
     !sourceOf(*state).same(source)||!authenticated(request,source)){
    failApplication(*state,false);return false;
  }
  if(!gitUpdate.claimTransfer(owner,applicationNow())){state->failed=true;return false;}
  if(!authenticated(request,source)||!completePreparedApplication(owner,source)){
    failApplication(*state,true);return false;
  }
  state->modelQueued=gitUpdate.finishTransfer(owner,applicationNow(),true);
  if(!state->modelQueued)state->failed=true;
  return state->modelQueued;
}
#endif
