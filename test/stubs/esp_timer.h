#pragma once
#include <stdint.h>
inline int64_t stubEspTimerMicros=0;
inline int64_t esp_timer_get_time(){return stubEspTimerMicros;}
