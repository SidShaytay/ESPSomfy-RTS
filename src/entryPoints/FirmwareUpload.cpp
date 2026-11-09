#include "ConfigSettings.h"
#include "ManualUploadSession.h"
#include "TaskWdt.h"
#include "WebRoutes.h"
#if defined(ESPSOMFY_BUNDLED_WEB)
#include "Web.h"
#include "SafeRestart.h"
#include <stdio.h>
extern Web webServer;
namespace {
bool firmwareOperation(const String &text,uint64_t &out) {
  if(!text.length()||text.length()>20||text.c_str()[0]=='0')return false;
  uint64_t value=0;
  for(unsigned i=0;i<text.length();++i) {
    const char c=text.c_str()[i];
    if(c<'0'||c>'9'||value>(UINT64_MAX-uint64_t(c-'0'))/10)return false;
    value=value*10+uint64_t(c-'0');
  }
  out=value;return value!=0;
}
}
#endif
extern ConfigSettings settings;
void registerFirmwareUploadRoute(AsyncWebServer &server) {
#if defined(ESPSOMFY_BUNDLED_WEB)
  server.on(AsyncURIMatcher::exact("/firmwareStatus"),HTTP_GET,[](AsyncWebServerRequest *request) {
    if(!webServer.isAuthenticated(request)) {
      request->send(401,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Authentication required.\"}");return;
    }
    uint64_t operation=0;
    if(!firmwareOperation(asyncParam(request,"operationGeneration"),operation)) {
      request->send(400,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Invalid firmware operation.\"}");return;
    }
    const auto status=gitUpdate.snapshot();
    if(!status.manual || status.operation!=operation) {
      request->send(409,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Firmware status is no longer current. Reconnect and review the device; do not resubmit automatically.\"}");return;
    }
    char body[512];
    const int size=snprintf(body,sizeof(body),
      "{\"operationGeneration\":\"%llu\",\"source\":\"manual-file\",\"state\":\"%s\",\"error\":\"%s\",\"progress\":%u,\"busy\":%s,\"restartScheduled\":%s}",
      static_cast<unsigned long long>(status.operation),gitUpdatePhase(status.phase),gitUpdateReason(status.reason),
      unsigned(status.progress),status.busy?"true":"false",status.phase==GitUpdatePhase::written&&restartPending()?"true":"false");
    if(size<0||size_t(size)>=sizeof(body)) {
      request->send(503,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Firmware status unavailable.\"}");return;
    }
    request->send(200,_encoding_json,body);
  });
#endif
  server.on(AsyncURIMatcher::exact("/updateFirmware"),HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if(!settings.hasOtaSlot) {
        request->send(400,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"This device has no OTA slot; no firmware upload started.\"}");return;
      }
      const auto *state=manualUploadState(request);
      if(!state || !state->started) {
        request->send(503,_encoding_json,"{\"status\":\"ERROR\",\"started\":false,\"desc\":\"Firmware upload did not acquire admission. No image write or restart was requested.\"}");return;
      }
      if(!state->uploaded) {
        request->send(500,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Firmware upload failed; no automatic restart was scheduled. Resolve the failure before requesting Restart.\"}");return;
      }
#if defined(ESPSOMFY_BUNDLED_WEB)
      if(state->failed || state->modelAttempted) {
        request->send(409,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Firmware request is no longer current; no new installation was queued.\"}");return;
      }
      if(!queueManualFirmware(request)) {
        if(!request->getAttribute("firmwareQueueRefused",false))
          request->send(503,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Firmware staging or admission failed; no installation was queued.\"}");
        return;
      }
      char body[256];
      snprintf(body,sizeof(body),"{\"status\":\"QUEUED\",\"operationGeneration\":\"%llu\",\"desc\":\"Firmware file received; verification and installation queued. No boot selection has occurred.\"}",static_cast<unsigned long long>(state->operation));
      request->send(202,_encoding_json,body);
      return;
#endif
      if(!state->restartQueued) {
        request->send(503,_encoding_json,"{\"status\":\"ERROR\",\"desc\":\"Firmware was written, but restart was not scheduled. Resolve pending work before explicitly requesting Restart.\"}");return;
      }
      request->send(202,_encoding_json,"{\"status\":\"QUEUED\",\"desc\":\"Firmware written; restart queued subject to final admission.\"}");
    },
    [](AsyncWebServerRequest *request,String,size_t index,uint8_t *data,size_t len,bool final) {
      if(!uploadAllowed(request,index) || !settings.hasOtaSlot) return;
      receiveManualImage(request,ManualUpdateKind::firmware,index,data,len,final);
      resetTaskWdt();
    });
}
