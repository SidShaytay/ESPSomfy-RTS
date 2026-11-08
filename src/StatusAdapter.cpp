#ifdef ESPSOMFY_STATUS_DISPLAY
#include "StatusDisplay.h"
#include "Somfy.h"
#include "ESPNetwork.h"
#include "entryPoints/Web.h"
#include <esp_timer.h>
#include <esp_ota_ops.h>
#include "BoardProfile.h"
#include <Wire.h>
extern SomfyShadeController somfy;
extern ESPNetwork net;
extern Web webServer;
extern ConfigSettings settings;
namespace StatusPanel {
namespace {
BatteryGauge::Monitor gauge;
TwoWire powerBus(1);
bool powerBusReady = false;
bool readPowerWord(uint8_t reg, uint16_t &value) {
  return BatteryGauge::readWord(powerBus, reg, value);
}
}
void pollPower(bool running) {
  if(!running) return;
  static bool attempted = false;
  if(!attempted) {
    attempted = true;
    const auto *partition = esp_ota_get_running_partition();
    esp_app_desc_t app{};
    if(partition && esp_ota_get_partition_description(partition, &app) == ESP_OK) {
      char digest[65];
      for(unsigned i = 0; i < sizeof(app.app_elf_sha256); ++i)
        snprintf(digest + i * 2, 3, "%02x", app.app_elf_sha256[i]);
      Serial.printf("LCD running slot=%s elfSHA256=%s\n", partition->label, digest);
    }
    powerBus.setTimeOut(2);
    powerBusReady = powerBus.begin(BoardProfile::powerSDA, BoardProfile::powerSCL, 100000);
  }
  gauge.tick(millis(), powerBusReady ? readPowerWord : nullptr);
}
StatusDisplay::Snapshot snapshot(bool running) {
  using namespace StatusDisplay;
  Snapshot s;
  s.running = running;
  s.battery = gauge.reading();
  snprintf(s.firmwareVersion, sizeof(s.firmwareVersion), "%s", FW_VERSION);
  snprintf(s.applicationVersion, sizeof(s.applicationVersion), "%s", settings.appVersion.name);
  s.uptimeSeconds = esp_timer_get_time() / 1000000;
  if(WiFi.status() == WL_CONNECTED) {
    s.link = Link::connected;
    if(uint32_t(WiFi.localIP()) != 0) WiFi.localIP().toString().toCharArray(s.address, sizeof(s.address));
  }
  else if(net.softAPOpened) {
    s.link = Link::accessPoint;
    if(uint32_t(WiFi.softAPIP()) != 0) WiFi.softAPIP().toString().toCharArray(s.address, sizeof(s.address));
  }
  else s.link = net.connecting() ? Link::connecting : Link::disconnected;
  const uint8_t state = webServer.listenerState();
  s.web = state == LISTEN ? StatusDisplay::Web::listening : running ? StatusDisplay::Web::error : StatusDisplay::Web::starting;
  bool ready = false;
  // A busy radio is actively owned by TX/reconfiguration: don't retain a stale
  // success. Keep only the frequency; readiness becomes unavailable until sampled.
  static float frequency = 433.42f;
  somfy.transceiver.displayStatus(ready, frequency);
  s.frequency = frequency;
  s.radio = !running ? Radio::initializing : ready ? Radio::ready : Radio::unavailable;
#ifdef ESPSOMFY_LOCAL_UI
  snprintf(s.build, sizeof(s.build), "Plus UI #15 %s", FW_VERSION);
#else
  snprintf(s.build, sizeof(s.build), "Plus LCD #15 %s", FW_VERSION);
#endif
  return s;
}
}
#endif
