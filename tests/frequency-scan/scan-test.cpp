#include "RadioScheduler.h"
#include "RadioUpdateActivity.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cmath>
static unsigned driverCalls=0,preemptions=0,suspensions=0;
static bool radioBusy=false,queueBusy=false,resourcesReady=true,rxHeld=false;
static constexpr unsigned portMAX_DELAY=1000;
static constexpr int INPUT=0,CHANGE=1,waiting_synchro=0;
static uint8_t rxmode=1;
static unsigned scanStart=0,lastScan=0;
static bool scanResumeReceive=false,rxPausedForInstall=false,noiseDetected=false;
static unsigned noiseStart=0,noiseCount=0;
static std::atomic<bool> scanStopRequested{false};
static int interruptPin=4,currRSSI=-100,markRSSI=-100;
static float markFreq=433,currFreq=433;
static std::atomic<bool> scanEmitPending{false};
static bool radioUpdateResourcesReady(){return resourcesReady;}
struct RadioLock {bool locked;explicit RadioLock(unsigned wait=0,bool preempt=false):locked(!radioBusy){assert(wait==0);preemptions+=preempt;}};
struct TxQueueLock {bool locked;explicit TxQueueLock(unsigned wait=0):locked(!queueBusy){assert(wait==0);}};
struct RxTaskLock {RxTaskLock(){assert(!rxHeld);rxHeld=true;}~RxTaskLock(){rxHeld=false;}};
static void io(){assert(!rxHeld);++driverCalls;}
static void pinMode(int,int){io();}
static int digitalPinToInterrupt(int value){return value;}
static void attachInterrupt(int,void(*)(),int){io();}
static void detachInterrupt(int){io();}
struct {float frequency=0,bandwidth=0;bool receiving=false;
 void setRxBW(float v){assert(rxmode!=0||rxPausedForInstall);io();bandwidth=v;}void SetRx(){io();receiving=true;}
 void setMHZ(float v){assert(rxmode!=0||rxPausedForInstall);io();frequency=v;}void setSidle(){io();receiving=false;}
 int getRssi(){io();return -65;}
} ELECHOUSE_cc1101;
struct {unsigned length=0;void init(){assert(rxHeld);length=0;}}rx_queue,tx_queue;
struct {int status=waiting_synchro;bool partial=false;void clear(){assert(rxHeld);status=waiting_synchro;partial=false;}}somfy_rx;
#define ESP_LOGD(...) ((void)0)
struct Transceiver {
 struct {bool enabled=true;int RXPin=4;float frequency=433.42f,rxBandwidth=99.97f;}config;
 std::atomic<bool> _available{true},_administrationPaused{false};
 RadioScheduler sched;bool counterBusy=false;
 bool available(){return _available.load()&&!_administrationPaused.load();}
 RadioUpdateActivity updateActivityLocked(){
  assert(rxHeld);RadioUpdateActivity a;a.observed=true;a.ready=available();a.schedulerIdle=!sched.hasWork();
  a.suspended=sched.suspended();a.txSealed=sched.sealedForRestart();a.counterBusy=counterBusy;
  a.receivedFrames=rx_queue.length;a.repeaterFrames=tx_queue.length;a.receivingFrame=somfy_rx.partial;
  a.scanning=rxmode==3;a.receiverPaused=rxPausedForInstall;return a;
 }
 void suspendTx(uint8_t mask){++suspensions;sched.suspend(mask,millis());}
 void resumeTx(uint8_t mask){sched.resume(mask);}
 void disableReceive(){io();rxmode=0;}
 void enableReceive(){io();rxmode=1;ELECHOUSE_cc1101.SetRx();}
 static void handleReceive(){}
 void beginFrequencyScan();void endFrequencyScan();void processFrequencyScan(bool=false);void emitFrequencyScan(){}
};
#include "production-scan.inc"
static somfy_tx_job_t job(){somfy_tx_job_t j;j.frame.remoteAddress=123;j.tx.bit_length=56;return j;}
static void reset(){rxmode=1;scanStopRequested=false;rxPausedForInstall=false;scanResumeReceive=false;radioBusy=queueBusy=false;resourcesReady=true;rx_queue.length=tx_queue.length=0;somfy_rx.partial=false;driverCalls=preemptions=suspensions=0;}
static void untouched(Transceiver &r){assert(!driverCalls&&!preemptions&&!suspensions&&rxmode==1&&!r._administrationPaused.load());}
int main(){
 reset();Transceiver accepted;auto j=job();assert(accepted.sched.submit(j,false,0,0));
 accepted.beginFrequencyScan();assert(accepted.sched.hasPendingFor(123));untouched(accepted);
 // Taken/on-air work and not-yet-consumed outcomes also prevent retuning.
 for(unsigned phase=0;phase<3;++phase){
  reset();Transceiver r;auto queued=job();const auto ticket=r.sched.submit(queued,false,0,0);
  somfy_tx_job_t taken;uint32_t wait=0;assert(r.sched.next(0,taken,wait));
  if(phase>0)assert(r.sched.start(ticket,0));
  if(phase>1)r.sched.ended(ticket,true,1);
  r.beginFrequencyScan();assert(r.sched.hasWork());untouched(r);
 }
 reset();Transceiver suspended;suspended.sched.suspend(TX_SUSPEND_OTA,0);
 suspended.beginFrequencyScan();assert(suspended.sched.suspended());untouched(suspended);
 for(unsigned which=0;which<10;++which){
  reset();Transceiver r;
  switch(which){case 0:radioBusy=true;break;case 1:queueBusy=true;break;case 2:resourcesReady=false;break;
   case 3:rx_queue.length=1;break;case 4:tx_queue.length=1;break;case 5:somfy_rx.partial=true;break;
   case 6:r.counterBusy=true;break;case 7:rxPausedForInstall=true;break;
   case 8:r.config.enabled=false;break;case 9:r._available=false;break;}
  r.beginFrequencyScan();untouched(r);
 }
 for(unsigned prior:{0u,1u}){
  reset();Transceiver r;rxmode=prior;r.beginFrequencyScan();
  assert(rxmode==3&&r.sched.sealedForRestart()&&!r.available()&&!rxPausedForInstall);
  auto next=job();assert(!r.sched.submit(next,true,0,0));
  const unsigned started=driverCalls;r.beginFrequencyScan();assert(driverCalls==started);
  rx_queue.length=2;somfy_rx.partial=true;noiseDetected=true;noiseCount=17;
  queueBusy=true;r.endFrequencyScan();assert(rxmode==3&&scanStopRequested.load()&&r.sched.sealedForRestart());
  const unsigned pending=driverCalls;r.processFrequencyScan();assert(driverCalls==pending);
  queueBusy=false;r.endFrequencyScan();
  assert(rxmode==prior&&!scanStopRequested.load()&&!r.sched.sealedForRestart()&&r.available());
  assert(!rx_queue.length&&!somfy_rx.partial&&!noiseDetected&&!noiseCount);
  assert(std::fabs(ELECHOUSE_cc1101.frequency-r.config.frequency)<0.0001f);
  assert(std::fabs(ELECHOUSE_cc1101.bandwidth-r.config.rxBandwidth)<0.0001f);
  const unsigned ended=driverCalls;r.endFrequencyScan();assert(driverCalls==ended);
  assert(r.sched.submit(next,false,0,0));
 }
 puts("Actual scan refuses busy admission, preserves accepted TX, seals RF and discards only scan-owned RX after restoration");
}
