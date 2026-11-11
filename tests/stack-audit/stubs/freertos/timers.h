#pragma once
#include "task.h"
inline TaskHandle_t xTimerGetTimerDaemonTaskHandle(){return reinterpret_cast<void*>(uintptr_t(3));}
