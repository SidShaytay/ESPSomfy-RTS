#include "ConfigSettings.h"
#include "Transceiver.h"
#include "RadioInitMarker.h"
#include "SettingsTypedValidation.h"
#include "SettingsDurability.h"
#include "SettingsRevisions.h"
#include "ProtoSettings.h"
#include "GitUpdate.h"
#include "ManualUpdateLifecycle.h"
#include <esp_timer.h>
#include <nvs.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <mutex>
static unsigned hardware=0,enabledReceive=0,disabledReceive=0,timeApplies=0;
static bool detected=true,boardReadback=true,afterEffect=false;
struct {Transceiver transceiver;} somfy;
ConfigSettings settings;
static uint8_t marker(){auto i=FakeNvs::values.find("CC1101/radioInit");return i==FakeNvs::values.end()?1:i->second.bytes[0];}
static void hardwareCall(){assert(marker()==0 && !somfy.transceiver.available());++hardware;}
namespace BoardProfile {bool usesPin(uint8_t){return false;}bool prepareRadio(float){hardwareCall();return true;} }
struct {void begin(int,int,int,int){hardwareCall();}} SPI;
struct {
#define CONFIG(name) template<class...Args> void name(Args...){hardwareCall();}
 CONFIG(setGDO0) CONFIG(setGDO) CONFIG(setSpiPin) CONFIG(Init) CONFIG(setCCMode) CONFIG(setMHZ)
 CONFIG(setRxBW) CONFIG(setDeviation) CONFIG(setPA) CONFIG(setModulation) CONFIG(setManchester)
 CONFIG(setPktFormat) CONFIG(setDcFilterOff) CONFIG(setCrc) CONFIG(setCRC_AF) CONFIG(setSyncMode) CONFIG(setAdrChk)
#undef CONFIG
 bool getCC1101(){hardwareCall();return detected;}
 void setSidle(){++hardware;}
} ELECHOUSE_cc1101;
struct {template<class...Args>void printf(const char *,Args...){}}Serial;
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
static constexpr unsigned portMAX_DELAY=0;
struct RadioLock {bool locked=true;RadioLock(unsigned,bool){}};
static GitUpdateOwnerToken radioOwner;
static RadioInstallToken radioPause{17};
static constexpr uint8_t INPUT=0;
static void pinMode(uint8_t,uint8_t){++hardware;}
bool Transceiver::installPauseHeld(const RadioInstallToken &token)const{return token.generation==17;}
settings_save_result_t saveRadioConfiguration(Transceiver &radio,const transceiver_config_t &next){
 return radio.setConfigWhilePaused(next,radioPause,radioOwner,currentRadioPinContext());
}
uint64_t retainedEthernetPinClaims(){return 0;}
RadioPinContext currentRadioPinContext(){return {0,true};}
RadioPinContext withRetainedEthernetClaims(RadioPinContext context){return context;}
bool radioPinConfigurationAvailable(const transceiver_config_t &,RadioPinContext context){return context.qualified;}
static unsigned rxmode=0;static bool scanEmitPending=false;uint8_t radioBitLength=56;
static void warnBootPin(const char *,uint8_t){}
static bool checkBoardRadio(float){hardwareCall();return boardReadback;}
void Transceiver::disableReceive(){++disabledReceive;}
void Transceiver::enableReceive(){assert(config.radioInit && marker()==1 && available());++enabledReceive;}
void Transceiver::resumeTx(uint8_t){}
void transceiver_config_t::applyChipDefaults(){applyBoardPins();} // Read-only chip metadata boundary.
bool ConfigSettings::getAppVersion(){return true;}
void ConfigSettings::print(){}
bool NTPSettings::apply(){
 assert(somfy.transceiver.config.loadStatus.state!=SettingsLoadState::unknown);
 assert(!FakeNvs::writeOpens);++timeApplies;afterEffect=true;return true;
}
static constexpr int WIFI_ALL_CHANNEL_SCAN=0,WIFI_CONNECT_AP_BY_SIGNAL=0,WL_CONNECTED=3,WIFI_STA=1;
static unsigned apCalls=0,staModes=0,hostnames=0,connectCalls=0;
static char apName[65]={};
struct {
 bool connected=false;
 void setScanMethod(int){}void setSortMethod(int){}void persistent(bool v){assert(!v);}void setAutoReconnect(bool){}
 template<class T>void onEvent(T){}
 int status(){return connected?WL_CONNECTED:0;}void disconnect(bool,bool erase=false){assert(!erase);connected=false;}
 void setHostname(const char*){++hostnames;}void mode(int){++staModes;}
 void softAP(const char *name,const char *password){assert(!password[0]);strlcpy(apName,name,sizeof(apName));++apCalls;}
} WiFi;
struct ESPNetwork {
 bool softAPOpened=false,openingSoftAP=false;unsigned long disconnectTime=0;
 static void networkEvent(int){}bool setup();bool openSoftAP();void loopAdmission();
 bool connected(){return false;}conn_types_t preferredConnType(){return conn_types_t::wifi;}
 bool connect(conn_types_t){++connectCalls;return true;}
};
static void clientScanLoop(ESPNetwork &){}
static void resetTaskWdt(){}static void delay(unsigned){}
#include "production-settings-startup.inc"
static void seedFlag(const char *key,uint8_t value){nvs_handle_t h;nvs_open("CC1101",NVS_READWRITE,&h);nvs_set_u8(h,key,value);nvs_close(h);}
static void seedText(const char *space,const char *key,const char *value){nvs_handle_t h;nvs_open(space,NVS_READWRITE,&h);nvs_set_str(h,key,value);nvs_close(h);}
static void counters(){FakeNvs::opens=FakeNvs::reads=FakeNvs::sets=FakeNvs::commits=FakeNvs::readOnlyOpens=FakeNvs::writeOpens=0;}
int main(int argc,char **argv){assert(argc==2);const std::string mode=argv[1];
 if(mode=="networkFailed" || mode=="networkKnown") {
  if(mode=="networkFailed")FakeNvs::failRead=1;
  settings.begin();ESPNetwork network;
  if(mode=="networkFailed")WiFi.connected=true;
  assert(network.setup());network.loopAdmission();
  if(mode=="networkFailed")assert(apCalls==1 && !staModes && !hostnames && !connectCalls && !strcmp(apName,"ESPSomfy RTS"));
  else assert(!apCalls && staModes==1 && hostnames==1 && connectCalls==1);
  assert(!FakeNvs::writeOpens && !FakeNvs::sets && !FakeNvs::erases && !hardware);
  puts("Actual network startup/loop use existing fallback transport without applying unknown settings or writing NVS");return 0;
 }
 if(mode=="startup" || mode=="startupFailed"){
  seedText("WIFI","hostname","legacy");counters();if(mode=="startupFailed")FakeNvs::failRead=1;
  FakeNvs::beforeGet=[]{if(afterEffect)assert(FakeNvs::lastOpenMode==NVS_READWRITE);};
  startupReads();assert(timeApplies==1 && !hardware && !enabledReceive);
  if(mode=="startup")assert(FakeNvs::sets && !settings.legacyMigrationPending);
  else assert(!FakeNvs::writeOpens && !settings.loadStatus.readable());
  const auto sets=FakeNvs::sets;settings.applyStartup(somfy.transceiver.config);assert(timeApplies==1 && sets==FakeNvs::sets);
  puts("Actual setup order stages all namespaces before time/migration effects and never replays them");return 0;
 }
 auto &radio=somfy.transceiver.config;radio.enabled=true;
 if(mode=="scanApply") {
  assert(somfy.transceiver.loadReadOnly());rxmode=3;const auto claims=somfy.transceiver.retainedPinClaims();
  const auto result=radio.apply();assert(!result.saved()&&!hardware&&!FakeNvs::sets&&!disabledReceive&&rxmode==3&&somfy.transceiver.retainedPinClaims()==claims);return 0;
 }
 if(mode=="hotDisable") {
  assert(somfy.transceiver.loadReadOnly());detected=false;radio.apply();assert(hardware && somfy.transceiver.retainedPinClaims());const auto claims=somfy.transceiver.retainedPinClaims();const auto calls=hardware;
  radio.enabled=false;radio.apply();assert(hardware==calls+1 && somfy.transceiver.retainedPinClaims()==claims && !enabledReceive);return 0;
 }
 if(mode=="disabledCold") {
  radio.enabled=false;assert(somfy.transceiver.loadReadOnly());radio.radioInit=true;radio.apply();
  assert(!hardware && !enabledReceive && !somfy.transceiver.available());
  puts("Disabled cold radio never touches CC1101/SPI/GPIO");return 0;
 }
 if(mode=="liveFirst" || mode=="liveLast" || mode=="liveOpen" || mode=="stalePause" || mode=="foreignOwner" || mode=="wrongKind" || mode=="ownedRestore" || mode=="liveExpiredEntry" || mode=="liveExpiredAfterSave") {
  assert(somfy.transceiver.loadReadOnly());radio.enabled=true;
  const bool restoring=mode=="ownedRestore" || mode=="liveExpiredEntry" || mode=="liveExpiredAfterSave";
  assert(gitUpdate.acquireExternal(mode=="wrongKind"?GitUpdateExternalOwner::counter:restoring?GitUpdateExternalOwner::restore:GitUpdateExternalOwner::configuration,radioOwner));
  if(restoring) {
   assert(gitUpdate.beginTransfer(radioOwner,ManualUpdateKind::restore,0));assert(gitUpdate.claimTransfer(radioOwner,0));
   assert(gitUpdate.transferResource(radioOwner,true));assert(gitUpdate.queueTransfer(radioOwner));assert(gitUpdate.finishTransfer(radioOwner,0,true));assert(gitUpdate.claimTransfer(radioOwner,0,true));
  }
  counters();const auto revision=deviceSettingsRevision(settings_section_t::radio);
  // The full namespace has 15 fields, followed by the before/after marker.
  if(mode=="liveFirst")FakeNvs::failSet=16;
  if(mode=="liveLast")FakeNvs::failSet=17;
  if(mode=="liveOpen")FakeNvs::failOpen=2;
  if(mode=="stalePause")radioPause.generation=18;
  if(mode=="foreignOwner")++radioOwner.generation;
  if(mode=="liveExpiredEntry")stubEspTimerMicros=30000000;
  if(mode=="liveExpiredAfterSave")FakeNvs::beforeGet=[] {stubEspTimerMicros=30000000;};
  JsonDocument request;request["frequency"]=433.5;
  const auto result=runtimeSaveRadio(request.as<JsonObject>());
  if(mode=="ownedRestore") {
   assert(result.saved() && result.liveApplied && radio.frequency==433.5f && enabledReceive==1 && gitUpdate.ownsExternal(radioOwner));
   puts("Exact already-owned restore path applies without nested acquisition");return 0;
  }
  if(mode=="liveExpiredEntry" || mode=="liveExpiredAfterSave") {
   assert(!result.saved() && !result.liveApplied && !hardware && !enabledReceive && radio.frequency!=433.5f && deviceSettingsRevision(settings_section_t::radio)==revision);
   if(mode=="liveExpiredEntry")assert(!FakeNvs::sets && !result.radioNamespaceVerified);
   else assert(result.state==settings_save_state_t::partial && result.radioNamespaceVerified && FakeNvs::sets==15 && !settingsDurability().status().restartAllowed());
   puts("Real claimed restore expiry blocks first radio write or post-write live adoption without replay");return 0;
  }
  const bool stale=mode=="stalePause" || mode=="foreignOwner" || mode=="wrongKind";
  assert(!result.saved());
  if(stale)assert(!result.liveApplied && !FakeNvs::sets && !hardware && deviceSettingsRevision(settings_section_t::radio)==revision);
  else assert(result.liveApplied && result.state==settings_save_state_t::partial && radio.frequency==433.5f && !enabledReceive && !radio.radioInit && !somfy.transceiver.available() && deviceSettingsRevision(settings_section_t::radio)==revision+1 && !settingsDurability().status().restartAllowed());
  puts("Actual full namespace save/marker failure reports applied partial and advances radio history; stale capabilities never write");return 0;
 }

 if(mode=="unknown"){radio.apply();assert(!hardware && !FakeNvs::opens && !somfy.transceiver.available());return 0;}
 if(mode=="markerFalse")seedFlag("radioInit",0);
 if(mode=="readFailed")FakeNvs::failOpen=1;
 const bool loaded=somfy.transceiver.loadReadOnly();
 if(mode=="readFailed")assert(!loaded);else assert(loaded);
 counters();
 if(mode=="openFailed")FakeNvs::failOpen=1;
 if(mode=="firstWriteFailed")FakeNvs::failSet=1;
 if(mode=="lastWriteFailed")FakeNvs::failSet=2;
 if(mode=="detectedFailed")detected=false;
 radio.apply();
 const bool success=mode=="success";
 assert(somfy.transceiver.available()==success && radio.radioInit==success && enabledReceive==unsigned(success));
 if(mode=="markerFalse" || mode=="readFailed" || mode=="openFailed" || mode=="firstWriteFailed")assert(!hardware);
 if(mode=="firstWriteFailed" || mode=="lastWriteFailed")assert(!settingsDurability().status().restartAllowed());
 if(success)assert(hardware && marker()==1 && FakeNvs::sets==2);
 puts("Actual radio apply withholds availability and RX until checked marker and hardware completion");
}
