#include "ConfigFile.h"
#include "ManualUploadSession.h"
#include "TaskWdt.h"
#include "WebRoutes.h"
#include <LittleFS.h>
namespace {
uint8_t parts(const restore_options_t &opts) {
  return (opts.settings?RESTORE_SETTINGS:0)|(opts.shades?RESTORE_SHADES:0)|
      (opts.network?RESTORE_NETWORK:0)|(opts.transceiver?RESTORE_TRANSCEIVER:0)|
      (opts.repeaters?RESTORE_REPEATERS:0)|(opts.mqtt?RESTORE_MQTT:0);
}
}
void registerRestoreUploadRoute(AsyncWebServer &server) {
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
      request->send(202,_encoding_json,"{\"status\":\"QUEUED\",\"desc\":\"Owned backup queued for loop validation and restore; it has not yet been applied.\"}");
    },
    [](AsyncWebServerRequest *request,String,size_t index,uint8_t *data,size_t len,bool final) {
      if(!uploadAllowed(request,index)) return;
      receiveManualRestore(request,index,data,len,final);resetTaskWdt();
    });
}
