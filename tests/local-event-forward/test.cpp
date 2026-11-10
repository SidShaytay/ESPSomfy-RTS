#include "TxBuffer.h"
#include "LocalControlQueue.h"
#include <assert.h>
#include <vector>
static unsigned depth=0,uiCalls=0,pops=0;
static std::vector<somfy_tx_event_t> modeled,observed;
struct TxQueueLock {
  TxQueueLock(){assert(depth==0);++depth;}
  ~TxQueueLock(){assert(depth==1);--depth;}
};
struct Scheduler {
  std::vector<somfy_tx_event_t> events;
  bool popEvent(somfy_tx_event_t &out){
    assert(depth==1);++pops;
    if(events.empty())return false;
    out=events.front();events.erase(events.begin());return true;
  }
};
struct Transceiver {Scheduler sched;void loop();};
// The receive half of the loop is outside this focused consumer extraction.
struct somfy_rx_t {~somfy_rx_t(){}};
struct Model {
  void processTxEvent(const somfy_tx_event_t &event){assert(depth==0);modeled.push_back(event);}
} somfy;
namespace LocalUi {
void localControlTxEvent(const somfy_tx_event_t &event){
  assert(depth==0 && !modeled.empty());
  assert(modeled.size()==observed.size()+1);
  assert(modeled.back().ticket==event.ticket && modeled.back().type==event.type);
  observed.push_back(event);++uiCalls;
}
}
#define ESP_LOGW(...) ((void)0)
#define TAG "test"
#include "production-event-loop.inc"
int main(){
  Transceiver radio;
  for(const auto type:{tx_event_t::started,tx_event_t::finished,tx_event_t::aborted,tx_event_t::dropped}){
    somfy_tx_event_t event;event.ticket=100+radio.sched.events.size();event.type=type;
    radio.sched.events.push_back(event);
  }
  radio.loop();
  assert(modeled.size()==4 && pops==5 && depth==0);
#ifdef ESPSOMFY_LOCAL_UI
  assert(uiCalls==4 && observed.size()==4);
#else
  assert(uiCalls==0 && observed.empty());
#endif
  radio.loop();assert(modeled.size()==4 && pops==6 && depth==0);
}
