#include "StackAudit.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>
static int64_t clockUs=0;
static unsigned unused=4096,reads=0;
static const char *task="loopTask";
static std::vector<std::string> logs;
const char *pcTaskGetName(void*){return task;}
unsigned uxTaskGetStackHighWaterMark(void*){++reads;clockUs+=7;return unused;}
int64_t esp_timer_get_time(){return clockUs;}
int log_printf(const char *format,...){char buffer[400];va_list args;va_start(args,format);const int size=vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);logs.emplace_back(buffer);return size;}
static void sample(){StackAudit::Scope scope;}
int main(){
 sample();assert(reads==1 && logs.size()==1);
 assert(logs.back()=="STACK_AUDIT schema=1 role=loop uptimeMs=0 minimumUnusedBytes=4096 samples=1 maxScanUs=7\n");
 assert(logs.back().find("role=loop")!=std::string::npos);
 assert(logs.back().find("minimumUnusedBytes=4096")!=std::string::npos);
 assert(logs.back().find("maxScanUs=7")!=std::string::npos);
 unused=100;clockUs=100;sample();assert(reads==1); // No scan in a hot loop.
 clockUs=5000000;sample();assert(reads==2 && logs.size()==1);
 unused=3000;clockUs=30000000;sample();assert(logs.size()==2);
 assert(logs.back().find("minimumUnusedBytes=100")!=std::string::npos);
 assert(logs.back().find("samples=3")!=std::string::npos);
 task="private-device-name";sample();assert(reads==3 && logs.size()==2);
 const char *tasks[]={"arduino_events","async_udp","async_tcp","radio_tx","github-update"};
 const char *roles[]={"network","udp","web","radio","updater"};
 for(int i=0;i<5;++i){task=tasks[i];sample();assert(logs.back().find(std::string("role=")+roles[i])!=std::string::npos);}
 for(const auto &line:logs)assert(line.find("private-device-name")==std::string::npos);
 puts("Stack self-sampling rate, minima, timing and role allowlist passed");
}
