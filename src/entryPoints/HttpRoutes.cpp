#include <LittleFS.h>
#if defined(ESPSOMFY_BUNDLED_WEB)
#include "BundledWebRoutes.h"
#endif
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

// HttpRoutes: the HTTP routes on port 80, the web UI's files and the few that
// stay HTTP (docs/adr/0006-two-security-modes-eleven-http-routes.md).
// Everything else is an op on the socket at /ws.

extern ConfigSettings settings;
extern SSDPClass SSDP;

extern SomfyShadeController somfy;
extern Web webServer;
extern MqttConnection mqtt;

static const char *TAG = "Web";

// The token check (requireToken in Web.cpp) runs once the body is in, after
// every chunk was handled, so an upload checks the token on its first chunk
// and drops every chunk of a refused one.
bool uploadAllowed(AsyncWebServerRequest *request, size_t index) {
  if(index == 0) request->setAttribute("uploadAllowed", webServer.isAuthenticated(request));
  return request->getAttribute("uploadAllowed", false);
}
// Every route is an exact match.  The library's default also takes any path
// below the route, and /backup/a.js then passed the token check as a web asset.
void registerHttpRoutes(AsyncWebServer &server) {
  server.on(AsyncURIMatcher::exact("/loginContext"), HTTP_GET, [](AsyncWebServerRequest *request) {
    if(!settings.Security.loadTrusted()) { request->send(503, "application/json", "{\"success\":false,\"msg\":\"Security settings unavailable.\"}");return; }
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

  registerRestoreUploadRoute(server);

  registerFirmwareUploadRoute(server);

  registerApplicationUploadRoute(server);

  server.onNotFound([](AsyncWebServerRequest *r) { webServer.handleNotFound(r); });

  // serveStatic MUST be registered AFTER all route handlers so it doesn't shadow them
#if defined(ESPSOMFY_BUNDLED_WEB)
  if(!registerBundledWebRoutes(server))ESP_LOGE(TAG,"Bundled web handler registration failed");
#else
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html")
    .setFilter([](AsyncWebServerRequest *r) { return isWebAsset(r->url().c_str()); });
#endif

}
