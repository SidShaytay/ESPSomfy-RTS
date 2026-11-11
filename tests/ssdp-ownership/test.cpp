#include <cassert>
#include <cstdio>
#include <strings.h>
#include <thread>
#include <atomic>
#include "SSDP.h"
#include "SSDPRequestLine.h"
static thread_local bool callback=false;
static unsigned long tick=100;
unsigned long millis(){return tick;}
static long randomChoice=0;
long random(long low,long high){return high>low?(randomChoice>=high?high-1:randomChoice<low?low:randomChoice):0;}
static void _trim(char *s){size_t n=strlen(s);while(n&&(s[n-1]==' '||s[n-1]=='"'))s[--n]=0;size_t i=0;while(s[i]==' '||s[i]=='"')++i;memmove(s,s+i,strlen(s+i)+1);}
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGD(...) ((void)0)
#define SSDP_PORT 1900
#define SSDP_BUFFER_SIZE 64
#define SSDP_MULTICAST_ADDR 239,255,255,250
struct Timestamp{static unsigned long epoch(){return 10000;}};
struct {struct{int major=4,minor=1,build=0;}fwVersion;}settings;
struct Sent {IPAddress address;uint16_t port;response_types_t type;std::string target;};
static std::vector<Sent> sent;
UPNPDeviceType::UPNPDeviceType(){memset(this,0,sizeof(*this));isActive=true;strcpy(deviceType,"urn:test:device:1");strcpy(uuid,"uuid:test");}
void SSDPClass::_sendNotify(){assert(!callback);}
void SSDPClass::_sendByeBye(){assert(!callback);}
void SSDPClass::_sendResponse(IPAddress a,uint16_t p,UPNPDeviceType*,const char *st,response_types_t type){assert(!callback&&"UDP callback must never send");sent.push_back({a,p,type,st});}
UPNPDeviceType *SSDPClass::findDeviceByType(char *s){assert(!callback);return strcmp(s,deviceTypes[0].deviceType)==0?&deviceTypes[0]:nullptr;}
UPNPDeviceType *SSDPClass::findDeviceByUUID(char *s){assert(!callback);return strcmp(s,deviceTypes[0].uuid)==0?&deviceTypes[0]:nullptr;}
#include "production-ssdp.inc"
struct TestSSDP:SSDPClass {
 uint32_t epoch(){return _requestEpoch();}
 bool stale(uint32_t e){ssdp_request_t r;r.address=IPAddress(1);strcpy(r.st,"upnp:rootdevice");return _enqueueRequest(r,e);}
 void disconnect(){_server.open=false;}
 void input(const char *target,bool multicast=false,uint8_t mx=0,IPAddress addr=IPAddress(1),uint16_t port=1234){
  std::string text="M-SEARCH * HTTP/1.1\r\nHOST: ";text+=multicast?"239.255.255.250:1900":"192.0.2.1:1900";
  text+="\r\nMAN: \"ssdp:discover\"\r\nST: ";text+=target;text+="\r\nMX: "+std::to_string(mx)+"\r\n\r\n";
  AsyncUDPPacket packet(text,addr,port);callback=true;_processRequest(packet);callback=false;
 }
};
int main(){
 TestSSDP s;assert(s.begin());
 s.input("upnp:rootdevice");assert(sent.empty());s.loop();assert(sent.size()==1);
 sent.clear();s.input("uuid:test");s.input("urn:test:device:1");assert(sent.empty());
 for(int i=0;i<5;++i){++tick;s.loop();}assert(sent.size()==2);
 assert(sent[0].target=="uuid:test" && sent[1].target=="urn:test:device:1");
 sent.clear();s.input("ssdp:all",true,2,IPAddress(10),5678);
 for(int i=0;i<4;++i){++tick;s.loop();}assert(sent.size()==3);
 assert(sent[0].address==IPAddress(10) && sent[0].port==5678);
 assert(sent[0].type==root && sent[1].type==uuid && sent[2].type==deviceType);
 sent.clear();s.input("upnp:rootdevice",true,2);s.loop();assert(sent.size()==1);
 assert(sent[0].address==IPAddress(239,255,255,250) && sent[0].port==1900);
 // Incoming queue is bounded and retains the earliest requests.
 sent.clear();for(unsigned i=0;i<SSDP_QUEUE_SIZE+1;++i)s.input("upnp:rootdevice",false,0,IPAddress(20+i));
 for(int i=0;i<10;++i)s.loop();assert(sent.size()==SSDP_QUEUE_SIZE);
 assert(sent.back().address==IPAddress(24));
 sent.clear();s.input("upnp:rootdevice");s.input("upnp:rootdevice");s.loop();s.loop();assert(sent.size()==1);
 // End/restart clears both incoming and delayed responses, including a producer
 // that parsed in an earlier generation and submits after restart.
 sent.clear();const auto old=s.epoch();s.input("upnp:rootdevice");s.end();assert(s.begin());assert(!s.stale(old));s.loop();assert(sent.empty());
 s.input("ssdp:all",true,2);s.loop();assert(sent.size()==1);sent.clear();s.end();assert(s.begin());s.loop();assert(sent.empty());
 s.input("upnp:rootdevice");s.disconnect();s.loop();assert(s.begin());s.loop();assert(sent.empty());
 // Stale request and delayed-reply deadlines are enforced across millis wrap.
 s.input("upnp:rootdevice");tick+=1001;s.loop();assert(sent.empty());
 s.input("ssdp:all",true,2);s.loop();assert(sent.size()==1);sent.clear();tick+=2001;s.loop();s.loop();assert(sent.empty());
 tick=0xfffffff0u;s.input("upnp:rootdevice");tick=uint32_t(tick+100);s.loop();assert(sent.size()==1);
 // Queue residence cannot extend the request's original MX deadline, even
 // when the random delay is nonzero and millis wraps before dispatch.
 randomChoice=1;
 for(uint32_t start:{100u,0xffffff00u}) {
   s.end();assert(s.begin());sent.clear();tick=start;
   s.input("upnp:rootdevice",true,3);tick=uint32_t(start+2900);s.loop();assert(sent.empty());
   tick=uint32_t(start+3000);s.loop();assert(sent.size()==1);
   s.end();assert(s.begin());sent.clear();tick=start;
   s.input("upnp:rootdevice",true,3);tick=uint32_t(start+2900);s.loop();assert(sent.empty());
   tick=uint32_t(start+4000);s.loop();assert(sent.empty());
 }
 // A fresh matching request replaces an expired delayed entry instead of
 // being coalesced into the entry that this same loop pass will discard.
 s.end();assert(s.begin());sent.clear();tick=100;
 s.input("upnp:rootdevice",true,3);s.loop();assert(sent.empty());
 tick=3200;s.input("upnp:rootdevice",true,3);s.loop();assert(sent.empty());
 tick=4200;s.loop();assert(sent.size()==1);
 randomChoice=0;
 // Exercise producer/consumer handoff concurrently. All sending remains on
 // this consumer thread; copied datagrams cannot retain producer storage.
 sent.clear();tick=100;std::atomic<bool> done{false};
 std::thread producer([&]{for(unsigned i=0;i<2000;++i)s.input("upnp:rootdevice",false,0,IPAddress(100+i));done.store(true);});
 while(!done.load())s.loop();producer.join();for(int i=0;i<10;++i)s.loop();
 assert(!sent.empty());for(const auto &reply:sent)assert(reply.target=="upnp:rootdevice" && reply.type==root);
 puts("SSDP callback ownership, destinations, overflow, duplicate, deadlines, lifecycle and concurrent handoff regressions passed");
}
