#include "StackAudit.h"
#ifdef ESPSOMFY_STACK_AUDIT
#include <atomic>
#include <cstdint>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/idf_additions.h>
#include <freertos/timers.h>
#include <esp_event.h>
#include <lwip/tcpip.h>
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
void printSample(const Sample &sample, uint64_t now) {
  // Fixed roles/numbers only. UART time is outside maxScanUs.
  log_printf("STACK_AUDIT schema=1 role=%s uptimeMs=%llu minimumUnusedBytes=%lu samples=%lu maxScanUs=%lu\n",
    sample.role,(unsigned long long)(now/1000),(unsigned long)sample.minimumUnused,
    (unsigned long)sample.samples,(unsigned long)sample.maxScanUs);
}
void updateSample(Sample &sample, uint32_t unused, uint32_t scanUs) {
  if(unused<sample.minimumUnused)sample.minimumUnused=unused;
  if(scanUs>sample.maxScanUs)sample.maxScanUs=scanUs;
  ++sample.samples;
}
// SDK callbacks publish numeric snapshots under a short lock. Stack scans and
// UART work never hold that lock; loop alone formats their records.
struct DeferredSample {
  Sample value;
  portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
  uint32_t reportedSamples=0;
};
DeferredSample sdkSamples[]={
  {{"sys_evt","system-events"}},{{"tiT","tcpip"}},{{"esp_timer","esp-timer"}}
};
Sample stableSamples[]={{nullptr,"idle0"},{nullptr,"idle1"},{nullptr,"timer-daemon"}};
std::atomic<bool> tcpipPending{false};
esp_event_handler_instance_t eventInstance=nullptr;
esp_timer_handle_t sdkTimer=nullptr;
bool eventRegistered=false,timerStarted=false,beginAttempted=false,pollStarted=false;
uint64_t lastBeginUs=0,lastPollUs=0;
uint8_t stableIndex=0;
void observeSdk(DeferredSample &deferred) {
  const char *task=pcTaskGetName(nullptr);
  if(!task || std::strcmp(task,deferred.value.task)!=0)return;
  const uint64_t now=uint64_t(esp_timer_get_time());
  portENTER_CRITICAL(&deferred.mux);
  const bool due=!deferred.value.samples || now-deferred.value.lastScanUs>=5000000;
  if(due)deferred.value.lastScanUs=now;
  portEXIT_CRITICAL(&deferred.mux);
  if(!due)return;
  const uint32_t unused=uint32_t(uxTaskGetStackHighWaterMark(nullptr));
  const uint32_t scanUs=uint32_t(uint64_t(esp_timer_get_time())-now);
  portENTER_CRITICAL(&deferred.mux);
  updateSample(deferred.value,unused,scanUs);
  portEXIT_CRITICAL(&deferred.mux);
}
void reportSdk(DeferredSample &deferred, uint64_t now) {
  Sample snapshot{};
  portENTER_CRITICAL(&deferred.mux);
  const bool due=deferred.value.samples && (!deferred.reportedSamples ||
    (deferred.value.samples!=deferred.reportedSamples && now-deferred.value.lastReportUs>=30000000));
  if(due) {
    deferred.value.lastReportUs=now;
    deferred.reportedSamples=deferred.value.samples;
    snapshot=deferred.value;
  }
  portEXIT_CRITICAL(&deferred.mux);
  if(due)printSample(snapshot,now);
}
void eventSample(void*,esp_event_base_t,int32_t,void*) {observeSdk(sdkSamples[0]);}
void tcpipSample(void*) {
  observeSdk(sdkSamples[1]);
  tcpipPending.store(false);
}
void timerSample(void*) {observeSdk(sdkSamples[2]);}
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
      printSample(sample,now);
    }
    return;
  }
}
void beginSdk() {
  if(eventRegistered && timerStarted)return;
  const uint64_t now=uint64_t(esp_timer_get_time());
  if(beginAttempted && now-lastBeginUs<5000000)return;
  beginAttempted=true;lastBeginUs=now;
  if(!eventRegistered)eventRegistered=esp_event_handler_instance_register(
    ESP_EVENT_ANY_BASE,ESP_EVENT_ANY_ID,eventSample,nullptr,&eventInstance)==ESP_OK;
  if(!sdkTimer) {
    esp_timer_create_args_t args{};
    args.callback=timerSample;args.dispatch_method=ESP_TIMER_TASK;
    args.name="stack-audit";args.skip_unhandled_events=true;
    if(esp_timer_create(&args,&sdkTimer)!=ESP_OK)sdkTimer=nullptr;
  }
  if(sdkTimer && !timerStarted)timerStarted=esp_timer_start_periodic(sdkTimer,5000000)==ESP_OK;
}
void pollSdk() {
  beginSdk();
  uint64_t now=uint64_t(esp_timer_get_time());
  for(auto &sample:sdkSamples)reportSdk(sample,now);
  now=uint64_t(esp_timer_get_time()); // Exclude deferred UART work from scan timing.
  if(pollStarted && now-lastPollUs<5000000)return;
  pollStarted=true;lastPollUs=now;
  // These three tasks live for the scheduler's lifetime. Never use name lookups
  // or retained handles for driver tasks that mode changes can destroy.
  TaskHandle_t handle=stableIndex<2?xTaskGetIdleTaskHandleForCore(stableIndex):xTimerGetTimerDaemonTaskHandle();
  Sample &sample=stableSamples[stableIndex];
  stableIndex=(stableIndex+1)%3;
  if(handle) {
    const uint32_t unused=uint32_t(uxTaskGetStackHighWaterMark(handle));
    const uint32_t scanUs=uint32_t(uint64_t(esp_timer_get_time())-now);
    updateSample(sample,unused,scanUs);
    if(sample.samples==1 || now-sample.lastReportUs>=30000000) {
      sample.lastReportUs=now;printSample(sample,now);
    }
  }
  // Nonblocking SDK queue submission; never accumulate more than one callback.
  // The callback releases this flag after self-sampling on the TCP/IP task.
  if(!tcpipPending.exchange(true) && tcpip_try_callback(tcpipSample,nullptr)!=ERR_OK)tcpipPending.store(false);
}

}
#endif
