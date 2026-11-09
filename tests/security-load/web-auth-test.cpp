#include "ConfigSettings.h"
#include "LoginThrottle.h"
#include <nvs.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
struct { SecuritySettings Security; } settings;
static constexpr int HTTP_OPTIONS=1;
static constexpr size_t API_TOKEN_SIZE=128;
static const char *_encoding_json="application/json";
static char g_async_content[512];
static LoginThrottle loginThrottle;
static uint32_t loginKey(IPAddress){return 1;}
static uint64_t loginNow(){return 100;}
#define ESP_LOGW(...) ((void)0)
struct AsyncWebServerResponse {void addHeader(const char *,String){}};
struct AsyncWebHeader {String value()const{return String("");}};
using AsyncWebParameter=AsyncWebHeader;
static bool originMatchesHost(const char *,const char *){return true;} // Origin policy is tested separately.
struct Client {IPAddress remoteIP(){return IPAddress(127,0,0,1);}};
struct AsyncWebServerRequest {
 bool upload=false;
 void setAttribute(const char *,bool value){upload=value;}bool getAttribute(const char *,bool fallback){return upload;}
 const AsyncWebParameter *getParam(const char *){return nullptr;}
 int status=0;String body;Client peer;int method(){return 0;}Client *client(){return &peer;}
 const AsyncWebHeader *getHeader(const char *){return nullptr;}
 void send(int status,const char *mime="",const char *data=""){this->status=status;body=String(data);}
 AsyncWebServerResponse *beginResponse(int,const char *,const char *){static AsyncWebServerResponse response;return &response;}
 void send(AsyncWebServerResponse *){status=429;}
};
static bool asyncHasParam(AsyncWebServerRequest *,const char *){return false;}
static String asyncParam(AsyncWebServerRequest *,const char *){return String("");}
static bool tokenMatches(const char *a,const char *b){return !strcmp(a,b);}
struct Web {
 unsigned generated=0;
 bool tokenAccepted(AsyncWebServerRequest *,const char *);
 bool isAuthenticated(AsyncWebServerRequest *);
 void handleLogin(AsyncWebServerRequest *,JsonVariant &);
 bool createAPIPinToken(IPAddress,const char *pin,char *out){++generated;snprintf(out,API_TOKEN_SIZE,"pin:%s",pin);return true;}
 bool createAPIPasswordToken(IPAddress,const char *user,const char *password,char *out){++generated;snprintf(out,API_TOKEN_SIZE,"user:%s:%s",user,password);return true;}
 bool createAPIToken(IPAddress ip,char *out){
  if(settings.Security.type==security_types::PinEntry)return createAPIPinToken(ip,settings.Security.pin,out);
  if(settings.Security.type==security_types::Password)return createAPIPasswordToken(ip,settings.Security.username,settings.Security.password,out);
  ++generated;strcpy(out,"open");return true;
 }
};
Web webServer;
#include "production-web-auth.inc"
int main(){
 Web &web=webServer;AsyncWebServerRequest request;JsonDocument doc;auto json=doc.as<JsonVariant>();
 assert(!web.tokenAccepted(&request,nullptr));assert(!web.isAuthenticated(&request));
 assert(!authorized(&request));assert(!uploadAllowed(&request,0));assert(!uploadAllowed(&request,100));
 web.handleLogin(&request,json);assert(request.status==503 && !web.generated);
 // A known first boot remains openly configurable; generic failed reads do not.
 FakeNvs::reset();assert(settings.Security.load());assert(web.isAuthenticated(&request));
 web.handleLogin(&request,json);assert(request.status==200 && strstr(request.body.c_str(),"apiKey"));
 for(int mode=1;mode<=2;++mode){
  FakeNvs::reset();nvs_handle_t h;nvs_open("SEC",NVS_READWRITE,&h);nvs_set_i8(h,"type",mode);nvs_close(h);
  assert(!settings.Security.load());const auto before=web.generated;
  assert(!web.isAuthenticated(&request));assert(!web.tokenAccepted(&request,"open"));
  web.handleLogin(&request,json);assert(request.status==503 && web.generated==before);
 }
 for(int mode=1;mode<=2;++mode){
  FakeNvs::reset();nvs_handle_t h;nvs_open("SEC",NVS_READWRITE,&h);nvs_set_i8(h,"type",mode);
  nvs_set_str(h,"pin","aB!9");nvs_set_str(h,"username","user");nvs_set_str(h,"password","password");nvs_close(h);
  assert(settings.Security.load());doc.clear();
  if(mode==1)doc["pin"]="aB!9";else {doc["username"]="user";doc["password"]="password";}
  json=doc.as<JsonVariant>();web.handleLogin(&request,json);assert(request.status==200);
  doc.clear();json=doc.as<JsonVariant>();web.handleLogin(&request,json);assert(request.status==401);
 }
 FakeNvs::reset();FakeNvs::failOpen=1;assert(!settings.Security.load());
 assert(!web.isAuthenticated(&request));web.handleLogin(&request,json);assert(request.status==503);
 puts("Actual SecuritySettings load and web authentication reject unknown/read-failed/blank-credential state; known first boot remains usable");
}
