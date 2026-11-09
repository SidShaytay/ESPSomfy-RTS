#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include "Arduino.h"
static uint8_t rxmode=1;
static uint32_t scanStart=0,lastScan=0;
static unsigned long scanNow=0;
#define millis() scanNow
#define SCAN_MAX_MS 300000
#define ESP_LOGW(...) ((void)0)
static constexpr int waiting_synchro=0;
static bool noiseDetected=false;
static struct {int status=waiting_synchro;} somfy_rx;
static int currRSSI=-100;
[[maybe_unused]] static std::atomic<bool> scanStopRequested{false};
static int interruptPin=4,markRSSI=-100;
static float markFreq=433,currFreq=433;
static std::atomic<bool> scanEmitPending{false};
[[maybe_unused]] static bool scanResumeReceive=false;
static unsigned storageWrites=0,enabled=0,driverWrites=0;
static constexpr int INPUT=0,CHANGE=1,TX_SUSPEND_SCAN=2;
static constexpr unsigned portMAX_DELAY=1000;
static bool lockAvailable=true;static unsigned lastWait=portMAX_DELAY;
struct RadioLock{bool locked;explicit RadioLock(unsigned wait=portMAX_DELAY,bool=false):locked(lockAvailable){lastWait=wait;}};
static void pinMode(int,int){}
static int digitalPinToInterrupt(int pin){return pin;}
static void attachInterrupt(int,void(*)(),int){}
static void detachInterrupt(int){}
#define ESP_LOGD(...) ((void)0)
struct Driver {
 bool receiving=false;float frequency=0,bandwidth=0;
 void setRxBW(float value){bandwidth=value;++driverWrites;}
 void setMHZ(float value){frequency=value;++driverWrites;}
 int getRssi(){return -100;}
 void SetRx(){receiving=true;}
 void setSidle(){receiving=false;}
}ELECHOUSE_cc1101;
struct Transceiver {
 struct Config{bool enabled=true;int RXPin=4;float frequency=433.42f,rxBandwidth=99.97f;void apply(){++storageWrites;}}config;
 bool available(){return true;}
 void suspendTx(uint8_t){}
 void resumeTx(uint8_t){}
 void disableReceive(){rxmode=0;}
 void enableReceive(){rxmode=1;++enabled;ELECHOUSE_cc1101.SetRx();}
 static void handleReceive(){}
 void beginFrequencyScan();void endFrequencyScan();
 void processFrequencyScan(bool=false);void emitFrequencyScan(){}void serviceScanStop();
};
#include "scan.inc"
int main(){
 for(uint8_t prior:{0,1}) {
  Transceiver radio;rxmode=prior;enabled=storageWrites=driverWrites=0;
  radio.beginFrequencyScan();assert(rxmode==3);
  ELECHOUSE_cc1101.frequency=433.88f;ELECHOUSE_cc1101.bandwidth=200.0f;
  radio.beginFrequencyScan(); // A duplicate Start must not lose the original RX state.
  radio.endFrequencyScan();
  assert(!storageWrites && "Ending observation must not initialize or write settings");
  assert(std::fabs(ELECHOUSE_cc1101.frequency-radio.config.frequency)<0.0001f);
  assert(std::fabs(ELECHOUSE_cc1101.bandwidth-radio.config.rxBandwidth)<0.0001f);
  assert(rxmode==prior && enabled==(prior==1) && ELECHOUSE_cc1101.receiving==(prior==1));
  const auto writes=driverWrites;radio.endFrequencyScan();assert(driverWrites==writes && !storageWrites);
 }
 // Busy cleanup preserves the active scan, immediately stops future tuning,
 // and the actual loop cleanup retries without a second user action.
 Transceiver radio;rxmode=1;lockAvailable=true;radio.beginFrequencyScan();
 const auto writes=driverWrites;lockAvailable=false;radio.endFrequencyScan();
 assert(rxmode==3 && driverWrites==writes && scanStopRequested.load() && lastWait==0);
 lockAvailable=true;scanNow=200;lastScan=0;radio.processFrequencyScan(false);
 assert(driverWrites==writes && rxmode==3);
 radio.serviceScanStop();assert(rxmode==1 && !scanStopRequested.load() && !storageWrites);
 puts("Actual frequency scan end restores configured tuning and prior RX without NVS");
}
