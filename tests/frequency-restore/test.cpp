#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include "Arduino.h"
static uint8_t rxmode=1;
static uint32_t scanStart=0;
static int interruptPin=4,markRSSI=-100;
static float markFreq=433,currFreq=433;
static std::atomic<bool> scanEmitPending{false};
[[maybe_unused]] static bool scanResumeReceive=false;
static unsigned storageWrites=0,enabled=0,driverWrites=0;
static constexpr int INPUT=0,CHANGE=1,TX_SUSPEND_SCAN=2;
static constexpr unsigned portMAX_DELAY=1000;
struct RadioLock{explicit RadioLock(unsigned=portMAX_DELAY,bool=false){}};
static void pinMode(int,int){}
static int digitalPinToInterrupt(int pin){return pin;}
static void attachInterrupt(int,void(*)(),int){}
static void detachInterrupt(int){}
#define ESP_LOGD(...) ((void)0)
struct Driver {
 bool receiving=false;float frequency=0,bandwidth=0;
 void setRxBW(float value){bandwidth=value;++driverWrites;}
 void setMHZ(float value){frequency=value;++driverWrites;}
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
 puts("Actual frequency scan end restores configured tuning and prior RX without NVS");
}
