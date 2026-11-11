#pragma once
#include <cstdint>
#include "sdk-stubs.h"
int64_t esp_timer_get_time();
using esp_timer_handle_t=void*;
enum esp_timer_dispatch_t{ESP_TIMER_TASK,ESP_TIMER_ISR};
struct esp_timer_create_args_t {void (*callback)(void*);void *arg;esp_timer_dispatch_t dispatch_method;const char *name;bool skip_unhandled_events;};
inline esp_err_t esp_timer_create(const esp_timer_create_args_t *args,esp_timer_handle_t *timer){
 ++AuditSdkStubs::timerCreates;
 if(args->dispatch_method!=ESP_TIMER_TASK || !args->skip_unhandled_events)return -1;
 if(AuditSdkStubs::timerCreateResult==0){AuditSdkStubs::timerCallback=args->callback;*timer=reinterpret_cast<void*>(1);}
 return AuditSdkStubs::timerCreateResult;
}
inline esp_err_t esp_timer_start_periodic(esp_timer_handle_t,uint64_t period){++AuditSdkStubs::timerStarts;return period==5000000?AuditSdkStubs::timerStartResult:-1;}
