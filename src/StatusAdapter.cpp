#ifdef ESPSOMFY_STATUS_DISPLAY
#include "StatusDisplay.h"
#include "LocalSystem.h"
#include "Somfy.h"
#include "ESPNetwork.h"
#if HAS_ETHERNET
#include <ETH.h>
#endif
#include "entryPoints/Web.h"
#include <esp_timer.h>
#include <esp_ota_ops.h>
#include "BoardProfile.h"
#include "StatusBuild.h"
#include "Utils.h"
#include <esp_heap_caps.h>
#include <Wire.h>
extern SomfyShadeController somfy;
extern ESPNetwork net;
extern Web webServer;
extern ConfigSettings settings;
extern LoopStats loopStats;
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
  s.health.sampled=true;
  constexpr uint32_t internalCaps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
  constexpr uint32_t psramCaps=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
  s.health.internalFreeBytes=heap_caps_get_free_size(internalCaps);
  s.health.internalMinimumBytes=heap_caps_get_minimum_free_size(internalCaps);
  s.health.internalLargestBytes=heap_caps_get_largest_free_block(internalCaps);
  s.health.psramPresent=heap_caps_get_total_size(psramCaps)!=0;
  if(s.health.psramPresent) s.health.psramFreeBytes=heap_caps_get_free_size(psramCaps);
  s.health.loopSampled=loopStats.hasPublished();
  if(s.health.loopSampled) {
    s.health.loopRate=loopStats.publishedRate();
    s.health.loopMaxUs=loopStats.publishedMaxUs();
  }
  s.battery = gauge.reading();
  snprintf(s.firmwareVersion, sizeof(s.firmwareVersion), "%s", FW_VERSION);
  snprintf(s.applicationVersion, sizeof(s.applicationVersion), "%s", settings.appVersion.name);
  s.uptimeSeconds = esp_timer_get_time() / 1000000;
#if HAS_ETHERNET
  if(net.connType==conn_types_t::ethernet&&net.connected()){
    s.link=Link::connected;
    if(uint32_t(ETH.localIP())!=0)ETH.localIP().toString().toCharArray(s.address,sizeof(s.address));
  }else
#endif
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
  buildIdentity(s.build, sizeof(s.build), FW_VERSION);
  return s;
}
}
#ifdef ESPSOMFY_LOCAL_UI
namespace LocalUi {
bool readLocalNetworkRow(uint8_t index,EntityAdmin::FormRow &out){
  out={};if(index>=networkDetailRows)return false;
  static const char *labels[]={"Connection","WiFi SSID","WiFi channel","WiFi signal","IP address","Ethernet link","Ethernet speed","Ethernet duplex","Back"};
  snprintf(out.label,sizeof(out.label),"%s",labels[index]);
  const bool wifi=WiFi.status()==WL_CONNECTED;
#if HAS_ETHERNET
  const bool ethernet=net.connType==conn_types_t::ethernet&&net.connected();
#else
  const bool ethernet=false;
#endif
  switch(index){
    case 0:snprintf(out.value,sizeof(out.value),"%s",ethernet?"Ethernet connected":wifi?"WiFi connected":net.softAPOpened?"WiFi access point":net.connected()?net.connTypeName():net.connecting()?"Connecting":"Disconnected");break;
    case 1:if(wifi)snprintf(out.value,sizeof(out.value),"%s",WiFi.SSID().c_str());else strcpy(out.value,"Unavailable");break;
    case 2:if(wifi&&net.channel>0)snprintf(out.value,sizeof(out.value),"%d",net.channel);else strcpy(out.value,"Unavailable");break;
    case 3:if(wifi)snprintf(out.value,sizeof(out.value),"%ld dBm",static_cast<long>(WiFi.RSSI()));else strcpy(out.value,"Unavailable");break;
    case 4:
#if HAS_ETHERNET
      if(ethernet&&uint32_t(ETH.localIP())!=0)ETH.localIP().toString().toCharArray(out.value,sizeof(out.value));
      else
#endif
      if(wifi&&uint32_t(WiFi.localIP())!=0)WiFi.localIP().toString().toCharArray(out.value,sizeof(out.value));
      else if(net.softAPOpened&&uint32_t(WiFi.softAPIP())!=0)WiFi.softAPIP().toString().toCharArray(out.value,sizeof(out.value));
      else strcpy(out.value,"Unavailable");
      break;
    case 5:case 6:case 7:
#if HAS_ETHERNET
      if(net.connType==conn_types_t::ethernet&&net.connected()){
        if(index==5)strcpy(out.value,"Connected");else if(index==6)snprintf(out.value,sizeof(out.value),"%u Mbps",unsigned(ETH.linkSpeed()));else strcpy(out.value,ETH.fullDuplex()?"Full":"Half");
      }else strcpy(out.value,index==5?"Down / not selected":"Unavailable");
#else
      strcpy(out.value,"Not supported on this build");
#endif
      break;
    default:break;
  }
  return true;
}
}
#endif
#endif
