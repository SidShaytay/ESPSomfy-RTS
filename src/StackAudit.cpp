#include "StackAudit.h"
#ifdef ESPSOMFY_STACK_AUDIT
#include <cstdint>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <esp32-hal-log.h>

namespace StackAudit {
namespace {
struct Sample {
  const char *task;
  const char *role;
  uint64_t lastScanUs=0,lastReportUs=0;
  uint32_t minimumUnused=UINT32_MAX,maxScanUs=0,samples=0;
};
// Each entry is written only by its one named RTOS task. No borrowed task
// handles, heap allocations, scheduler suspension or cross-task stack scans.
Sample samples[]={
  {"loopTask","loop"},{"arduino_events","network"},{"async_udp","udp"},
  {"async_tcp","web"},{"radio_tx","radio"},{"github-update","updater"}
};
}
void observeCurrent() {
  const char *name=pcTaskGetName(nullptr);
  if(!name)return;
  for(auto &sample:samples) {
    if(std::strcmp(name,sample.task)!=0)continue;
    const uint64_t now=uint64_t(esp_timer_get_time());
    if(sample.samples && now-sample.lastScanUs<5000000)return;
    sample.lastScanUs=now;
    const uint32_t unused=uint32_t(uxTaskGetStackHighWaterMark(nullptr));
    const uint32_t scanUs=uint32_t(uint64_t(esp_timer_get_time())-now);
    if(unused<sample.minimumUnused)sample.minimumUnused=unused;
    if(scanUs>sample.maxScanUs)sample.maxScanUs=scanUs;
    ++sample.samples;
    if(sample.samples==1 || now-sample.lastReportUs>=30000000) {
      sample.lastReportUs=now;
      // Opt-in audit output must survive release log filtering. The project
      // serializes log_printfv. Scan timing excludes UART/logging.
      log_printf("STACK_AUDIT schema=1 role=%s uptimeMs=%llu minimumUnusedBytes=%lu samples=%lu maxScanUs=%lu\n",
        sample.role,(unsigned long long)(now/1000),(unsigned long)sample.minimumUnused,
        (unsigned long)sample.samples,(unsigned long)sample.maxScanUs);
    }
    return;
  }
}
}
#endif
