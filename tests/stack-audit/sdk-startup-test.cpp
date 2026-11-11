#include "StackAudit.h"
#include "sdk-stubs.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
static int64_t clockUs=0;
const char *pcTaskGetName(void*){return "loopTask";}
unsigned uxTaskGetStackHighWaterMark(void*){return 100;}
int64_t esp_timer_get_time(){return clockUs;}
int log_printf(const char*,...){return 0;}
int main(){
 using namespace AuditSdkStubs;
 timerStartResult=-1;StackAudit::beginSdk();
 assert(timerCreates==1 && timerStarts==1 && eventRegistrations==1);
 StackAudit::beginSdk();assert(timerStarts==1);
 clockUs=5000000;StackAudit::beginSdk();
 assert(timerCreates==1 && timerStarts==2 && eventRegistrations==1);
 timerStartResult=0;clockUs=10000000;StackAudit::beginSdk();
 assert(timerCreates==1 && timerStarts==3 && eventRegistrations==1);
 clockUs=15000000;StackAudit::beginSdk();
 assert(timerCreates==1 && timerStarts==3 && eventRegistrations==1);
 puts("SDK timer-start failures retain one allocation and retry at bounded intervals");
}
