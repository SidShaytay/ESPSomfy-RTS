#include "ManualUploadSession.h"
#include "ModelCommitService.h"
#include "WebRoutes.h"
#include "MqttConnection.h"
#include "Somfy.h"
#include "Utils.h"
#include "SafeRestart.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Update.h>
#include <esp_timer.h>
#include <new>
#include <stdlib.h>
extern SomfyShadeController somfy;
extern MqttConnection mqtt;

namespace {
uint64_t now() { return uint64_t(esp_timer_get_time())/1000; }
void close(ManualUploadState &state) {
  if(gitUpdate.closeTransfer(state.owner)) gitUpdate.releaseExternal(state.owner);
}
bool queueModel(ManualUploadState &state,bool restart) {
  if(state.kind!=ManualUpdateKind::application || !state.started || state.modelAttempted) return false;
  state.modelAttempted=true;
  const bool held=restart ? gitUpdate.transferRestartHeld(state.owner,now()) : gitUpdate.holdTransferRecovery(state.owner,now());
  if(!held) return false;
  // Publish only after retiring this SDK operation claim. The same owner stays
  // held; loop can immediately claim the exact marker without racing this call.
  gitUpdate.finishTransfer(state.owner,now(),restart);
  state.modelQueued=gitUpdate.transferContinuing(state.owner,now()) &&
      submitModelCommit({},restart ? ModelCommitMode::applicationRecoveryAndRestart : ModelCommitMode::applicationRecovery,state.owner);
  if(!state.modelQueued && gitUpdate.claimTransferRecovery(state.owner,now())) close(state);
  return true;
}
void fail(ManualUploadState &state,bool abortSdk) {
  state.failed=true;
  if(abortSdk) {
    Update.abort(); // Caller holds the exact request's operation claim.
    if(Update.isRunning()) {
      gitUpdate.retireTransfer(state.owner);gitUpdate.finishTransfer(state.owner,now(),false);return;
    }
  }
  gitUpdate.transferResource(state.owner,false);
  if(!queueModel(state,false)) close(state);
}
ManualUploadState *begin(AsyncWebServerRequest *request,ManualUpdateKind kind) {
  if(request->getAttribute("manualAttempted",false)) return nullptr;
  request->setAttribute("manualAttempted",true);
  if(request->_tempObject) return nullptr;
  auto *memory=malloc(sizeof(ManualUploadState));
  if(!memory) return nullptr;
  auto *state=new(memory) ManualUploadState{};
  state->kind=kind;request->_tempObject=state;
  request->setAttribute("manualState",true);
  if(!gitUpdate.acquireExternal(kind==ManualUpdateKind::restore ? GitUpdateExternalOwner::restore : GitUpdateExternalOwner::manual,state->owner)) { state->failed=true;return state; }
  // Checked AFTER owner acquisition: no counter edit can begin between this
  // observation and SDK begin/shutdown. The owner is held through cleanup.
  if(restartTransferBlocked() ||
     !gitUpdate.beginTransfer(state->owner,kind,now())) {
    state->failed=true;gitUpdate.releaseExternal(state->owner);return state;
  }
  const auto owner=state->owner;
  request->onDisconnect([owner]() { gitUpdate.retireTransfer(owner); });
  return state;
}
}
ManualUploadState *manualUploadState(AsyncWebServerRequest *request) {
  return request->getAttribute("manualState",false) ? static_cast<ManualUploadState*>(request->_tempObject) : nullptr;
}
void receiveManualImage(AsyncWebServerRequest *request,ManualUpdateKind kind,size_t index,
                        uint8_t *data,size_t len,bool final) {
  auto *state=index==0 ? begin(request,kind) : manualUploadState(request);
  if(!state || state->failed || state->finalReceived || state->kind!=kind) return;
  if(index!=state->received || len>SIZE_MAX-state->received) {
    state->failed=true;gitUpdate.retireTransfer(state->owner);return;
  }
  if(!gitUpdate.claimTransfer(state->owner,now())) { state->failed=true;return; }
  if(index==0) {
    if(!Update.begin(UPDATE_SIZE_UNKNOWN,kind==ManualUpdateKind::application ? U_SPIFFS : U_FLASH)) {
      // A failed begin never acquired the SDK object. Do not inspect/abort a
      // potentially foreign Update, and do not shut down hardware or save.
      state->failed=true;close(*state);return;
    }
    state->started=true;gitUpdate.transferResource(state->owner,true);
    if(!gitUpdate.transferContinuing(state->owner,now())) { fail(*state,true);return; }
    somfy.transceiver.end();mqtt.end();
  }
  if((len && Update.write(data,len)!=len) || !gitUpdate.transferContinuing(state->owner,now())) {
    fail(*state,true);return;
  }
  state->received+=len;
  if(final) {
    state->finalReceived=true;
    bool uploaded=Update.end(true) && !Update.hasError();
    // SDK end retires our SDK ownership, including error outcomes. Only an
    // SDK still running under this exact operation may be aborted here.
    if(Update.isRunning()) {
      uploaded=false;Update.abort();
      if(Update.isRunning()) {
        state->failed=true;gitUpdate.retireTransfer(state->owner);
        gitUpdate.finishTransfer(state->owner,now(),false);return;
      }
    }
    gitUpdate.transferResource(state->owner,false);
    state->uploaded=uploaded && gitUpdate.transferContinuing(state->owner,now()) && gitUpdate.holdTransferRestart(state->owner);
    if(!state->uploaded) { fail(*state,false);return; }
    if(kind==ManualUpdateKind::application) {
      if(!queueModel(*state,true)) close(*state);
      return;
    }
    if(!gitUpdate.finishTransfer(state->owner,now(),true)) {state->failed=true;return;}
    const auto submitted=submitOwnedRestart(state->owner,500);
    state->restartQueued=submitted==RestartSubmission::queued;
    state->restartReason=state->restartQueued ? RestartReason::none :
        (submitted==RestartSubmission::busy ? RestartReason::pending : RestartReason::notReady);
    if(!state->restartQueued)gitUpdate.releaseRestartSource(state->owner,now());
    return;
  }
  gitUpdate.finishTransfer(state->owner,now(),len!=0 || final);
}
bool claimManualRestore(AsyncWebServerRequest *request) {
  const auto *state=manualUploadState(request);
  return state && state->kind==ManualUpdateKind::restore && state->uploaded &&
      !state->failed && !state->modelAttempted && gitUpdate.claimTransfer(state->owner,now());
}
void discardManualRestore(AsyncWebServerRequest *request) {
  auto *state=manualUploadState(request);
  if(!state || state->kind!=ManualUpdateKind::restore || !gitUpdate.ownsExternal(state->owner)) return;
  state->failed=true;
  LittleFS.remove("/shades.tmp");gitUpdate.transferResource(state->owner,false);close(*state);
}
bool queueManualRestore(AsyncWebServerRequest *request,uint8_t parts) {
  auto *state=manualUploadState(request);
  if(!state || state->kind!=ManualUpdateKind::restore || state->modelAttempted) return false;
  state->modelAttempted=true;
  if(!gitUpdate.queueTransfer(state->owner)) {
    // Completion still holds this exact receive claim even if disconnect
    // retired it during validation. Release the claim before loop cleanup.
    state->failed=true;gitUpdate.retireTransfer(state->owner);
    gitUpdate.finishTransfer(state->owner,now(),false);
    request->send(409,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Backup ownership expired; nothing was queued.\"}");return false;
  }
  gitUpdate.finishTransfer(state->owner,now(),true);
  loop_item_t item;item.type=loop_item_type_t::restore_shades;
  item.restoreParts=parts;item.externalOwner=state->owner;
  if(!gitUpdate.transferContinuing(state->owner,now())) {
    request->send(409,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Backup ownership expired; nothing was queued.\"}");return false;
  }
  state->modelQueued=queueLoopItem(request,item);
  if(!state->modelQueued && gitUpdate.claimTransfer(state->owner,now(),true)) discardManualRestore(request);
  return state->modelQueued;
}
void receiveManualRestore(AsyncWebServerRequest *request,size_t index,uint8_t *data,size_t len,bool final) {
  auto *state=index==0 ? begin(request,ManualUpdateKind::restore) : manualUploadState(request);
  if(!state || state->failed || state->finalReceived || state->kind!=ManualUpdateKind::restore) return;
  if(index!=state->received || len>SIZE_MAX-state->received) {
    state->failed=true;gitUpdate.retireTransfer(state->owner);return;
  }
  if(!gitUpdate.claimTransfer(state->owner,now())) {state->failed=true;return;}
  if(index==0) {
    gitUpdate.transferResource(state->owner,true); // Own this temporary namespace before creation.
    auto file=LittleFS.open("/shades.tmp","w");
    if(!file || file.size()!=0) {file.close();discardManualRestore(request);return;}
    file.close();state->started=true;
  }
  if(len) {
    auto file=LittleFS.open("/shades.tmp","a");
    const bool wrote=file && file.write(data,len)==len && file.size()==state->received+len;file.close();
    if(!wrote) {discardManualRestore(request);return;}
  }
  if(!gitUpdate.transferContinuing(state->owner,now())) {discardManualRestore(request);return;}
  state->received+=len;
  if(final) {state->uploaded=true;state->finalReceived=true;}
  gitUpdate.finishTransfer(state->owner,now(),len!=0 || final);
}
void manualUploadLoop() {
  const auto cleanup=gitUpdate.claimTransferCleanup(now());
  if(!cleanup.token.generation) return;
  if(cleanup.resourceOpen) {
    if(cleanup.kind==ManualUpdateKind::restore) LittleFS.remove("/shades.tmp");
    else {
      Update.abort();
      if(Update.isRunning()) {gitUpdate.finishTransfer(cleanup.token,now(),false);return;}
      gitUpdate.transferResource(cleanup.token,false);
      if(cleanup.kind==ManualUpdateKind::application) {
        ManualUploadState recovery;recovery.owner=cleanup.token;
        recovery.kind=ManualUpdateKind::application;recovery.started=true;
        if(queueModel(recovery,false)) return;
      }
    }
    gitUpdate.transferResource(cleanup.token,false);
  }
  if(gitUpdate.closeTransfer(cleanup.token)) gitUpdate.releaseExternal(cleanup.token);
}
