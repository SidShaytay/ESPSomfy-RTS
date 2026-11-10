#define SOMFY_H
#define configsettings_h
#define PROTO_SOCKET_H
#include "FrequencyScanService.h"
#include "FrequencyScanAdmission.h"
#include "ProtoSettings.h"
#include "LocalUiAuthorizationAdapter.h"
#include "LoopQueue.h"
#include "SomfyFrame.h"
#include "esp_timer.h"
#include <cassert>
static bool settingsTrusted=true;
static uint64_t sec=1,radioHistory=1;
uint64_t securityConfigurationRevision(){return sec;}
uint64_t deviceSettingsRevision(settings_section_t){return radioHistory;}
static bool pending=false;bool restartPending(){return pending;}
static bool scan=false;bool frequencyScanActive(){return scan;}
struct SomfyShadeController {
 struct {bool trusted=true;bool usable(){return trusted;}} modelLoad;
 struct Shade {uint8_t id=255;bool idle=true;uint8_t getShadeId(){return id;}bool idleForAdministration(){return idle;}}shades[32];
 struct Group {uint8_t id=255;int8_t direction=0;somfy_frame_t lastFrame;uint8_t linkedShades[32]{};uint8_t getGroupId(){return id;}}groups[16];
 Shade *getShadeById(uint8_t id){for(auto &s:shades)if(s.id==id&&id!=255)return &s;return nullptr;}
 struct Radio {
  struct {struct {bool usable(){return true;}}loadStatus;}config;
  unsigned begins=0,stops=0;bool uncertain=false;
  bool counterEditUncertain(){return uncertain;}
  bool beginFrequencyScan(){++begins;if(scan)return false;scan=true;return true;}
  void endFrequencyScan(){++stops;}
  bool readFrequencyScan(FrequencyScan::Sample &out){out={433.42f,433.43f,-60,-55};return true;}
 }transceiver;
}somfy;
struct ConfigSettings {bool settingsUsable(){return settingsTrusted;}} settings;
struct ProtoSocket {bool acceptsClientMutation(uint32_t client)const{return client==7;}}protoSocket;
LoopQueue loopQueue;uint32_t LoopQueue::pending()const{return 0;}
namespace LocalUi {
static SecuritySnapshot auth{AuthMode::none,true,false,1,1};
SecuritySnapshot pollAuthorization(){return auth;}
bool localControlsFrozen(){return false;}
}
#include "../../src/FrequencyScanService.cpp"
#include <cstdio>
int main(){
 using namespace FrequencyScan;FrequencyScanToken first;
 assert(begin(webSource(7),first)&&scan&&gitUpdate.busy());
 GitUpdateOwnerToken foreign;assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::configuration,foreign));
 assert(!stop(webSource(8),first)&&!somfy.transceiver.stops);
 poll();assert(status().sampleValid);
 ++radioHistory;poll();assert(somfy.transceiver.stops==1&&gitUpdate.busy());
 scan=false;poll();assert(!gitUpdate.busy()&&status().phase==Phase::review);
 float value=99;assert(!peak(webSource(7),first,value)&&value==99);
 FrequencyScanToken next;assert(begin(localSource(1,1),next));assert(!next.same(first));
 assert(!stop(webSource(7),first));LocalUi::auth.allowed=false;poll();assert(somfy.transceiver.stops==2&&gitUpdate.busy());
 scan=false;poll();assert(!gitUpdate.busy());
 LocalUi::auth.allowed=true;settingsTrusted=false;FrequencyScanToken untouched{8,9};
 assert(!begin(webSource(7),untouched)&&untouched.bootSession==8&&!gitUpdate.busy());
 settingsTrusted=true;somfy.shades[0].id=7;somfy.shades[0].idle=false;
 assert(!begin(webSource(7),untouched)&&!gitUpdate.busy());
 puts("Actual scan service: shared owner, source/client/session/history revocation, deferred restoration and no stale Stop/peak passed");
}
