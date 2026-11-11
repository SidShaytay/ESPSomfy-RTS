#pragma once
#include <cstdint>
#include <mutex>
using portMUX_TYPE=std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
inline thread_local int auditCriticalDepth=0;
inline void auditEnter(portMUX_TYPE *mux){mux->lock();++auditCriticalDepth;}
inline void auditExit(portMUX_TYPE *mux){--auditCriticalDepth;mux->unlock();}
#define portENTER_CRITICAL(m) auditEnter(m)
#define portEXIT_CRITICAL(m) auditExit(m)
