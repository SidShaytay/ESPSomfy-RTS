#include "StackAudit.h"
#include "sdk-stubs.h"
#include <freertos/FreeRTOS.h>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>
static int64_t clockUs=0;
static unsigned unused=2048;
static const char *task="loopTask";
static std::vector<void*> scanned;
static std::vector<std::string> logs;
static bool simulateLogCost=false;
const char *pcTaskGetName(void*){return task;}
unsigned uxTaskGetStackHighWaterMark(void *handle){assert(auditCriticalDepth==0);scanned.push_back(handle);clockUs+=7;return unused;}
int64_t esp_timer_get_time(){return clockUs;}
int log_printf(const char *format,...){assert(auditCriticalDepth==0);assert(std::string(task)=="loopTask");char b[400];va_list args;va_start(args,format);int n=vsnprintf(b,sizeof(b),format,args);va_end(args);logs.emplace_back(b);if(simulateLogCost)clockUs+=1000;return n;}
static bool role(const char *name){for(const auto &line:logs)if(line.find(std::string("role=")+name+" ")!=std::string::npos)return true;return false;}
int main(){
 using namespace AuditSdkStubs;
 // Startup failures retry without per-loop allocations or duplicate handlers.
 eventResult=-1;timerCreateResult=-1;StackAudit::beginSdk();
 assert(eventRegistrations==1 && timerCreates==1);
 StackAudit::beginSdk();assert(eventRegistrations==1 && timerCreates==1);
 eventResult=0;timerCreateResult=0;clockUs=5000000;StackAudit::beginSdk();
 assert(eventRegistrations==2 && timerCreates==2 && timerStarts==1);
 StackAudit::beginSdk();assert(eventRegistrations==2 && timerCreates==2);
 // At most one scheduler-lifetime foreign task per 5s, rotated fairly.
 StackAudit::pollSdk();assert(scanned.size()==1 && scanned.back()==reinterpret_cast<void*>(1));
 StackAudit::pollSdk();assert(scanned.size()==1);
 assert(tcpipPosts==1 && tcpipCallback);
 // A blocked tcpip callback cannot accumulate an unbounded callback backlog.
 clockUs=10000000;StackAudit::pollSdk();assert(tcpipPosts==1 && scanned.back()==reinterpret_cast<void*>(2));
 auto records=logs.size();
 auto previousReads=scanned.size();eventCallback(nullptr,nullptr,0,nullptr);assert(scanned.size()==previousReads);
 task="sys_evt";eventCallback(nullptr,nullptr,0,nullptr);
 task="tiT";tcpipCallback(nullptr);
 task="esp_timer";timerCallback(nullptr);
 assert(logs.size()==records); // High-priority SDK contexts never print.
 assert(scanned[scanned.size()-1]==nullptr);
 task="loopTask";StackAudit::pollSdk();
 assert(role("system-events") && role("tcpip") && role("esp-timer"));
 auto count=scanned.size();task="sys_evt";eventCallback(nullptr,nullptr,0,nullptr);assert(scanned.size()==count);
 task="loopTask";tcpipResult=-1;clockUs=15000000;StackAudit::pollSdk();
 assert(scanned.back()==reinterpret_cast<void*>(3) && tcpipPosts==2);
 clockUs=20000000;StackAudit::pollSdk();assert(scanned.back()==reinterpret_cast<void*>(1) && tcpipPosts==3);
 assert(role("idle0") && role("idle1") && role("timer-daemon"));
 // Report throttling must not hide a lower SDK minimum after it is scanned.
 task="sys_evt";unused=80;eventCallback(nullptr,nullptr,0,nullptr);
 task="loopTask";count=logs.size();StackAudit::pollSdk();assert(logs.size()==count);
 simulateLogCost=true;clockUs=41000000;StackAudit::pollSdk();
 bool minimum=false;for(const auto &line:logs)if(line.find("role=system-events ")!=std::string::npos && line.find("minimumUnusedBytes=80")!=std::string::npos)minimum=true;
 assert(minimum);
 for(const auto &line:logs)assert(line.find("maxScanUs=7")!=std::string::npos);
 puts("SDK lifetime handles, bounded scans, callback backpressure, deferred logs and startup retries passed");
}
