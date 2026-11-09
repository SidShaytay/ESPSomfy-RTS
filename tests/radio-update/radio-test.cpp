#include "RadioUpdateActivity.h"
#include "RadioScheduler.h"
#include "PulseDecoder.h"
#include "TxBuffer.h"
#include "CounterEdit.h"
#include "ShadeTypes.h"
#include "SettingsLoadState.h"
#include <cassert>
#include <mutex>
#include <vector>
#include <cstring>
#include <cstdio>
static unsigned pins=0,attaches=0,detaches=0,rxCalls=0;
static bool radioBusy=false,txBusy=false;
static std::vector<char> locks;
static std::mutex receiveMutex;
static thread_local bool rxHeld=false;
static unsigned decoded=0,logCalls=0,normalRadio=0,modelMoves=0,modelGPIO=0,modelCommits=0;
static void logOutsideRx(){assert(!rxHeld);++logCalls;}
using SemaphoreHandle_t=void*;
static SemaphoreHandle_t radioMutex=nullptr,txQueueMutex=nullptr;
static bool radioCreate=true,txCreate=true;
static void *xSemaphoreCreateRecursiveMutex(){return radioCreate ? reinterpret_cast<void*>(1) : nullptr;}
static void *xSemaphoreCreateMutex(){return txCreate ? reinterpret_cast<void*>(2) : nullptr;}
struct RadioLock {
  bool locked;
  explicit RadioLock(unsigned=0):locked(!radioBusy){if(locked){assert(locks.empty() || locks.front()=='r');locks.push_back('r');}}
  ~RadioLock(){if(locked){assert(locks.back()=='r');locks.pop_back();}}
};
struct TxQueueLock {
  bool locked;
  explicit TxQueueLock(unsigned=0):locked(!txBusy){if(locked){assert(locks.size()==1 && locks.back()=='r');locks.push_back('t');}}
  ~TxQueueLock(){if(locked){assert(locks.back()=='t');locks.pop_back();}}
};
struct RxTaskLock {
  std::lock_guard<std::mutex> held;
  RxTaskLock():held(receiveMutex){assert(locks.empty() || locks.front()=='r');locks.push_back('x');rxHeld=true;}
  ~RxTaskLock(){assert(locks.back()=='x');rxHeld=false;locks.pop_back();}
};
struct RxInterruptLock {std::unique_lock<std::mutex> held;RxInterruptLock():held(receiveMutex){rxHeld=true;}void release(){if(held.owns_lock()){rxHeld=false;held.unlock();}}~RxInterruptLock(){release();}};
#define MAX_RX_BUFFER 3
#define RECEIVE_ATTR
#define INPUT 0
#define CHANGE 1
#define CC1101_MARCSTATE 0
static uint8_t rxmode=1;
static int interruptPin=8;
static bool rxPausedForInstall=false,noiseDetected=false;
static unsigned long noiseStart=0;static uint32_t noiseCount=0;
static unsigned long testClock=10000;
static unsigned long clockMs(){assert(!rxHeld);return testClock/1000;}
static unsigned long clockMicros(){assert(!rxHeld);return testClock;}
static void (*beforeDetach)()=nullptr;
static void pinMode(int,int){assert(!rxHeld);++pins;}
static int digitalPinToInterrupt(int p){return p;}
static int digitalRead(int){return 0;}
static void attachInterrupt(int,void(*)(),int){assert(!rxHeld);++attaches;}
static void detachInterrupt(int){assert(!rxHeld);++detaches;if(beforeDetach)beforeDetach();}
static void delay(int){}
static void gpio_intr_disable(int){assert(!rxHeld);}
using gpio_num_t=int;
struct {void SetRx(){assert(!rxHeld);++rxCalls;}uint8_t SpiReadStatus(int){return 0x0d;}void setSidle(){}} ELECHOUSE_cc1101;
struct {void println(const char*){}template<class...T>void printf(const char*,T...){}} Serial;
namespace BoardProfile {constexpr bool integratedRadio=false;constexpr int sw1=1,sw0=2;}
#define ESP_LOGI(...)
#define ESP_LOGD(...) logOutsideRx()
static somfy_rx_t somfy_rx;
struct somfy_rx_queue_t {void init();uint8_t length=0,index[MAX_RX_BUFFER]={255,255,255};somfy_rx_t items[MAX_RX_BUFFER];bool pop(somfy_rx_t *rx){assert(rxHeld);if(!length)return false;--length;*rx=items[0];return true;}} rx_queue;
static somfy_tx_queue_t tx_queue;
struct PublishedBool {
 bool value=false;PublishedBool(bool initial=false):value(initial){}bool load()const{return value;}
 operator bool()const{return value;}PublishedBool &operator=(bool next){value=next;return *this;}
};
struct Transceiver {
  struct {SettingsLoadStatus loadStatus{SettingsLoadState::loaded};bool enabled=true,noiseDetection=false,radioInit=true;int RXPin=8;void apply(){assert(!rxHeld);}}config;
  bool available()const;
  PublishedBool _available{true},_administrationPaused{false};
  RadioScheduler sched;CounterEditSafety counterEdits;
  uint64_t configurationRevision=1,installConfigurationRevision=0,installGeneration=0;
  bool installActive=false,installResumeReceive=false;
  RadioUpdateActivity updateActivityLocked() const;
  struct Frame {bool valid=false;void decodeFrame(somfy_rx_t*){assert(!rxHeld);++decoded;valid=true;}}frame;
  void emitFrame(Frame*,somfy_rx_t*){assert(!rxHeld);}
  bool receive(somfy_rx_t*);
  void loop(){++normalRadio;}bool begin();void startTxTask(){assert(!rxHeld);}
  bool updateActivity(RadioUpdateActivity &) const;
  bool pauseForAdministration(RadioInstallToken &,bool);
  bool pauseForInstall(RadioInstallToken &);bool pauseForRestore(RadioInstallToken &);
  void serviceUntrustedReceive();
  RadioInstallResume resumeAfterInstall(const RadioInstallToken &);
  bool installPauseHeld(const RadioInstallToken &) const;
  void enableReceive();void disableReceive();void clearReceived();static void handleReceive();
};
struct SomfyShadeController {
 struct{bool trusted=true;bool usable()const{return trusted;}}modelLoad;Transceiver transceiver;
 struct Shade{uint8_t id=255;uint8_t getShadeId(){return id;}void checkMovement(){++modelMoves;}void setGPIOs(){++modelGPIO;}}shades[SOMFY_MAX_SHADES];
 bool isDirty=false;unsigned long lastCommit=0;void loop();
 ModelCommitResult commit(){++modelCommits;return {ModelCommitState::saved};}
}somfy;
struct{void modelPersistence(ModelCommitReport){}}protoSocket;
#define millis clockMs
#define micros clockMicros
#include "production-radio.inc"
#undef micros
#undef millis
static void fresh() {
  somfy.transceiver=Transceiver{};somfy.modelLoad.trusted=true;somfy_rx.clear();rx_queue=somfy_rx_queue_t{};tx_queue={};
  radioBusy=txBusy=false;radioCreate=txCreate=true;radioMutex=reinterpret_cast<void*>(1);txQueueMutex=reinterpret_cast<void*>(2);noiseDetected=false;noiseStart=0;noiseCount=0;rxmode=1;interruptPin=8;rxPausedForInstall=false;beforeDetach=nullptr;
  pins=attaches=detaches=rxCalls=decoded=logCalls=normalRadio=modelMoves=modelGPIO=modelCommits=0;somfy.isDirty=false;for(auto &shade:somfy.shades)shade.id=255;testClock+=10000;
}
int main() {
  fresh();assert(somfy.transceiver.begin() && logCalls==1 && !rx_queue.length);
  for(unsigned failure=0;failure<2;++failure) {
    fresh();radioMutex=txQueueMutex=nullptr;radioCreate=failure!=0;txCreate=failure!=1;
    assert(!somfy.transceiver.begin()); // No unsafe startup after resource allocation failure.
    RadioUpdateActivity unknown;unknown.receivedFrames=99;RadioInstallToken noToken;
    assert(!somfy.transceiver.updateActivity(unknown) && unknown.receivedFrames==99);
    assert(!somfy.transceiver.pauseForInstall(noToken) && !noToken.generation && !pins && !detaches && !rxCalls);
  }
  fresh();rxmode=0;interruptPin=0;
  somfy.transceiver.config.loadStatus={SettingsLoadState::failed};somfy.transceiver.config.radioInit=false;
  assert(somfy.transceiver.begin() && !somfy.transceiver.available());
  RadioUpdateActivity failedLoad;RadioInstallToken repair;
  assert(somfy.transceiver.updateActivity(failedLoad) && failedLoad.quiescent() && !failedLoad.ready);
  assert(somfy.transceiver.pauseForRestore(repair));
  assert(somfy.transceiver.resumeAfterInstall(repair)==RadioInstallResume::resumed && !somfy.transceiver.available());
  assert(!pins && !attaches && !detaches && !rxCalls);
  fresh();somfy.transceiver.config.noiseDetection=true;noiseStart=clockMs();noiseCount=100;
  Transceiver::handleReceive();assert(noiseDetected);
  fresh();RadioUpdateActivity activity;
  assert(somfy.transceiver.updateActivity(activity)&&activity.canPause());
  for(unsigned condition=0;condition<17;++condition) {
    fresh();auto &t=somfy.transceiver;somfy_tx_job_t job;job.frame.remoteAddress=123;
    if(condition==0)radioBusy=true;
    if(condition==1)txBusy=true;
    if(condition==2)assert(t.sched.submit(job,false,0,0));
    if(condition==3)rx_queue.length=1;
    if(condition==4)somfy_rx.cpt_synchro_hw=1;
    if(condition==5)somfy_rx.cpt_bits=1;
    if(condition==6)somfy_rx.status=receiving_data;
    if(condition==7)tx_queue.length=1;
    if(condition==8)rxmode=3;
    if(condition==9)assert(t.sched.sealForRestart());
    if(condition==10)assert(t.counterEdits.reserve(123,true,false,false)==CounterEditAdmission::admitted);
    if(condition==11)t.sched.suspend(TX_SUSPEND_SCAN,0);
    if(condition==12 || condition==13) {
      assert(t.sched.submit(job,false,0,0));somfy_tx_job_t taken;uint32_t wait;
      assert(t.sched.next(0,taken,wait));
      if(condition==13){assert(t.sched.start(taken.ticket,0));t.sched.ended(taken.ticket,true,0);}
    }
    if(condition==14)t._available=false;
    if(condition==15)radioMutex=nullptr;
    if(condition==16)txQueueMutex=nullptr;
    if(condition>=15) {
      RadioUpdateActivity unknown;unknown.receivedFrames=99;
      assert(!t.updateActivity(unknown) && unknown.receivedFrames==99);
      assert(!t.installPauseHeld({1}));assert(t.resumeAfterInstall({1})==RadioInstallResume::busy);
    }
    const auto pending=rx_queue.length,sync=somfy_rx.cpt_synchro_hw,bits=somfy_rx.cpt_bits;
    RadioInstallToken token;assert(!t.pauseForInstall(token));assert(!token.generation);
    if(condition!=14)assert(!t.pauseForRestore(token)&&!token.generation);
    assert(!pins&&!attaches&&!detaches&&!rxCalls&&!rxPausedForInstall);
    assert(rx_queue.length==pending && somfy_rx.cpt_synchro_hw==sync && somfy_rx.cpt_bits==bits);
    if(condition==2 || condition==12 || condition==13)assert(t.sched.hasWork());
  }
  fresh();auto &t=somfy.transceiver;RadioInstallToken token;
  rx_queue.length=1;somfy_rx_t received;assert(t.receive(&received)&&!rx_queue.length&&decoded==1);
  beforeDetach=[] {testClock+=10000;Transceiver::handleReceive();assert(!somfy_rx.pulseCount);};
  assert(t.pauseForInstall(token)&&token.generation&&rxPausedForInstall&&detaches==1);
  assert(t.installPauseHeld(token));assert(!t.sched.hasRoom());
  auto retained=token;assert(!t.pauseForInstall(retained)&&retained.generation==token.generation);
  radioBusy=true;assert(t.resumeAfterInstall(token)==RadioInstallResume::busy);radioBusy=false;
  txBusy=true;assert(t.resumeAfterInstall(token)==RadioInstallResume::busy);txBusy=false;
  assert(t.installPauseHeld(token)&&!attaches);
  somfy_tx_job_t job;job.frame.remoteAddress=123;assert(!t.sched.submit(job,false,0,0));
  const auto beforePins=pins;t.enableReceive();t.clearReceived();assert(pins==beforePins&&!attaches&&!rxCalls);
  auto wrong=token;++wrong.generation;assert(t.resumeAfterInstall(wrong)==RadioInstallResume::stale);
  assert(t.installPauseHeld(token)&&rxPausedForInstall);
  assert(t.resumeAfterInstall(token)==RadioInstallResume::resumed);assert(!rxPausedForInstall&&attaches==1&&rxCalls==1);
  assert(!t.installPauseHeld(token));assert(t.resumeAfterInstall(token)==RadioInstallResume::stale);
  assert(t.sched.hasRoom());beforeDetach=nullptr;
  RadioInstallToken next;assert(t.pauseForInstall(next)&&next.generation!=token.generation);
  assert(t.resumeAfterInstall(token)==RadioInstallResume::stale);assert(t.resumeAfterInstall(next)==RadioInstallResume::resumed);
  fresh();token={};t.config.enabled=false;t._available=false;rxmode=0;interruptPin=0;
  assert(t.pauseForInstall(token));assert(t.resumeAfterInstall(token)==RadioInstallResume::resumed);assert(!pins&&!attaches&&!rxCalls&&!detaches);
  fresh();token={};assert(t.pauseForInstall(token));t._available=false;assert(t.resumeAfterInstall(token)==RadioInstallResume::unavailable&&!rxPausedForInstall&&t.sched.hasRoom());
  fresh();token={};t.installGeneration=UINT64_MAX;assert(!t.pauseForInstall(token)&&!token.generation&&!detaches);
  // Same token/seal, but explicit restore can repair unavailable radio config.
  fresh();token={};t._available=false;rxmode=0;interruptPin=0;
  assert(!t.pauseForInstall(token)&&!token.generation);assert(t.pauseForRestore(token)&&!t.available());
  assert(t.installPauseHeld(token));assert(t.resumeAfterInstall(token)==RadioInstallResume::resumed&&!t.available()&&!pins&&!attaches&&!rxCalls);
  // A new applied config under pause cannot expose availability/RX early. The
  // resume honors that config, rather than the prior disabled receive state.
  fresh();token={};rxmode=0;interruptPin=0;assert(t.pauseForRestore(token)&&!t.available());
  ++t.configurationRevision;t.config.enabled=true;t._available=true;t.enableReceive();assert(!t.available()&&!pins&&!attaches&&!rxCalls);
  assert(t.resumeAfterInstall(token)==RadioInstallResume::resumed&&t.available()&&rxCalls==1&&attaches==1);
  fresh();token={};assert(t.pauseForRestore(token));++t.configurationRevision;t.config.enabled=false;t._available=false;
  assert(t.resumeAfterInstall(token)==RadioInstallResume::resumed&&!t.available()&&!attaches&&!rxCalls);
  // Unchanged config preserves an intentionally inactive receiver.
  fresh();token={};rxmode=0;interruptPin=0;assert(t.pauseForRestore(token));assert(t.resumeAfterInstall(token)==RadioInstallResume::resumed&&t.available()&&!rxCalls&&!attaches);
  // Quarantined receive servicing is bounded memory-only; no decode, model,
  // repeater/TX, GPIO or NVS path exists in the compiled boundary fixture.
  fresh();rx_queue.length=MAX_RX_BUFFER;somfy_rx.cpt_bits=12;somfy_rx.cpt_synchro_hw=2;somfy_rx.status=receiving_data;
  t.serviceUntrustedReceive();assert(rx_queue.length==MAX_RX_BUFFER&&somfy_rx.cpt_bits==12); // trusted frames untouched
  somfy.modelLoad.trusted=false;t.serviceUntrustedReceive();assert(!rx_queue.length&&!somfy_rx.cpt_bits&&!somfy_rx.cpt_synchro_hw&&somfy_rx.status==waiting_synchro);
  assert(!pins&&!attaches&&!detaches&&!rxCalls&&!decoded&&!logCalls&&!tx_queue.length);
  assert(t.pauseForRestore(token={})&&t.installPauseHeld(token));rx_queue.length=1;somfy_rx.cpt_bits=1;
  t.serviceUntrustedReceive();assert(rx_queue.length==1&&somfy_rx.cpt_bits==1); // exact paused source owns its state
  rx_queue.length=0;somfy_rx.clear();assert(t.resumeAfterInstall(token)==RadioInstallResume::resumed);
  fresh();somfy.modelLoad.trusted=false;somfy.isDirty=true;somfy.shades[0].id=1;
  rx_queue.length=MAX_RX_BUFFER;somfy_rx.cpt_bits=9;somfy.loop();
  assert(!rx_queue.length&&!somfy_rx.cpt_bits&&!normalRadio&&!modelMoves&&!modelGPIO&&!modelCommits);
  somfy.modelLoad.trusted=true;somfy.isDirty=false;somfy.loop();assert(normalRadio==1&&modelMoves==1&&modelGPIO==1&&!modelCommits);
  puts("Actual radio pause/resume: pending work, ISR race, exact token, no flush/TX and disabled-radio preservation passed");
}
