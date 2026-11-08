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
void registerApplicationUploadRoute(AsyncWebServer &server) {
  // Browser-owned application image upload. HTTP reports upload/admission;
  // only loop() may save the stable model and authorize the requested restart.
  server.on(
      AsyncURIMatcher::exact("/updateApplication"), HTTP_POST,
      [](AsyncWebServerRequest *request) {
        const auto response = applicationUploadResponse(
            request->getAttribute("applicationUploaded", false),
            request->getAttribute("applicationCommitQueued", false));
        request->send(response.status, _encoding_json, response.json);
      },
      [](AsyncWebServerRequest *request, String, size_t index, uint8_t *data,
         size_t len, bool final) {
        if (!uploadAllowed(request, index))
          return;
        if (index == 0) {
          webServer.uploadSuccess = false;
          request->setAttribute("applicationUploaded", false);
          request->setAttribute("applicationCommitQueued", false);
          request->setAttribute("applicationCommitAttempted", false);
          ESP_LOGI(TAG, "Application image upload started");
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS))
            ESP_LOGE(TAG, "Update begin failed");
          else {
            somfy.transceiver.end();
            mqtt.end();
            // The request and its attributes still exist during this callback
            // (the pinned server releases its self-reference after
            // onDisconnect).
            request->onDisconnect([request]() {
              if (request->getAttribute("applicationCommitAttempted", false))
                return;
              request->setAttribute("applicationCommitAttempted", true);
              if (Update.isRunning())
                Update.abort();
              const bool queued =
                  submitModelCommit({}, ModelCommitMode::applicationRecovery);
              ESP_LOGW(TAG,
                       "Application upload abandoned; model save %s; no "
                       "automatic restart",
                       queued ? "queued" : "not queued");
            });
          }
        }
        if (len && Update.write(data, len) != len) {
          ESP_LOGE(TAG, "Application upload write failed");
          Update.abort();
        }
        if (final) {
          const bool uploaded = Update.end(true) && !Update.hasError();
          webServer.uploadSuccess = uploaded;
          request->setAttribute("applicationUploaded", uploaded);
          request->setAttribute("applicationCommitAttempted", true);
          const bool queued = submitModelCommit(
              {}, uploaded ? ModelCommitMode::applicationRecoveryAndRestart
                           : ModelCommitMode::applicationRecovery);
          request->setAttribute("applicationCommitQueued", queued);
          ESP_LOGI(TAG, "Application upload %s; model save %s",
                   uploaded ? "complete" : "failed",
                   queued ? "queued" : "not queued");
        }
        resetTaskWdt();
      });
}
