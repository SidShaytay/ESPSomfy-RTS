#include "ESPAsyncWebServer.h"
#include "ModelCommitService.h"
#include "Update.h"
#include <iostream>
#include <vector>
#define SOMFY_H
#define webserver_h
#define MQTT_CONNECTION_H
#define TASK_WDT_H
struct SomfyShadeController {
  struct Radio {
    unsigned stops = 0;
    void end() { ++stops; }
  } transceiver;
} somfy;
struct Web {
  bool uploadSuccess = false;
} webServer;
struct MqttConnection {
  unsigned stops = 0;
  void end() { ++stops; }
} mqtt;
FakeUpdate Update;
static bool admitted = true;
static std::vector<bool> restarts;
static unsigned resets = 0;
static void resetTaskWdt() { ++resets; }
const char _encoding_json[] = "application/json";
bool uploadAllowed(AsyncWebServerRequest *request, size_t) {
  return request->allowed;
}
bool submitModelCommit(const proto_reply_to_t &, ModelCommitMode mode) {
  assert(mode != ModelCommitMode::save);
  restarts.push_back(mode == ModelCommitMode::applicationRecoveryAndRestart);
  return admitted;
}
#include "../../src/entryPoints/ApplicationUpload.cpp"
static void reset() {
  Update = {};
  somfy = {};
  mqtt = {};
  webServer = {};
  admitted = true;
  restarts.clear();
  resets = 0;
}
int main() {
  AsyncWebServer server;
  registerApplicationUploadRoute(server);
  uint8_t byte = 42;
  reset();
  AsyncWebServerRequest denied;
  denied.allowed = false;
  server.upload(&denied, String("x.bin"), 0, &byte, 1, true);
  server.complete(&denied);
  assert(Update.begins == 0 && restarts.empty() && denied.status == 503);
  reset();
  AsyncWebServerRequest success;
  server.upload(&success, String("x.bin"), 0, &byte, 1, true);
  server.complete(&success);
  assert(success.status == 202 && restarts == std::vector<bool>{true});
  assert(success.body.find("queued") != std::string::npos &&
         success.body.find("SUCCESS") == std::string::npos);
  success.disconnected();
  assert(restarts.size() == 1);
  reset();
  admitted = false;
  AsyncWebServerRequest full;
  server.upload(&full, String("x.bin"), 0, &byte, 1, true);
  server.complete(&full);
  assert(full.status == 503 && restarts == std::vector<bool>{true});
  full.disconnected();
  assert(restarts.size() == 1);
  reset();
  Update.beginOk = false;
  Update.running = true;
  webServer.uploadSuccess = true;
  AsyncWebServerRequest beginFailed;
  server.upload(&beginFailed, String("x.bin"), 0, &byte, 1, true);
  server.complete(&beginFailed);
  assert(beginFailed.status == 503 && restarts.empty());
  assert(Update.begins == 1 && Update.writes == 0 && Update.ends == 0 &&
         Update.aborts == 0);
  assert(Update.running && webServer.uploadSuccess &&
         !beginFailed.disconnected);
  assert(somfy.transceiver.stops == 0 && mqtt.stops == 0);
  Update.beginOk = true;
  server.upload(&beginFailed, String("x.bin"), 1, &byte, 1, true);
  server.upload(&beginFailed, String("x.bin"), 0, &byte, 1, true);
  assert(Update.begins == 1 && Update.writes == 0 && Update.ends == 0 &&
         Update.aborts == 0 && restarts.empty());
  reset();
  Update.endOk = false;
  AsyncWebServerRequest endFailed;
  server.upload(&endFailed, String("x.bin"), 0, &byte, 1, true);
  server.complete(&endFailed);
  assert(endFailed.status == 500 && restarts == std::vector<bool>{false});
  const auto failedWrites = Update.writes;
  Update.running = true;
  Update.error = false;
  server.upload(&endFailed, String("x.bin"), 1, &byte, 1, true);
  endFailed.disconnected();
  assert(Update.writes == failedWrites && Update.ends == 1 &&
         Update.aborts == 0 && Update.running);
  assert(restarts == std::vector<bool>{false});
  reset();
  Update.shortWrite = true;
  Update.endOk = false;
  AsyncWebServerRequest failed;
  server.upload(&failed, String("x.bin"), 0, &byte, 1, true);
  server.complete(&failed);
  assert(failed.status == 500 && restarts == std::vector<bool>{false});
  failed.disconnected();
  assert(restarts.size() == 1 && Update.aborts == 1);
  assert(Update.writes == 1 && Update.ends == 0);
  // A foreign transfer can own Update after our abort. Remaining chunks,
  // including a repeated index-zero callback, must not touch that transfer.
  Update.running = true;
  Update.error = false;
  Update.shortWrite = false;
  webServer.uploadSuccess = true;
  server.upload(&failed, String("x.bin"), 1, &byte, 1, false);
  server.upload(&failed, String("x.bin"), 2, &byte, 1, true);
  server.upload(&failed, String("x.bin"), 0, &byte, 1, true);
  failed.disconnected();
  assert(Update.begins == 1 && Update.writes == 1 && Update.ends == 0 &&
         Update.aborts == 1);
  assert(Update.running && webServer.uploadSuccess &&
         restarts == std::vector<bool>{false});
  reset();
  Update.error = true;
  AsyncWebServerRequest inconsistent;
  server.upload(&inconsistent, String("x.bin"), 0, &byte, 1, true);
  server.complete(&inconsistent);
  assert(inconsistent.status == 500 && restarts == std::vector<bool>{false});
  reset();
  AsyncWebServerRequest abandoned;
  server.upload(&abandoned, String("x.bin"), 0, &byte, 1, false);
  assert(restarts.empty());
  abandoned.disconnected();
  assert(Update.aborts == 1 && restarts == std::vector<bool>{false});
  abandoned.disconnected();
  assert(restarts.size() == 1);
  std::cout << "Application upload actual callbacks: auth denial, "
               "verified-restart admission, queue-full, write/end errors and "
               "disconnect policy passed.\n";
}
