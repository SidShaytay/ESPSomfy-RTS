#include "ApplicationUploadResult.h"
#include "ModelCommitService.h"
#include "MqttConnection.h"
#include "Somfy.h"
#include "TaskWdt.h"
#include "Web.h"
#include "WebRoutes.h"
#include "esp_log.h"
#include <Update.h>
extern SomfyShadeController somfy;
extern Web webServer;
extern MqttConnection mqtt;
static const char *TAG = "ApplicationUpload";
namespace {
void queueApplicationSave(AsyncWebServerRequest *request,
                          ModelCommitMode mode) {
  if (!request->getAttribute("applicationStarted", false) ||
      request->getAttribute("applicationCommitAttempted", false))
    return;
  request->setAttribute("applicationCommitAttempted", true);
  request->setAttribute("applicationCommitQueued", submitModelCommit({}, mode));
}
} // namespace
void registerApplicationUploadRoute(AsyncWebServer &server) {
  // Browser-owned application image upload. HTTP reports upload/admission;
  // only loop() may save the stable model and authorize the requested restart.
  server.on(
      AsyncURIMatcher::exact("/updateApplication"), HTTP_POST,
      [](AsyncWebServerRequest *request) {
        if (!request->getAttribute("applicationStarted", false)) {
          request->send(
              503, _encoding_json,
              "{\"status\":\"ERROR\",\"started\":false,\"desc\":\"Application "
              "upload did not start. Retry after any active transfer finishes. "
              "No model save or restart was requested.\"}");
          return;
        }
        const auto response = applicationUploadResponse(
            request->getAttribute("applicationUploaded", false),
            request->getAttribute("applicationCommitQueued", false));
        request->send(response.status, _encoding_json, response.json);
      },
      [](AsyncWebServerRequest *request, String, size_t index, uint8_t *data,
         size_t len, bool final) {
        if (!uploadAllowed(request, index))
          return;
        // These flags are request-owned and sticky. A failed request cannot
        // acquire the shared SDK object again through another multipart chunk.
        if (request->getAttribute("applicationFailed", false))
          return;
        if (index == 0) {
          if (request->getAttribute("applicationAttempted", false))
            return;
          request->setAttribute("applicationAttempted", true);
          request->setAttribute("applicationUploaded", false);
          request->setAttribute("applicationCommitQueued", false);
          request->setAttribute("applicationCommitAttempted", false);
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
            request->setAttribute("applicationFailed", true);
            ESP_LOGE(TAG, "Application upload did not acquire Update");
            return;
          }
          request->setAttribute("applicationStarted", true);
          request->setAttribute("applicationOwnsUpdate", true);
          webServer.uploadSuccess = false;
          somfy.transceiver.end();
          mqtt.end();
          // The pinned server releases the request after this callback. Only
          // our still-active transfer may abort; failure/final paths retire it.
          request->onDisconnect([request]() {
            if (!request->getAttribute("applicationOwnsUpdate", false) ||
                request->getAttribute("applicationFailed", false))
              return;
            request->setAttribute("applicationFailed", true);
            request->setAttribute("applicationOwnsUpdate", false);
            Update.abort();
            queueApplicationSave(request, ModelCommitMode::applicationRecovery);
            ESP_LOGW(TAG, "Application upload abandoned; no automatic restart");
          });
        }
        if (!request->getAttribute("applicationOwnsUpdate", false))
          return;
        if (len && Update.write(data, len) != len) {
          request->setAttribute("applicationFailed", true);
          request->setAttribute("applicationOwnsUpdate", false);
          Update.abort();
          queueApplicationSave(request, ModelCommitMode::applicationRecovery);
          ESP_LOGE(TAG, "Application upload write failed; request retired");
          resetTaskWdt();
          return;
        }
        if (final) {
          const bool uploaded = Update.end(true) && !Update.hasError();
          request->setAttribute("applicationOwnsUpdate", false);
          request->setAttribute("applicationFailed", !uploaded);
          request->setAttribute("applicationUploaded", uploaded);
          webServer.uploadSuccess = uploaded;
          queueApplicationSave(
              request, uploaded ? ModelCommitMode::applicationRecoveryAndRestart
                                : ModelCommitMode::applicationRecovery);
          ESP_LOGI(TAG, "Application upload %s; model save %s",
                   uploaded ? "complete" : "failed",
                   request->getAttribute("applicationCommitQueued", false)
                       ? "queued"
                       : "not queued");
        }
        resetTaskWdt();
      });
}
