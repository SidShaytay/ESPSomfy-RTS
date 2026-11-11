// Run the production event callback, connection setup and loop with inert I/O.
// Discovery calls in an event callback fail even when nested inside helpers.
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdint>
using String=std::string;
enum class conn_types_t {unset,wifi,ethernet,ap};
enum WiFiEvent_t {ARDUINO_EVENT_WIFI_READY,ARDUINO_EVENT_WIFI_SCAN_DONE,ARDUINO_EVENT_WIFI_STA_START,
 ARDUINO_EVENT_WIFI_STA_STOP,ARDUINO_EVENT_WIFI_STA_CONNECTED,ARDUINO_EVENT_WIFI_STA_DISCONNECTED,
 ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE,ARDUINO_EVENT_WIFI_STA_GOT_IP,ARDUINO_EVENT_WIFI_STA_LOST_IP,
 ARDUINO_EVENT_ETH_START,ARDUINO_EVENT_ETH_STOP,ARDUINO_EVENT_ETH_CONNECTED,ARDUINO_EVENT_ETH_DISCONNECTED,
 ARDUINO_EVENT_ETH_GOT_IP,ARDUINO_EVENT_WIFI_AP_START,ARDUINO_EVENT_WIFI_AP_STOP};
constexpr int WL_CONNECTED=3,WIFI_STA=1,WIFI_OFF=0,WIFI_SCAN_RUNNING=-1,ESP_OK=0;
constexpr unsigned STATUS_EMIT_MS=30000,SSID_SCAN_INTERVAL=600000,CONNECT_TIMEOUT=20000;
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
static bool inEvent=false;
static unsigned now=100;
unsigned millis(){return now;}
void resetTaskWdt(){}
struct Address {String toString(){return "192.0.2.1";}};
struct Wifi {
 int state=WL_CONNECTED;int status(){return state;} int scanComplete(){return -2;} int scanNetworks(bool,bool,bool,int,int,const char*){return -1;}
 void scanDelete(){} const uint8_t *BSSID(){return nullptr;}
 void setHostname(const char*){} void softAPdisconnect(bool){} void mode(int){} void disconnect(bool){}
 int softAPgetStationNum(){return 0;} String SSID(){return "test";} String BSSIDstr(){return "test";}
 int RSSI(){return -60;} int channel(){return 1;}
 Address localIP(){return {};} Address subnetMask(){return {};} Address gatewayIP(){return {};} Address dnsIP(int){return {};}
 Address softAPIP(){return {};}
} WiFi;
struct Ethernet:Wifi {bool up=true;bool linkUp(){return up;}bool fullDuplex(){return true;}int linkSpeed(){return 100;}} ETH;
struct wifi_ap_record_t {int authmode=0;};
int esp_wifi_sta_get_ap_info(wifi_ap_record_t*){return ESP_OK;}
struct Settings {
 bool usable=true,ssdpBroadcast=true;
 char hostname[16]="test",chipModel[16]="test",serverId[16]="test";
 struct {bool dhcp=true;Address ip,subnet,gateway,dns1,dns2;} IP;
 struct {bool hidden=true,roaming=false;char ssid[16]="test";} WIFI;
 struct {const char *name="test";} fwVersion;
 bool networkUsable(){return usable;}void printAvailHeap(){}
} settings;
struct Discovery {
 bool isStarted=false;
 int starts=0,stops=0,configures=0;
 void touch(){assert(!inEvent && "discovery must run on loop, never arduino_events");}
 void setHTTPPort(int){touch();++configures;}
 template<class... A>void setSchemaURL(A...){touch();}
 template<class... A>void setChipId(A...){touch();}
 template<class... A>void setDeviceType(A...){touch();}
 template<class... A>void setName(A...){touch();}
 template<class... A>void setModelName(A...){touch();}
 template<class... A>void setModelNumber(A...){touch();}
 template<class... A>void setModelURL(A...){touch();}
 template<class... A>void setManufacturer(A...){touch();}
 template<class... A>void setManufacturerURL(A...){touch();}
 template<class... A>void setURL(A...){touch();}
 template<class... A>void setActive(A...){touch();}
 bool begin(){touch();++starts;isStarted=true;return true;}
 void end(){touch();++stops;isStarted=false;}
 void loop(){touch();}
} SSDP;
struct Mdns {
 int starts=0;
 bool begin(const char*){assert(!inEvent);++starts;return true;}
 template<class... A>void addService(A...){assert(!inEvent);}
 template<class... A>void addServiceTxt(A...){assert(!inEvent);}
} MDNS;
struct {void loop(){}} mqtt;
struct {bool pending(){return false;}bool active(){return false;}} clientScanner;
static bool _apScanning=false;
static int connectRetries=0;
class ESPNetwork {
public:
 std::atomic<bool> discoveryPending{false};
 conn_types_t connType=conn_types_t::unset;
 bool _connecting=true,softAPOpened=false,openingSoftAP=false,ethStarted=false,wifiFallback=false;
 unsigned connectStart=0,connectTime=0,disconnectTime=0,lastWifiScan=0,lastEmit=0;
 int connectAttempts=0,strength=0,channel=0;String ssid,mac;
 void setConnected(conn_types_t);static void networkEvent(WiFiEvent_t);void loop();void discoveryLoop();
 bool connected();bool connecting();void clearConnecting();
 conn_types_t preferredConnType(){return conn_types_t::wifi;}
 void connect(conn_types_t){}void openSoftAP(){}void emitStatus(){}
 bool getStrongestAP(const char*,uint8_t*,int32_t*){return false;}
 void connectWiFi(const uint8_t*,int32_t){}void changeAP(const uint8_t*,int32_t){}
 uint32_t getChipId(){return 1;}
} net;
void clientScanLoop(ESPNetwork&){}
#include "production-network.inc"
static void event(WiFiEvent_t e){inEvent=true;ESPNetwork::networkEvent(e);inEvent=false;}
int main(){
 event(ARDUINO_EVENT_WIFI_STA_GOT_IP);
 assert(net.connected() && net.connectAttempts==1);
 assert(SSDP.starts==0 && MDNS.starts==0);
 net.loop();assert(SSDP.starts==1 && MDNS.starts==1 && SSDP.configures==1);
 net.loop();assert(SSDP.starts==1 && MDNS.starts==1);
 // A disconnect before the loop consumes a notification must not start services.
 event(ARDUINO_EVENT_WIFI_STA_GOT_IP);event(ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
 net.loop();assert(MDNS.starts==1);
 event(ARDUINO_EVENT_WIFI_STA_GOT_IP);net.loop();assert(MDNS.starts==2 && SSDP.starts==2);
 settings.ssdpBroadcast=false;
 event(ARDUINO_EVENT_WIFI_STA_GOT_IP);net.loop();assert(MDNS.starts==3 && !SSDP.isStarted);
 settings.ssdpBroadcast=true;net.loop();assert(SSDP.isStarted && MDNS.starts==3);
#if HAS_ETHERNET
 event(ARDUINO_EVENT_ETH_GOT_IP);assert(net.connType==conn_types_t::ethernet);
 net.loop();assert(MDNS.starts==4 && SSDP.starts==4);
#endif
 // The link can be down before the event callback catches up.
 int before=MDNS.starts;
 event(ARDUINO_EVENT_WIFI_STA_GOT_IP);WiFi.state=0;
 net.loop();assert(MDNS.starts==before);
 WiFi.state=WL_CONNECTED;net.loop();assert(MDNS.starts==before+1);
#if HAS_ETHERNET
 before=MDNS.starts;event(ARDUINO_EVENT_ETH_GOT_IP);ETH.up=false;
 net.loop();assert(MDNS.starts==before);
 ETH.up=true;net.loop();assert(MDNS.starts==before+1);
#endif
 before=MDNS.starts;
 event(ARDUINO_EVENT_WIFI_STA_GOT_IP);event(ARDUINO_EVENT_WIFI_STA_GOT_IP);
 settings.usable=false;net.loop();assert(MDNS.starts==before);
 settings.usable=true;net.loop();assert(MDNS.starts==before+1);
 net.loop();assert(MDNS.starts==before+1);
 puts("Network discovery task-boundary regressions passed");
}
