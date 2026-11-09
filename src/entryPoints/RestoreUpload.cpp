#include "ConfigFile.h"
#include "RestoreExecution.h"
#include "Web.h"
#include <stdio.h>
#include "ManualUploadSession.h"
#include "TaskWdt.h"
#include "WebRoutes.h"
#include <LittleFS.h>
extern Web webServer;
namespace {
// Canonical decimal strings avoid 64-bit rounding in browsers and reject
// ambiguous or overflowed identities before observing any cached job.
bool identity(const String &text,uint64_t &out){
  if(!text.length()||text.length()>20||text.c_str()[0]=='0')return false;
  uint64_t value=0;
  for(unsigned i=0;i<text.length();++i){const char c=text.c_str()[i];
    if(c<'0'||c>'9'||value>(UINT64_MAX-uint64_t(c-'0'))/10)return false;
    value=value*10+uint64_t(c-'0');
  }
  out=value;return value!=0;
}
const char *phaseName(RestoreExecutionPhase phase){
  switch(phase){
    case RestoreExecutionPhase::idle:return "idle";
    case RestoreExecutionPhase::queued:return "queued";
    case RestoreExecutionPhase::running:return "running";
    case RestoreExecutionPhase::cleanupPending:return "cleanupPending";
    case RestoreExecutionPhase::applied:return "applied";
    case RestoreExecutionPhase::partial:return "partial";
    case RestoreExecutionPhase::refused:return "refused";
    case RestoreExecutionPhase::cleanupFault:return "cleanupFault";
    default:return "unavailable";
  }
}
void sendStatus(AsyncWebServerRequest *request,const RestoreExecutionSnapshot &status,int code){
  char body[768];
  const int length=snprintf(body,sizeof(body),
    "{\"status\":\"%s\",\"bootSession\":\"%llu\",\"generation\":\"%llu\",\"phase\":\"%s\",\"fault\":%u,\"dataComplete\":%s,\"modelApplied\":%s,\"counterWriteAttempted\":%s,\"restartScheduled\":%s,\"restartBlocked\":%s,\"settingsState\":%u,\"requestedNamespaces\":%u,\"verifiedNamespaces\":%u,\"appliedNamespaces\":%u,\"desc\":\"%s\"}",
    code==202?"QUEUED":"OK",static_cast<unsigned long long>(status.bootSession),static_cast<unsigned long long>(status.generation),
    phaseName(status.phase),unsigned(status.fault),status.apply.complete?"true":"false",status.apply.modelApplied?"true":"false",
    status.apply.counterWriteAttempted?"true":"false",status.restartScheduled?"true":"false",status.restartBlocked?"true":"false",
    unsigned(status.apply.settings.state),unsigned(status.apply.settings.requestedNamespaces),unsigned(status.apply.settings.verifiedNamespaces),
    unsigned(status.apply.settings.appliedNamespaces),restoreExecutionMessage(status));
  if(length<0||size_t(length)>=sizeof(body)){request->send(503,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Restore status is unavailable.\"}");return;}
  request->send(code,_encoding_json,body);
}
uint8_t parts(const restore_options_t &opts) {
  return (opts.settings?RESTORE_SETTINGS:0)|(opts.shades?RESTORE_SHADES:0)|
      (opts.network?RESTORE_NETWORK:0)|(opts.transceiver?RESTORE_TRANSCEIVER:0)|
      (opts.repeaters?RESTORE_REPEATERS:0)|(opts.mqtt?RESTORE_MQTT:0);
}
}
void registerRestoreUploadRoute(AsyncWebServer &server) {
  server.on(AsyncURIMatcher::exact("/restoreStatus"),HTTP_GET,[](AsyncWebServerRequest *request){
    if(!webServer.isAuthenticated(request)){request->send(401,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Authentication required.\"}");return;}
    uint64_t boot=0,generation=0;
    if(!identity(asyncParam(request,"bootSession"),boot)||!identity(asyncParam(request,"generation"),generation)){
      request->send(400,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Invalid restore identity.\"}");return;
    }
    const auto status=restoreExecutionStatus();
    if(!status.bootSession){request->send(503,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Restore status is unavailable.\"}");return;}
    if(status.bootSession!=boot||status.generation!=generation){request->send(409,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"This restore status is stale. Do not resubmit automatically.\"}");return;}
    sendStatus(request,status,200);
  });
  server.on(AsyncURIMatcher::exact("/restore"),HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if(!claimManualRestore(request)) {
        request->send(409,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"This request has no completed owned backup upload. Nothing was restored.\"}");return;
      }
      restore_options_t opts;
      if(asyncHasParam(request,"data")) {
        const String data=asyncParam(request,"data");
        JsonDocument doc;
        if(data.length()>256 || deserializeJson(doc,data.c_str(),data.length()) || !doc.is<JsonObject>()) {
          discardManualRestore(request);
          request->send(400,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Invalid restore options; nothing was restored.\"}");return;
        }
        auto object=doc.as<JsonObject>();opts.fromJSON(object);
      } else opts.shades=true;
      const auto selected=parts(opts);
      if(!selected || !ShadeConfigFile::validateFile("/shades.tmp")) {
        discardManualRestore(request);
        request->send(400,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"No backup section selected or backup invalid; nothing was restored.\"}");return;
      }
      if(!queueManualRestore(request,selected)) return; // queueLoopItem reports full admission.
      // Loop execution can finish before this asynchronous response. Bind the
      // reply to THIS request, never a newer job that replaced the cache.
      RestoreExecutionSnapshot queued;
      queued.bootSession=restoreExecutionStatus().bootSession;
      queued.generation=manualUploadState(request)->owner.generation;
      queued.phase=RestoreExecutionPhase::queued;queued.fault=RestoreExecutionFault::none;
      sendStatus(request,queued,202);
    },
    [](AsyncWebServerRequest *request,String,size_t index,uint8_t *data,size_t len,bool final) {
      if(!uploadAllowed(request,index)) return;
      receiveManualRestore(request,index,data,len,final);resetTaskWdt();
    });
}
