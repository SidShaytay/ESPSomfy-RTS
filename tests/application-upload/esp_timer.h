#pragma once
#include <stdint.h>
inline uint64_t manualClockMs=0;
inline int64_t esp_timer_get_time(){return int64_t(manualClockMs*1000);}
