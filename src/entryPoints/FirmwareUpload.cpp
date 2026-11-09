#include "ConfigSettings.h"
#include "ManualUploadSession.h"
#include "TaskWdt.h"
#include "WebRoutes.h"
extern ConfigSettings settings;
void registerFirmwareUploadRoute(AsyncWebServer &server) {
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
      request->send(202,_encoding_json,"{\"status\":\"QUEUED\",\"desc\":\"Firmware file received; verification and installation queued. No boot selection has occurred.\"}");
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
