#pragma once
#include <cstring>
#include <deque>
#include <string>
#include <cstdarg>
#include <cstdio>
#include <vector>
static uint32_t now=100;
uint32_t millis(){return now;}
static std::vector<std::string> logs;
int log_printf(const char *fmt,...){char b[300];va_list a;va_start(a,fmt);int n=vsnprintf(b,sizeof(b),fmt,a);va_end(a);logs.emplace_back(b);return n;}
struct SerialFake {std::deque<char> bytes;int available(){return int(bytes.size());}int read(){auto c=bytes.front();bytes.pop_front();return c;}} Serial;
constexpr int WL_CONNECTED=3;
struct WifiFake {unsigned disconnects=0;int status(){return WL_CONNECTED;}bool disconnect(bool off,bool erase){if(off||erase)throw "credentials/radio changed";++disconnects;return true;}} WiFi;
namespace LocalUi {
struct Security {bool allowed=true;uint32_t generation=1,configurationGeneration=2;};
static Security security;
static bool frozen=false;
Security pollAuthorization(){return security;}
bool localControlsFrozen(){return frozen;}
}
enum class GitUpdateRequest:uint8_t {accepted,busy,unavailable};
struct Candidate{};
struct Snapshot {Candidate candidate;unsigned phase=0,reason=0,progress=0;bool busy=false,compatible=false;};
struct UpdateFake {unsigned stages=0,cancels=0;Snapshot snapshot(){return {};}GitUpdateRequest requestAuditStage(Candidate){++stages;return GitUpdateRequest::accepted;}bool requestCancel(){++cancels;return true;}} gitUpdate;
static unsigned checks=0;
GitUpdateRequest requestLocalGitUpdateCheck(uint32_t a,uint32_t b,uint64_t &){if(a!=1||b!=2)throw "auth lost";++checks;return GitUpdateRequest::accepted;}
