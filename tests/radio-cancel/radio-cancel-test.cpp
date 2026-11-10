#include "RadioScheduler.h"
#include <assert.h>
// Exercise the unchanged production cancelTx body with the real scheduler.
// Only FreeRTOS locking/clock/task notification are substituted. Deliberately
// no RadioLock seam: cancellation must not wait behind a transmission.
static unsigned lockDepth=0,notifications=0;
static uint32_t clockNow=10;
static void *txTaskHandle=reinterpret_cast<void *>(1);
struct TxQueueLock {
  TxQueueLock() { assert(lockDepth==0);++lockDepth; }
  ~TxQueueLock() { assert(lockDepth==1);--lockDepth; }
};
static uint32_t cancellationClock() { assert(lockDepth==1);return clockNow; }
static void xTaskNotifyGive(void *task) {
  assert(lockDepth==0 && task==txTaskHandle);++notifications;
}
struct Transceiver {
  RadioScheduler sched;
  bool cancelTx(uint32_t ticket);
};
#define millis cancellationClock
#include "production-radio-cancel.inc"
#undef millis
static uint32_t submit(Transceiver &radio,uint32_t remote,uint16_t delay=0) {
  somfy_tx_job_t job;job.frame.remoteAddress=remote;job.repeats=127;
  return radio.sched.submit(job,false,delay,0);
}
int main() {
  Transceiver radio;
  const auto owned=submit(radio,1,5000),other=submit(radio,2);
  assert(owned && other);
  assert(!radio.cancelTx(0) && notifications==0);
  assert(!radio.cancelTx(other+1) && notifications==0);
  assert(radio.cancelTx(owned) && notifications==1);
  somfy_tx_event_t event;
  assert(radio.sched.popEvent(event) && event.ticket==owned &&
    event.type==tx_event_t::dropped && event.at==clockNow);
  assert(!radio.cancelTx(owned) && notifications==1);
  somfy_tx_job_t job;uint32_t wait=0;
  assert(radio.sched.next(clockNow,job,wait) && job.ticket==other);
  assert(radio.sched.start(other,clockNow));
  assert(radio.cancelTx(other) && notifications==2);
  assert(!radio.sched.keepSending(other));
  radio.sched.ended(other,false,clockNow+1);
  assert(radio.sched.popEvent(event) && event.ticket==other && event.type==tx_event_t::started);
  assert(radio.sched.popEvent(event) && event.ticket==other && event.type==tx_event_t::aborted);
  assert(!radio.sched.popEvent(event));
  assert(!radio.cancelTx(other) && notifications==2);
  const auto detached=submit(radio,3);
  txTaskHandle=nullptr;
  assert(radio.cancelTx(detached) && notifications==2);
  assert(radio.sched.popEvent(event) && event.ticket==detached && event.type==tx_event_t::dropped);
  assert(!radio.sched.hasWork() && lockDepth==0);
}
