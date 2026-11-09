#include "ConfigSettings.h"
#include "Transceiver.h"
#include "SettingsTypedValidation.h"
#include "LivePinClaims.h"
#include <cassert>
struct Shade {uint8_t id=255,gpioUp=255,gpioDown=255,gpioMy=255;radio_proto proto=radio_proto::RTS;uint8_t getShadeId()const{return id;}};
struct {struct {bool ok=true;bool usable()const{return ok;}}modelLoad;Shade shades[32];Transceiver transceiver;}somfy;
ConfigSettings settings;
struct ESPNetwork {LivePinClaims liveEthernetPins;uint64_t retainedPinClaims()const{return liveEthernetPins.snapshot();}
#if HAS_ETHERNET
 bool ethStarted=false,_connecting=false,wifiFallback=false;unsigned connectAttempts=0,connectStart=0;conn_types_t connTarget=conn_types_t::unset,connType=conn_types_t::unset;
 bool connectWired();bool connected(){return false;}bool openSoftAP(){++fallbacks;return true;}bool connectWiFi(){return false;}void setConnected(conn_types_t type){connType=type;}static unsigned fallbacks;
#endif
}net;
namespace BoardProfile{bool usesPin(uint8_t){return false;}}
#define PIN_OWNERSHIP_HOST_TEST
#include "PinOwnership.cpp"
#if HAS_ETHERNET
unsigned ESPNetwork::fallbacks=0;
static unsigned driverCalls=0;static bool link=false;
static void driver(){assert((net.retainedPinClaims()&settings.Ethernet.pinClaims())==settings.Ethernet.pinClaims());++driverCalls;}
struct {bool linkUp(){return link;}void setHostname(const char*){driver();}const char *getHostname(){return "fake";}bool begin(eth_phy_type_t,int32_t,int mdc,int mdio,int power,eth_clock_mode_t){assert(mdc>=0 && mdio>=0 && power==-1);driver();return false;}template<class...Args>bool config(Args...){driver();return true;}}ETH;
static constexpr int WL_CONNECTED=3,WIFI_OFF=0,INADDR_NONE=0;
struct {int status(){return 0;}void disconnect(bool){driver();}void mode(int){driver();}}WiFi;
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#include "production-ethernet.inc"
#endif
int main(){
 auto &rf=somfy.transceiver.config;rf.loadStatus={SettingsLoadState::loaded};rf.enabled=true;
 settings.loadStatus={SettingsLoadState::loaded};settings.connType=conn_types_t::wifi;
#if HAS_ETHERNET
 settings.Ethernet.loadStatus={SettingsLoadState::loaded};
#endif
 assert(currentRadioPinConfigurationAvailable(rf));
 const auto oldTx=rf.TXPin;retainGpioPinClaims(LivePinClaims::pinMask(41));rf.TXPin=41;
 assert(!currentRadioPinConfigurationAvailable(rf));rf.TXPin=oldTx;assert(currentRadioPinConfigurationAvailable(rf));
 auto &shade=somfy.shades[31];shade.id=1;shade.proto=radio_proto::GP_Relay;shade.gpioUp=rf.TXPin;
 assert(!currentRadioPinConfigurationAvailable(rf));shade.gpioUp=255;shade.gpioMy=rf.TXPin;assert(currentRadioPinConfigurationAvailable(rf));
 shade.proto=radio_proto::GP_Remote;assert(!currentRadioPinConfigurationAvailable(rf));shade.gpioMy=255;
 net.liveEthernetPins.retain(LivePinClaims::pinMask(rf.TXPin));assert(!currentRadioPinConfigurationAvailable(rf));
 assert(!radioPinConfigurationAvailable(rf,{0,true})); // Caller cannot omit retained physical ETH ownership.
 rf.TXPin=42;assert(currentRadioPinConfigurationAvailable(rf));
 somfy.modelLoad.ok=false;assert(!currentRadioPinConfigurationAvailable(rf));somfy.modelLoad.ok=true;
#if HAS_ETHERNET
 settings.loadStatus={SettingsLoadState::failed};assert(!currentRadioPinConfigurationAvailable(rf));settings.loadStatus={SettingsLoadState::loaded};
 settings.connType=conn_types_t::ethernet;settings.Ethernet.PWRPin=43;settings.Ethernet.MDCPin=44;settings.Ethernet.MDIOPin=45;
 rf.enabled=false;assert(networkPinConfigurationAvailable(settings));
 shade.gpioUp=21;assert(!networkPinConfigurationAvailable(settings));shade.gpioUp=255;
 assert(ethernetActivationPinsAvailable(settings));
 settings.WIFI.loadStatus=settings.IP.loadStatus={SettingsLoadState::loaded};
 settings.Ethernet.PWRPin=41;assert(!networkPinConfigurationAvailable(settings));assert(!net.connectWired()&&!driverCalls);
 settings.Ethernet.PWRPin=-1;assert(!LivePinClaims::contains(settings.Ethernet.pinClaims(),255));
 settings.Ethernet.MDCPin=-1;assert(!networkPinConfigurationAvailable(settings));settings.Ethernet.MDCPin=44;
 settings.WIFI.loadStatus=settings.IP.loadStatus={SettingsLoadState::loaded};
 somfy.modelLoad.ok=false;assert(!net.connectWired());assert(!driverCalls&&ESPNetwork::fallbacks==2);
 somfy.modelLoad.ok=true;assert(!net.connectWired());assert(driverCalls && !net.ethStarted);const auto claimed=net.retainedPinClaims();assert((claimed&settings.Ethernet.pinClaims())==settings.Ethernet.pinClaims());
 link=true;somfy.modelLoad.ok=false;const auto calls=driverCalls;assert(net.connectWired());assert(driverCalls==calls && net.retainedPinClaims()==claimed);
 settings.connType=conn_types_t::wifi;assert(!ethernetActivationPinsAvailable(settings));
#endif
 return 0;
}
