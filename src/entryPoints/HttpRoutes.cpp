#include <LittleFS.h>
#include <Update.h>
#include "TaskWdt.h"
#include "esp_log.h"
#include "ConfigSettings.h"
#include "ConfigFile.h"
#include "Utils.h"
#include "SSDP.h"
#include "Somfy.h"
#include "LoopQueue.h"
#include "Web.h"
#include "WebRoutes.h"
#include "MqttConnection.h"
#include "HttpAccess.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <atomic>
#include "RestoreUpload.h"
#include "ManualUpload.h"
#ifdef ESPSOMFY_GITHUB_UPDATES
#include "GitUpdate.h"
#endif

// HttpRoutes: the HTTP routes on port 80, the web UI's files and the few that
// stay HTTP (docs/adr/0006-two-security-modes-eleven-http-routes.md).
// Everything else is an op on the socket at /ws.

extern ConfigSettings settings;
extern SSDPClass SSDP;
extern SomfyShadeController somfy;
extern Web webServer;
extern MqttConnection mqtt;
extern LoopQueue loopQueue;

static const char *TAG = "Web";

// A restore's options travel through the loop queue as RESTORE_* bits.
static uint8_t restoreParts(const restore_options_t &opts) {
  return (opts.settings ? RESTORE_SETTINGS : 0) | (opts.shades ? RESTORE_SHADES : 0) |
    (opts.network ? RESTORE_NETWORK : 0) | (opts.transceiver ? RESTORE_TRANSCEIVER : 0) |
    (opts.repeaters ? RESTORE_REPEATERS : 0) | (opts.mqtt ? RESTORE_MQTT : 0);
}
// The token check (requireToken in Web.cpp) runs once the body is in, after
// every chunk was handled, so an upload checks the token on its first chunk
// and drops every chunk of a refused one.
static bool uploadAllowed(AsyncWebServerRequest *request, size_t index) {
  if(index == 0) request->setAttribute("uploadAllowed", webServer.isAuthenticated(request));
  return request->getAttribute("uploadAllowed", false);
}
// A restore and a manual image upload share the GitHub worker's gate on the
// Plus. Generic builds still serialize their local upload/restore operations.
struct HttpUpdateGate {
#ifndef ESPSOMFY_GITHUB_UPDATES
  std::atomic<bool> owned{false};
#endif
  bool beginManualUpdate() {
    if(loopQueue.rebootRequested()) return false;
#ifdef ESPSOMFY_GITHUB_UPDATES
    return gitUpdate.beginManualUpdate();
#else
    bool expected = false;
    return owned.compare_exchange_strong(expected, true);
#endif
  }
  void endManualUpdate() {
#ifdef ESPSOMFY_GITHUB_UPDATES
    gitUpdate.endManualUpdate();
#else
    owned.store(false);
#endif
  }
};
static HttpUpdateGate httpUpdateGate;
// A queued restore that fails on loop() must relinquish ownership too.
void releaseRestoreUpdateGate() { httpUpdateGate.endManualUpdate(); }

// The adapter keeps activation out of the multipart chunk callback.
struct ManualUpdateWriter {
  bool begin(int command) { return Update.begin(UPDATE_SIZE_UNKNOWN,command); }
  size_t write(uint8_t *data,size_t size) { return Update.write(data,size); }
  bool end() { return Update.end(true); }
  bool isRunning() { return Update.isRunning(); }
  bool hasError() { return Update.hasError(); }
  void abort() { Update.abort(); }
};
struct ManualUpdateEffects {
  ManualUploadDeadline deadline;
  void quiesce() {
    somfy.transceiver.end();
    mqtt.end();
    // Completion middleware can refuse a token changed during an upload.
    // Even a client keeping that connection open cannot strand servicing.
    deadline.begin(millis());
    loopQueue.requestStateCommit();
    loopQueue.requestReboot(180000);
  }
  bool expired() { return deadline.expired(millis()); }
  void reboot() {
    loopQueue.requestStateCommit();
    deadline.shorten(millis());
    loopQueue.requestReboot(500);
  }
};
static ManualUpdateWriter manualUpdateWriter;
static ManualUpdateEffects manualUpdateEffects;

#ifdef ESPSOMFY_GITHUB_UPDATES
// Browsers on another origin cannot initiate an install even in the None
// security mode. Scripts without Origin use the normal API-token policy.
static bool allowGitUpdateControl(AsyncWebServerRequest *request) {
  const AsyncWebHeader *origin = request->getHeader("Origin");
  const AsyncWebHeader *host = request->getHeader("Host");
  if(!originMatchesHost(origin ? origin->value().c_str() : nullptr, host ? host->value().c_str() : nullptr)) {
    request->send(403, _encoding_json, F("{\"status\":\"ERROR\",\"desc\":\"Update control requires the controller's own page.\"}"));
    return false;
  }
  return true;
}
static void sendGitUpdateStatus(AsyncWebServerRequest *request, int code) {
  AsyncJsonResponse *response = new AsyncJsonResponse();
  gitUpdate.status(response->getRoot().to<JsonObject>());
  response->setCode(code);
  response->addHeader("Cache-Control", "no-store");
  response->setLength();
  request->send(response);
}
#endif
// Every route is an exact match.  The library's default also takes any path
// below the route, and /backup/a.js then passed the token check as a web asset.
void registerHttpRoutes(AsyncWebServer &server) {
  server.on(AsyncURIMatcher::exact("/loginContext"), HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncJsonResponse *response = new AsyncJsonResponse();
    JsonObject root = response->getRoot().to<JsonObject>();
    root["type"] = static_cast<uint8_t>(settings.Security.type);
    root["serverId"] = settings.serverId;
    root["version"] = settings.fwVersion.name;
    root["model"] = "ESPSomfyRTS";
    root["hostname"] = settings.hostname;
    response->setLength();
    request->send(response);
  });
  // Commands for scripts, as a JSON body or query parameters.
  server.addHandler(new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact("/shadeCommand"),
    [](AsyncWebServerRequest *request, JsonVariant &json) { webServer.handleShadeCommand(request, json); }));
  server.on(AsyncURIMatcher::exact("/shadeCommand"), HTTP_GET, [](AsyncWebServerRequest *request) { JsonVariant v; webServer.handleShadeCommand(request, v); });

  server.addHandler(new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact("/groupCommand"),
    [](AsyncWebServerRequest *request, JsonVariant &json) { webServer.handleGroupCommand(request, json); }));
  server.on(AsyncURIMatcher::exact("/groupCommand"), HTTP_GET, [](AsyncWebServerRequest *request) { JsonVariant v; webServer.handleGroupCommand(request, v); });

  server.addHandler(new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact("/tiltCommand"),
    [](AsyncWebServerRequest *request, JsonVariant &json) { webServer.handleTiltCommand(request, json); }));
  server.on(AsyncURIMatcher::exact("/tiltCommand"), HTTP_GET, [](AsyncWebServerRequest *request) { JsonVariant v; webServer.handleTiltCommand(request, v); });

  server.on(AsyncURIMatcher::exact("/upnp.xml"), HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/xml");
    SSDP.schema(*response);
    request->send(response);
  });

  server.addHandler(new AsyncCallbackJsonWebHandler(AsyncURIMatcher::exact("/login"),
    [](AsyncWebServerRequest *request, JsonVariant &json) { webServer.handleLogin(request, json); }));
  server.on(AsyncURIMatcher::exact("/login"), HTTP_GET, [](AsyncWebServerRequest *request) { JsonVariant v; webServer.handleLogin(request, v); });

  server.on(AsyncURIMatcher::exact("/backup"), HTTP_GET, [](AsyncWebServerRequest *request) { webServer.handleBackup(request); });

  server.on(AsyncURIMatcher::exact("/restore"), HTTP_POST,
    [](AsyncWebServerRequest *request) {
      // Only this request's complete upload can be restored. Another client
      // cannot borrow completion state or release the owner's update gate.
      if(restoreUploadComplete(request)) {
        restore_options_t opts;
        if(asyncHasParam(request, "data")) {
          String dataStr = asyncParam(request, "data");
          ESP_LOGD(TAG, "%s", dataStr.c_str());
          JsonDocument doc;
          DeserializationError err = deserializeJson(doc, dataStr);
          if(err) {
            discardRestoreUpload(request, httpUpdateGate, LittleFS);
            request->send(500, _encoding_json, "{\"status\":\"ERROR\",\"desc\":\"JSON parse error\"}");
            return;
          }
          else {
            JsonObject obj = doc.as<JsonObject>();
            opts.fromJSON(obj);
          }
        }
        else {
          ESP_LOGD(TAG, "No restore options sent.  Using defaults...");
          opts.shades = true;
        }
        // Otherwise the device would reboot having restored nothing.
        const uint8_t parts = restoreParts(opts);
        if(parts == 0) {
          discardRestoreUpload(request, httpUpdateGate, LittleFS);
          request->send(400, _encoding_json, F("{\"status\":\"ERROR\",\"desc\":\"No part of the backup was chosen, so nothing was restored.\"}"));
          return;
        }
        // Checked here so the client hears about a bad file.  It only reads
        // the file; the shades are still changed on loop() alone.
        if(!ShadeConfigFile::validateFile("/shades.tmp")) {
          discardRestoreUpload(request, httpUpdateGate, LittleFS);
          request->send(400, _encoding_json, F("{\"status\":\"ERROR\",\"desc\":\"This isn't a valid backup file, so nothing was restored.\"}"));
          return;
        }
        // The main loop restores, since it owns the shades and their rolling
        // codes, and then reboots.
        loop_item_t item;
        item.type = loop_item_type_t::restore_shades;
        item.restoreParts = parts;
        // Keep the gate across the handoff: the loop may consume immediately.
        queueRestoreUpload(request);
        if(!queueLoopItem(request, item)) {
          request->setAttribute("restoreQueued", false);
          discardRestoreUpload(request, httpUpdateGate, LittleFS);
          return;
        }
        request->send(200, _encoding_json, "{\"status\":\"Success\",\"desc\":\"Restoring Shade settings\"}");
      }
      else {
        discardRestoreUpload(request, httpUpdateGate, LittleFS);
        request->send(409, _encoding_json, F("{\"status\":\"ERROR\",\"desc\":\"Restore upload was refused, incomplete, or invalid; nothing was restored.\"}"));
      }
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if(!uploadAllowed(request, index)) return;
      resetTaskWdt();
      if(index == 0) {
        if(!beginRestoreUpload(request, httpUpdateGate, LittleFS)) return;
        File fup = LittleFS.open("/shades.tmp", "w");
        if(!fup) { discardRestoreUpload(request, httpUpdateGate, LittleFS); return; }
        fup.close();
      }
      if(!request->getAttribute("restoreOwned", false)) return;
      if(len > 0) {
        File fup = LittleFS.open("/shades.tmp", "a");
        if(!fup || fup.write(data, len) != len) {
          fup.close();
          discardRestoreUpload(request, httpUpdateGate, LittleFS);
          return;
        }
        fup.close();
      }
      if(final) finishRestoreUpload(request);
    });

#ifdef ESPSOMFY_GITHUB_UPDATES
  // These exact routes use the same API-token middleware as manual updates.
  // Mutations are POST only; no token or release URL is accepted in a query.
  server.on(AsyncURIMatcher::exact("/githubUpdate"), HTTP_GET, [](AsyncWebServerRequest *request) {
    sendGitUpdateStatus(request, 200);
  });
  server.on(AsyncURIMatcher::exact("/githubUpdate/check"), HTTP_POST, [](AsyncWebServerRequest *request) {
    if(!allowGitUpdateControl(request)) return;
    sendGitUpdateStatus(request, gitUpdate.requestCheck() ? 202 : 409);
  });
  server.on(AsyncURIMatcher::exact("/githubUpdate/install"), HTTP_POST, [](AsyncWebServerRequest *request) {
    if(!allowGitUpdateControl(request)) return;
    sendGitUpdateStatus(request, gitUpdate.requestInstall() ? 202 : 409);
  });
  server.on(AsyncURIMatcher::exact("/githubUpdate/cancel"), HTTP_POST, [](AsyncWebServerRequest *request) {
    if(!allowGitUpdateControl(request)) return;
    sendGitUpdateStatus(request, gitUpdate.requestCancel() ? 202 : 409);
  });
#endif

  // Manual uploads activate only after the entire one-file request completes.
  server.on(AsyncURIMatcher::exact("/updateFirmware"), HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if(!settings.hasOtaSlot) {
        request->send(400, _encoding_json, F("{\"status\":\"ERROR\",\"desc\":\"This device has no OTA slot, so its firmware is flashed over USB.\"}"));
        return;
      }
      const ManualUploadResult result=finishManualUpload(request,manualUpdateWriter,manualUpdateEffects);
      if(result==ManualUploadResult::notStarted)
        request->send(409, _encoding_json, F("{\"status\":\"ERROR\",\"desc\":\"Upload was not accepted: another update is active, initialization failed, or no file was sent.\"}"));
      else if(result==ManualUploadResult::failed)
        request->send(400, _encoding_json, F("{\"status\":\"ERROR\",\"desc\":\"Firmware upload failed, timed out, or contained more than one file. The controller will restart.\"}"));
      else
        request->send(200, _encoding_json, F("{\"status\":\"SUCCESS\",\"desc\":\"Successfully updated firmware\"}"));
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if(!uploadAllowed(request,index)) { failManualUpload(request,manualUpdateWriter,manualUpdateEffects);return; }
      if(!settings.hasOtaSlot) return;
      if(index==0 && !beginManualUpload(request,httpUpdateGate,manualUpdateWriter,manualUpdateEffects,U_FLASH)) return;
      writeManualUpload(request,manualUpdateWriter,manualUpdateEffects,data,len,final);
      resetTaskWdt();
    });

  server.on(AsyncURIMatcher::exact("/updateApplication"), HTTP_POST,
    [](AsyncWebServerRequest *request) {
#ifdef ESPSOMFY_GITHUB_UPDATES
      // Shared LittleFS replacement is not an atomic part of firmware OTA.
      request->send(409, _encoding_json, F("{\"status\":\"ERROR\",\"desc\":\"Filesystem changes require coherent provisioning during an approved service window. Web filesystem upload is disabled on this profile.\"}"));
#else
      const ManualUploadResult result=finishManualUpload(request,manualUpdateWriter,manualUpdateEffects);
      if(result==ManualUploadResult::notStarted)
        request->send(409, _encoding_json, F("{\"status\":\"ERROR\",\"desc\":\"Upload was not accepted: another update is active, initialization failed, or no file was sent.\"}"));
      else if(result==ManualUploadResult::failed)
        request->send(400, _encoding_json, F("{\"status\":\"ERROR\",\"desc\":\"Application upload failed, timed out, or contained more than one file. The controller will restart.\"}"));
      else
        request->send(200, _encoding_json, F("{\"status\":\"SUCCESS\",\"desc\":\"Successfully updated application\"}"));
#endif
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
#ifndef ESPSOMFY_GITHUB_UPDATES
      if(!uploadAllowed(request,index)) { failManualUpload(request,manualUpdateWriter,manualUpdateEffects);return; }
      if(index==0 && !beginManualUpload(request,httpUpdateGate,manualUpdateWriter,manualUpdateEffects,U_SPIFFS)) return;
      writeManualUpload(request,manualUpdateWriter,manualUpdateEffects,data,len,final);
      resetTaskWdt();
#endif
    });

  server.onNotFound([](AsyncWebServerRequest *r) { webServer.handleNotFound(r); });

  // serveStatic MUST be registered AFTER all route handlers so it doesn't shadow them
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html")
    .setFilter([](AsyncWebServerRequest *r) { return isWebAsset(r->url().c_str()); });

}
