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
#include "ApplicationUploadResult.h"
#include "ManualUploadSession.h"
#include "TaskWdt.h"
#include "WebRoutes.h"
void registerApplicationUploadRoute(AsyncWebServer &server) {
  // Upload reports only SDK completion and loop-save admission. Ownership
  // does not certify raw filesystem mount/layout or installer compatibility.
  server.on(AsyncURIMatcher::exact("/updateApplication"),HTTP_POST,
    [](AsyncWebServerRequest *request) {
      const auto *state=manualUploadState(request);
      if(!state || !state->started) {
        request->send(503,_encoding_json,"{\"status\":\"ERROR\",\"started\":false,\"desc\":\"Application upload did not start. Retry after any active transfer finishes. No model save or restart was requested.\"}");return;
      }
      const auto response=applicationUploadResponse(state->uploaded,state->modelQueued);
      request->send(response.status,_encoding_json,response.json);
    },
    [](AsyncWebServerRequest *request,String,size_t index,uint8_t *data,size_t len,bool final) {
      if(!uploadAllowed(request,index)) return;
      receiveManualImage(request,ManualUpdateKind::application,index,data,len,final);
      resetTaskWdt();
    });
}

#endif
