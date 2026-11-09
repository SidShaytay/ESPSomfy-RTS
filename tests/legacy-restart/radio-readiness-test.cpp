#include "RadioScheduler.h"
#include <assert.h>
// The raw repeater buffer is loop-owned. Only the hardware lock is replaced;
// the readiness/seal bodies below are extracted unchanged from Transceiver.cpp.
struct TxQueueLock {};
static somfy_tx_queue_t tx_queue;
struct Transceiver {
  RadioScheduler sched;
  bool restartQuiescent();
  bool sealForRestart();
  void reopenAfterRefusedRestart();
};
#include "production-radio-readiness.inc"
int main() {
  Transceiver radio;
  assert(radio.restartQuiescent());
  byte bytes[10] = {};
  tx_queue.push(4,bytes,56);
  assert(!radio.restartQuiescent());
  assert(!radio.sealForRestart());
  assert(tx_queue.length==1); // no flushing accepted raw work
  somfy_tx_t raw;assert(tx_queue.pop(&raw));
  assert(radio.restartQuiescent());
  somfy_tx_job_t job;job.frame.remoteAddress=123;
  assert(radio.sched.submit(job,false,0,0));
  assert(!radio.restartQuiescent() && !radio.sealForRestart());
  uint32_t wait;assert(radio.sched.next(0,job,wait));
  assert(!radio.restartQuiescent());
  assert(radio.sched.start(job.ticket,0));radio.sched.ended(job.ticket,true,1);
  assert(!radio.restartQuiescent()); // model has not observed outcomes yet
  somfy_tx_event_t event;while(radio.sched.popEvent(event)) {}
  assert(radio.restartQuiescent() && radio.sealForRestart());
  job.ticket=456;assert(!radio.sched.submit(job,false,0,2) && !job.ticket);
  radio.reopenAfterRefusedRestart();
  assert(radio.sched.submit(job,false,0,2));
}
