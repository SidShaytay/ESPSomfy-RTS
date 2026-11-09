#pragma once
#include <stdint.h>
#include <functional>
inline std::function<void()> duringClock;
inline uint64_t manualClockMs=0;
inline int64_t esp_timer_get_time(){if(duringClock)duringClock();return int64_t(manualClockMs*1000);}
