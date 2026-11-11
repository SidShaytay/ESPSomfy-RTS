#pragma once
#include <cstdint>
namespace AuditSdkStubs {
inline int eventRegistrations=0,timerCreates=0,timerStarts=0,tcpipPosts=0;
inline int eventResult=0,timerCreateResult=0,timerStartResult=0,tcpipResult=0;
inline void (*eventCallback)(void*,const char*,int32_t,void*)=nullptr;
inline void (*timerCallback)(void*)=nullptr;
inline void (*tcpipCallback)(void*)=nullptr;
}
using esp_err_t=int;
constexpr int ESP_OK=0;
