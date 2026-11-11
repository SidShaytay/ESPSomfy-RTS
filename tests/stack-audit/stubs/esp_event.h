#pragma once
#include "sdk-stubs.h"
using esp_event_base_t=const char*;
using esp_event_handler_instance_t=void*;
constexpr esp_event_base_t ESP_EVENT_ANY_BASE=nullptr;
constexpr int32_t ESP_EVENT_ANY_ID=-1;
inline esp_err_t esp_event_handler_instance_register(esp_event_base_t base,int32_t id,void (*callback)(void*,esp_event_base_t,int32_t,void*),void*,esp_event_handler_instance_t *instance){
 ++AuditSdkStubs::eventRegistrations;
 if(base || id!=-1)return -1;
 if(AuditSdkStubs::eventResult==0){AuditSdkStubs::eventCallback=callback;*instance=reinterpret_cast<void*>(1);}
 return AuditSdkStubs::eventResult;
}
