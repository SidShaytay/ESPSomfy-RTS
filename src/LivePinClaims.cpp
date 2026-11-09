#include "LivePinClaims.h"
#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#else
#include <mutex>
#endif
namespace {
#if defined(ESP_PLATFORM)
portMUX_TYPE pinClaimsMutex=portMUX_INITIALIZER_UNLOCKED;
struct Guard {Guard(){portENTER_CRITICAL(&pinClaimsMutex);}~Guard(){portEXIT_CRITICAL(&pinClaimsMutex);}};
#else
std::mutex pinClaimsMutex;
struct Guard {std::lock_guard<std::mutex> lock;Guard():lock(pinClaimsMutex){}};
#endif
}
void LivePinClaims::retain(uint64_t claims){if(!claims)return;Guard guard;pins|=claims;}
uint64_t LivePinClaims::snapshot()const{Guard guard;return pins;}
