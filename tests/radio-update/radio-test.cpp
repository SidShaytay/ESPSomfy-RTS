#include "RadioUpdateActivity.h"
#include "RadioScheduler.h"
#include "PulseDecoder.h"
#include "TxBuffer.h"
#include "CounterEdit.h"
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
static unsigned decoded=0,logCalls=0;
static void logOutsideRx(){assert(!rxHeld);++logCalls;}
using SemaphoreHandle_t=void*;
static SemaphoreHandle_t radioMutex=nullptr,txQueueMutex=nullptr;
static void *xSemaphoreCreateRecursiveMutex(){return reinterpret_cast<void*>(1);}
static void *xSemaphoreCreateMutex(){return reinterpret_cast<void*>(2);}
struct RadioLock {
  bool locked;
  explicit RadioLock(unsigned=0):locked(!radioBusy){if(locked){assert(locks.empty());locks.push_back('r');}}
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
static void pinMode(int,int){++pins;}
static int digitalPinToInterrupt(int p){return p;}
static int digitalRead(int){return 0;}
static void attachInterrupt(int,void(*)(),int){++attaches;}
static void detachInterrupt(int){++detaches;if(beforeDetach)beforeDetach();}
static void delay(int){}
static void gpio_intr_disable(int){assert(!rxHeld);}
using gpio_num_t=int;
struct {void SetRx(){++rxCalls;}uint8_t SpiReadStatus(int){return 0x0d;}void setSidle(){}} ELECHOUSE_cc1101;
struct {void println(const char*){}template<class...T>void printf(const char*,T...){}} Serial;
namespace BoardProfile {constexpr bool integratedRadio=false;constexpr int sw1=1,sw0=2;}
#define ESP_LOGI(...)
#define ESP_LOGD(...) logOutsideRx()
static somfy_rx_t somfy_rx;
struct somfy_rx_queue_t {void init();uint8_t length=0,index[MAX_RX_BUFFER]={255,255,255};somfy_rx_t items[MAX_RX_BUFFER];bool pop(somfy_rx_t *rx){assert(rxHeld);if(!length)return false;--length;*rx=items[0];return true;}} rx_queue;
static somfy_tx_queue_t tx_queue;
struct Transceiver {
  struct {bool enabled=true,noiseDetection=false,radioInit=true;int RXPin=8;void load(){assert(!rxHeld);}void apply(){assert(!rxHeld);}}config;
  bool ready=true;bool available() const{return ready;}
  bool _available=true;
  RadioScheduler sched;CounterEditSafety counterEdits;
  uint64_t installGeneration=0;
  bool installActive=false,installResumeReceive=false;
  RadioUpdateActivity updateActivityLocked() const;
  struct Frame {bool valid=false;void decodeFrame(somfy_rx_t*){assert(!rxHeld);++decoded;valid=true;}}frame;
  void emitFrame(Frame*,somfy_rx_t*){assert(!rxHeld);}
  bool receive(somfy_rx_t*);
  bool begin();void startTxTask(){assert(!rxHeld);}
  bool updateActivity(RadioUpdateActivity &) const;
  bool pauseForInstall(RadioInstallToken &);
  RadioInstallResume resumeAfterInstall(const RadioInstallToken &);
  bool installPauseHeld(const RadioInstallToken &) const;
  void enableReceive();void disableReceive();void clearReceived();static void handleReceive();
};
struct {Transceiver transceiver;} somfy;
#define millis clockMs
#define micros clockMicros
#include "production-radio.inc"
#undef micros
#undef millis
static void fresh() {
  somfy.transceiver=Transceiver{};somfy_rx.clear();rx_queue=somfy_rx_queue_t{};tx_queue={};
  radioBusy=txBusy=false;noiseDetected=false;noiseStart=0;noiseCount=0;rxmode=1;interruptPin=8;rxPausedForInstall=false;beforeDetach=nullptr;
  pins=attaches=detaches=rxCalls=decoded=logCalls=0;testClock+=10000;
}
int main() {
  fresh();assert(somfy.transceiver.begin() && logCalls==1 && !rx_queue.length);
  fresh();somfy.transceiver.config.noiseDetection=true;noiseStart=clockMs();noiseCount=100;
  Transceiver::handleReceive();assert(noiseDetected);
  fresh();RadioUpdateActivity activity;
  assert(somfy.transceiver.updateActivity(activity)&&activity.canPause());
  for(unsigned condition=0;condition<15;++condition) {
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
    if(condition==14)t.ready=false;
    const auto pending=rx_queue.length,sync=somfy_rx.cpt_synchro_hw,bits=somfy_rx.cpt_bits;
    RadioInstallToken token;assert(!t.pauseForInstall(token));assert(!token.generation);
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
  fresh();token={};t.config.enabled=false;t.ready=false;rxmode=0;interruptPin=0;
  assert(t.pauseForInstall(token));assert(t.resumeAfterInstall(token)==RadioInstallResume::resumed);assert(!pins&&!attaches&&!rxCalls&&!detaches);
  fresh();token={};assert(t.pauseForInstall(token));t.ready=false;assert(t.resumeAfterInstall(token)==RadioInstallResume::unavailable&&!rxPausedForInstall&&t.sched.hasRoom());
  fresh();token={};t.installGeneration=UINT64_MAX;assert(!t.pauseForInstall(token)&&!token.generation&&!detaches);
  puts("Actual radio pause/resume: pending work, ISR race, exact token, no flush/TX and disabled-radio preservation passed");
}
