#include "GitUpdateBootHealth.h"
#include <cassert>
#include <cstring>
#include <string>
static uint64_t clockMs=0;
static bool otaPending=true,routes=true,filesystem=true,allocator=true,graphics=true,paused=false,busy=false;
static bool updaterBootloaderHealthy=true,updaterAssetsHealthy=true,confirmationFails=false;
static unsigned confirmations=0,modelLoops=0,drains=0,changes=0,startupStep=0;
static constexpr unsigned long OTA_PROVEN_MS=60000;
static constexpr int ESP_OK=0,MALLOC_CAP_INTERNAL=1,MALLOC_CAP_8BIT=2;
static size_t freeBytes=32768;
static GitUpdateBootHealth otaHealth;
void gitUpdateRecoveryLoop(bool,uint64_t){}
struct {uint32_t duration=1000;uint32_t us()const{return duration;}}pass;
struct {bool online=true,softAPOpened=false;bool connected()const{return online;}}net;
struct {
  bool isDirty=false;
  struct {bool trusted=true;bool usable()const{return trusted;}}modelLoad;
  struct {
    bool uncertain=false,ready=true;
    struct {struct{bool valid=true;bool usable()const{return valid;}}loadStatus;bool enabled=false,radioInit=true;}config;
    bool available()const{return ready;}bool counterEditUncertain()const{return uncertain;}
  }transceiver;
  void loop(){++modelLoops;}void sendChanges(){++changes;}
}somfy;
struct {bool trusted=true;bool settingsUsable()const{return trusted;}}settings;
static bool durable=true;
struct Durability {Durability status()const{return{};}bool restartAllowed()const{return durable;}};
static Durability settingsDurability(){return{};}
struct {bool busy()const{return ::busy;}}gitUpdate;
static int64_t esp_timer_get_time(){return int64_t(clockMs)*1000;}
static unsigned long millis(){return clockMs;}
static size_t heap_caps_get_free_size(int){return freeBytes;}
static bool modelFilesystemMounted(){return filesystem;}
static bool gitUpdateMemoryReady(){return allocator;}
static bool gitUpdateControllerPaused(){return paused;}
static bool waitingRestart=false,detectedMovement=false,stopAvailable=false;
namespace StatusPanel {static bool healthy(){return graphics;}static void loop(){if(detectedMovement)stopAvailable=!waitingRestart;}}
static bool restartLoop(){if(detectedMovement)waitingRestart=false;return false;}
static void loopGitUpdateController(){}
struct {void loop(){}}webServer,protoSocket;
static bool bundledWebRoutesReady(){return routes;}
static int esp_ota_mark_app_valid_cancel_rollback(){++confirmations;return confirmationFails?-1:ESP_OK;}
static bool gitUpdateMemoryBegin(){assert(startupStep++==0);return allocator;}
struct {void begin(int){assert(startupStep++==1);}}Serial;
namespace BoardProfile {static void begin(){assert(startupStep++==2);}}
namespace StatusPanel {static void begin(){assert(startupStep++==3);}}
struct {void drain(){++drains;}}loopQueue;
struct {uint32_t lap(){return 0;}}timing;
#define SERIAL_BAUD 115200
#define logIfSlow(...) ((void)0)
#define esp_task_wdt_reset() ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#include "production-boot-health.inc"
static void sampleUntil(uint64_t end){while(end-clockMs>500){clockMs+=500;health();}clockMs=end;health();}
int main(int argc,char **argv){
  assert(argc==2);const std::string mode=argv[1];otaHealth.start(true,0);health();
  if(mode=="longGap"){
    clockMs=60000;health();assert(otaPending && !confirmations);
    sampleUntil(119999);assert(!confirmations);sampleUntil(120000);assert(confirmations==1 && !otaPending);return 0;
  }
  if(mode=="startup" || mode=="allocatorFailureStartup"){
    if(mode=="allocatorFailureStartup")allocator=false;
    startupPrefix();assert(startupStep==4);return 0;
  }
  if(mode=="loop"){
    paused=true;modelPass();assert(!modelLoops && !drains && !changes);
    paused=false;modelPass();assert(modelLoops==1 && drains==1 && changes==1);return 0;
  }
  if(mode=="stopAfterMovement"){
    detectedMovement=waitingRestart=true;presentationPass();assert(stopAvailable && !waitingRestart);return 0;
  }
  if(mode=="rollbackClock"){
    clockMs=1000;health();clockMs=999;health();assert(otaHealth.rollbackRequired() && !confirmations);health();assert(otaHealth.rollbackRequired());return 0;
  }
  if(mode=="deadline"){
    clockMs=179000;graphics=false;health();graphics=true;health();clockMs=180000;health();
    assert(otaPending && otaHealth.rollbackRequired() && !confirmations);clockMs=181000;health();assert(otaHealth.rollbackRequired());return 0;
  }
  if(mode=="retry"){
    confirmationFails=true;sampleUntil(60000);assert(otaPending && confirmations==1);
    clockMs=60001;health();assert(confirmations==1);clockMs=61000;confirmationFails=false;health();
    assert(!otaPending && confirmations==2 && !otaHealth.rollbackRequired());return 0;
  }
  if(mode=="failedUntilDeadline"){
    confirmationFails=true;sampleUntil(60000);clockMs=180000;health();assert(otaPending && confirmations==1 && otaHealth.rollbackRequired());return 0;
  }
  if(mode!="valid"){
    clockMs=59000;
    if(mode=="network")net.online=false;
    if(mode=="routes")routes=false;
    if(mode=="filesystem")filesystem=false;
    if(mode=="allocator")allocator=false;
    if(mode=="graphics")graphics=false;
    if(mode=="bootloader")updaterBootloaderHealthy=false;
    if(mode=="assets")updaterAssetsHealthy=false;
    if(mode=="memory")freeBytes=24575;
    if(mode=="slow")pass.duration=1000000;
    if(mode=="paused")paused=true;
    if(mode=="busy")busy=true;
    if(mode=="counter")somfy.transceiver.uncertain=true;
    if(mode=="radio"){somfy.transceiver.config.enabled=true;somfy.transceiver.ready=false;}
    if(mode=="model")somfy.modelLoad.trusted=false;
    if(mode=="settings")settings.trusted=false;
    if(mode=="durability")durable=false;
    if(mode=="dirty")somfy.isDirty=true;
    health();assert(!confirmations);
    net.online=routes=filesystem=allocator=graphics=updaterBootloaderHealthy=updaterAssetsHealthy=true;
    freeBytes=32768;pass.duration=1000;paused=busy=false;somfy.transceiver.uncertain=false;somfy.transceiver.ready=true;
    somfy.modelLoad.trusted=settings.trusted=durable=true;somfy.isDirty=false;
    clockMs=60000;health();assert(!confirmations);sampleUntil(119999);assert(!confirmations);clockMs=120000;
  }else{sampleUntil(59999);assert(!confirmations);clockMs=60000;}
  health();assert(!otaPending && confirmations==1 && !otaHealth.rollbackRequired());
}
