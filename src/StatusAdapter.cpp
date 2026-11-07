#ifdef ESPSOMFY_STATUS_DISPLAY
#include "StatusDisplay.h"
#include "Somfy.h"
#include "ESPNetwork.h"
#include "entryPoints/Web.h"
#include <esp_timer.h>
extern SomfyShadeController somfy;
extern ESPNetwork net;
extern Web webServer;
namespace StatusPanel {
StatusDisplay::Snapshot snapshot(bool running) {
  using namespace StatusDisplay;
  Snapshot s;
  s.running = running;
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
