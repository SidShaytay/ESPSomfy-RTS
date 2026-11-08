#include "LocalUiRuntime.h"
#include "LocalUiAdapter.h"
#include "LocalUiCapture.h"
#include "LocalUiAuthorizationAdapter.h"
#include "../ui-adapter/Model.h"
#include <assert.h>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <map>
#include "DeviceOps.h"
#include "LoopQueue.h"
namespace LocalUi {
struct RuntimeTestAccess {
  static uint16_t *losePixels(Runtime &ui) {auto *pixels=ui.memory.pixels;ui.memory.pixels=nullptr;return pixels;}
  static void restorePixels(Runtime &ui,uint16_t *pixels) {ui.memory.pixels=pixels;}
  static const View &view(const Runtime &ui) {return ui.controller.view();}
  static bool noPresentation(const Runtime &ui) {
    return !ui.hasPresented && !ui.presentedSettingsValid && !ui.stopPresentedToken && !ui.validBands;
  }
};
}
bool hasPsram=true,allocateOK=true;
void *allocated=nullptr;
bool settingsAllocateOK=true;
std::map<void *,size_t> settingsAllocations;
uint64_t authTestNow=0;
static unsigned settingsSaves=0;
static uint64_t settingsRevision=1;
uint64_t deviceSettingsRevision(settings_section_t section) {return uint8_t(section)<5?settingsRevision:0;}
bool readDeviceSettings(settings_section_t section,SettingsSnapshotSink &sink) {
  for(uint8_t i=0;i<settingsFieldCount(section,false);i++) {
    const auto &field=*settingsFieldAt(section,i,false);
    settings_value_t value;value.kind=field.schema().kind;
    value.integer=field.lo;value.number=float(field.lo)/100;
    if(field.field==proto_field_t::hostname) strcpy(value.text,"lcd-host");
    if(field.field==proto_field_t::ntpServer) strcpy(value.text,"pool.ntp.org");
    if(field.field==proto_field_t::posixZone) strcpy(value.text,"UTC0");
    if(field.secret()) {value.kind='b';value.boolean=true;sink.field(field.presenceField(),value);}
    else sink.field(field.field,value);
  }
  return true;
}
settings_apply_result_t applyDeviceSettings(settings_section_t section,JsonObject request,uint32_t origin) {
  assert(section==settings_section_t::general && origin==0 && request["ssdpBroadcast"].as<bool>());
  ++settingsSaves;++settingsRevision;return {};
}
int testPins[64]={};uint32_t testNow=0;
SomfyShadeController somfy;
static LocalUi::Authorization authorization;
static LocalUi::View lastQuery;
static std::vector<shade_request_t> submitted;
static unsigned bands=0,presentations=0,locks=0;
static bool busBusy=false,captureProtected=true;
struct RecordingRequests {shade_request_result_t run(const shade_request_t &r){submitted.push_back(r);return shade_request_result_t::done;}} shadeRequests;
#include "../ui-adapter/LoopHarness.h"
#define ShadeRequests RecordingRequests
#define snapshotModel adapterSnapshotModel
#define markControlPresented adapterMarkControlPresented
#include "../../src/LocalUiAdapter.cpp"
#undef markControlPresented
#undef snapshotModel
#undef ShadeRequests
LocalUi::Model LocalUi::snapshotModel(bool running,const View &view){lastQuery=view;return adapterSnapshotModel(running,view);}
void LocalUi::markControlPresented(EntityRef ref,uint64_t token,bool allowed,uint32_t generation){++presentations;adapterMarkControlPresented(ref,token,allowed,generation);}
LocalUi::SecuritySnapshot LocalUi::pollAuthorization(){
  const bool allowed=authorization.allowed(testNow);
  return {authorization.configuredMode(),allowed,authorization.stopGraceAllowed(),authorization.generation(),authorization.configurationGeneration(),authorization.retryAfter(testNow),authorization.expiredSessionGeneration()};
}
void LocalUi::authorizationActivity(){authorization.activity(testNow);}
void LocalUi::lockAuthorization(){++locks;authorization.lock();}
LocalUi::AuthResult LocalUi::unlockAuthorization(Credentials &given){auto result=authorization.unlock(given,testNow);given.clear();return result;}
void LocalUi::setCaptureProtected(bool value){captureProtected=value;}
void LocalUi::capturePoll(const uint16_t *,bool,uint8_t,uint32_t){}
static bool send(uint8_t y,uint8_t rows,const uint8_t *data,size_t bytes){
  assert(data && rows && rows<=4 && y+rows<=170 && bytes==rows*640u);
  if(busBusy)return false;++bands;return true;
}
static void light(uint8_t value){assert(value==0 || value==4 || value==32);}
static void saveFrame(const char *directory,const char *name) {
  if(!directory) return;
  char path[512];snprintf(path,sizeof(path),"%s/%s.ppm",directory,name);
  FILE *file=fopen(path,"wb");assert(file && allocated);
  fprintf(file,"P6\n320 170\n255\n");
  const auto *pixels=static_cast<const uint16_t *>(allocated);
  for(size_t i=0;i<320*170;i++) {
    const auto p=pixels[i];const uint8_t rgb[]={uint8_t((p>>11)*255/31),uint8_t(((p>>5)&63)*255/63),uint8_t((p&31)*255/31)};
    fwrite(rgb,1,3,file);
  }
  fclose(file);
}
int main(int argc,char **argv){
  const char *preview=argc>1?argv[1]:nullptr;
  using namespace LocalUi;
  authorization.configure(AuthMode::none,"","");loopQueue.begin();
  auto &one=somfy.shades[13];one.id=7;one.address=700;one.shadeType=shade_types::shutter;one.bitLength=56;one.idle=false;strcpy(one.name,"Bedroom");
  auto &two=somfy.shades[31];two.id=22;two.address=2200;two.shadeType=shade_types::shutter;two.bitLength=56;two.idle=false;strcpy(two.name,"Bathroom");
  testPins[4]=testPins[5]=testPins[6]=1;
  StatusDisplay::Snapshot status;status.running=true;status.link=StatusDisplay::Link::connected;
  status.web=StatusDisplay::Web::listening;status.radio=StatusDisplay::Radio::ready;
  Runtime ui;ui.begin();assert(ui.failure()==GraphicsFailure::none && allocated);
  Runtime *active=&ui;
  int ab=3;
  auto tick=[&](bool pressed=false,unsigned dt=50){testNow+=dt;authTestNow=testNow;loopQueue.drain();active->tick(testNow,status,pressed,send,light);};
  auto idle=[&](unsigned duration=140){for(unsigned t=0;t<duration;t+=2)tick(false,2);};
  auto rotate=[&](int direction){
    const int middle=direction>0?(ab==3?1:2):(ab==3?2:1);
    testPins[4]=middle>>1;testPins[5]=middle&1;tick();
    ab=ab==3?0:3;testPins[4]=ab>>1;testPins[5]=ab&1;tick();
  };
  auto click=[&]{tick(true);tick(true);tick(false);tick(false);tick(false,2);};
  auto back=[&]{testPins[6]=0;tick();tick();testPins[6]=1;tick();tick();};
  busBusy=true;idle(20);assert(bands==0 && captureProtected);
  busBusy=false;idle();assert(bands==43 && !captureProtected);
  const auto unchanged=bands;idle();assert(bands==unchanged);
  rotate(1);idle();rotate(1);idle();click();idle(); // Home -> Devices
  assert(lastQuery.screen==Screen::catalogue);
  busBusy=true;click();idle(10);assert(lastQuery.screen==Screen::control);
  assert(presentations==0);click();assert(submitted.empty()); // never-presented STOP is blocked
  busBusy=false;idle();assert(presentations>0);click();
  assert(submitted.size()==1 && submitted.back().id==7 && submitted.back().command==somfy_commands::My);
  // Retain an already-visible STOP across estimate/action repainting, but do
  // not admit a never-presented UP highlight while SPI is blocked.
  busBusy=true;++one.value.currentPos;rotate(-1);click();assert(submitted.size()==1);
  click();assert(submitted.size()==2 && submitted.back().command==somfy_commands::My);
  // A different target cannot inherit the previous target's sticky STOP.
  back();rotate(1);click();assert(lastQuery.screen==Screen::catalogue && submitted.size()==2);
  busBusy=false;idle();busBusy=true;click();idle(10);click();assert(submitted.size()==2);
  assert(lastQuery.entity.id==22);
  // Context changes while locally authorized require a new presented selection.
  ++two.address;++two.configurationRevision;++somfy.configurationRevision;idle(10);click();assert(submitted.size()==2);
  busBusy=false;back();idle();rotate(-1);idle();click();idle();
  assert(lastQuery.screen==Screen::control && lastQuery.entity.id==7);
  authorization.configure(AuthMode::pin,"","!!!!");click();idle();
  assert(lastQuery.screen==Screen::unlock && captureProtected && submitted.size()==2);
  busBusy=true;back();assert(captureProtected);busBusy=false;idle();assert(!captureProtected);
  click();idle();assert(lastQuery.screen==Screen::unlock && captureProtected);
  rotate(1);for(unsigned i=0;i<4;i++)click();
  for(unsigned i=0;i<96;i++)rotate(1);
  click();idle();assert(lastQuery.screen==Screen::control && !captureProtected);
  assert(submitted.size()==2); // unlock does not replay the initial command
  // Existing authorized motion retains only the bounded 30 s expiry lease.
  testNow+=120000;ui.tick(testNow,status,false,send,light);assert(submitted.size()==2);
  click();assert(submitted.size()==3 && submitted.back().id==7);
  testNow+=31000;ui.tick(testNow,status,false,send,light);click();
  assert(submitted.size()==3);idle();assert(lastQuery.screen==Screen::unlock);
  back();one.idle=two.idle=true;idle();const auto beforeSleep=locks;
  testNow+=21000;ui.tick(testNow,status,false,send,light);assert(locks==beforeSleep+1);
  testNow+=100;ui.tick(testNow,status,false,send,light);assert(locks==beforeSleep+1);
  // Numeric movement requires the exact Apply choice to finish reaching LCD.
  authorization.configure(AuthMode::none,"","");one.idle=false;idle();
  rotate(1);idle();rotate(1);idle();click();idle();click();idle();
  assert(lastQuery.screen==Screen::control && lastQuery.entity.id==7);
  rotate(1);idle();rotate(1);idle();click();idle();
  assert(lastQuery.screen==Screen::actions);click();idle();
  assert(lastQuery.screen==Screen::value);const int expected=lastQuery.draftValue+1;
  rotate(1);idle();assert(lastQuery.draftValue==expected);
  const auto beforeDraft=submitted.size();click();idle();assert(lastQuery.valueChoice==2);
  busBusy=true;rotate(-1);click();assert(submitted.size()==beforeDraft);
  busBusy=false;idle();click();assert(submitted.size()==beforeDraft+1);
  assert(submitted.back().target==expected && submitted.back().type==shade_request_type_t::shade_command);
  // Back retracts an unsubmitted intent; no request or MY is generated.
  const auto beforePending=submitted.size();somfy.transceiver.room=false;
  idle();rotate(-1);idle();click();assert(localControlPending() && submitted.size()==beforePending);
  back();assert(!localControlPending() && submitted.size()==beforePending);
  somfy.transceiver.room=true;idle();click();idle();assert(lastQuery.screen==Screen::control);
  // STOP also retracts an idle target's queued UP, with zero idle MY.
  one.idle=true;idle();somfy.transceiver.room=false;rotate(-1);idle();click();
  assert(localControlPending() && submitted.size()==beforePending);idle();click();
  assert(!localControlPending() && submitted.size()==beforePending);
  somfy.transceiver.room=true;idle();click();assert(!localControlPending() && submitted.size()==beforePending);
  one.idle=two.idle=true; // Settings teardown case tests idle sleep.
  free(allocated);allocated=nullptr;
  authorization.configure(AuthMode::none,"","");
  Runtime settingsUi;active=&settingsUi;settingsUi.begin();ab=3;testPins[4]=testPins[5]=1;idle();
  const auto rfBeforeSettings=submitted.size();
  rotate(1);idle();for(unsigned i=0;i<3;i++) {rotate(1);idle();}click();idle(); // Home Settings
  assert(lastQuery.screen==Screen::settings && captureProtected);
  click();idle();saveFrame(preview,"runtime-settings-general"); // Actual form/runtime frame
  busBusy=true;rotate(1);click();assert(settingsSaves==0); // unseen field cannot open
  busBusy=false;idle();click();idle(); // Broadcast editor
  rotate(1);idle();click();idle(); // true -> numeric/boolean actions
  rotate(1);idle();click();idle(); // Apply to draft
  assert(Settings::runtimeMemory().used()>0 && settingsSaves==0);
  busBusy=true;for(unsigned i=0;i<3;i++) rotate(1);click();assert(settingsSaves==0); // unseen Save blocked
  busBusy=false;idle();click();idle();assert(settingsSaves==1 && captureProtected);saveFrame(preview,"runtime-settings-saved");
  assert(Settings::runtimeMemory().used()==0 && settingsAllocations.empty());
  back();idle();assert(lastQuery.screen==Screen::home && !captureProtected);
  click();idle();click();idle();rotate(1);idle();click();idle();rotate(1);idle();click();idle();rotate(1);idle();click();idle();
  assert(Settings::runtimeMemory().used()>0);
  testNow+=21000;authTestNow=testNow;settingsUi.tick(testNow,status,false,send,light);idle();
  assert(Settings::runtimeMemory().used()==0 && settingsAllocations.empty() && lastQuery.screen==Screen::status);
  assert(submitted.size()==rfBeforeSettings);
  // Inject loss after an active draft, rather than only startup allocation failure.
  click();idle();rotate(1);idle();for(unsigned i=0;i<3;i++){rotate(1);idle();}click();idle();
  assert(lastQuery.screen==Screen::settings);click();idle();rotate(1);idle();click();idle();
  rotate(1);idle();click();idle();rotate(1);idle();click();idle();
  assert(Settings::runtimeMemory().used()>0);
  auto *savedPixels=RuntimeTestAccess::losePixels(settingsUi);
  const auto locksBeforeLoss=locks;
  assert(!settingsUi.tick(testNow,status,true,send,light));
  assert(locks==locksBeforeLoss+1 && Settings::runtimeMemory().used()==0 && settingsAllocations.empty());
  assert(RuntimeTestAccess::view(settingsUi).screen==Screen::status && RuntimeTestAccess::noPresentation(settingsUi));
  for(unsigned i=0;i<3;i++) assert(!settingsUi.tick(++testNow,status,true,send,light));
  assert(locks==locksBeforeLoss+1); // No repeated authorization generation churn.
  RuntimeTestAccess::restorePixels(settingsUi,savedPixels);busBusy=true;
  assert(settingsUi.tick(testNow,status,true,send,light) && captureProtected);
  testNow+=1000;authTestNow=testNow;settingsUi.tick(testNow,status,true,send,light);
  assert(RuntimeTestAccess::view(settingsUi).screen==Screen::status);
  assert(RuntimeTestAccess::noPresentation(settingsUi) && captureProtected);
  busBusy=false;tick(false);idle();assert(!captureProtected);
  // An old presented target and its queued start cannot survive resource loss.
  rotate(1);idle();rotate(1);idle();click();idle();click();idle();
  assert(lastQuery.screen==Screen::control && lastQuery.openedToken);
  Command old{lastQuery.entity,lastQuery.openedToken,Action::up};
  old.authGeneration=pollAuthorization().generation;
  const auto beforeGraphicsCommand=submitted.size();
  somfy.transceiver.room=false;rotate(-1);idle();click();assert(localControlPending());
  const auto oldIntentToken=lastLocalToken;
  savedPixels=RuntimeTestAccess::losePixels(settingsUi);
  assert(!settingsUi.tick(testNow,status,false,send,light));
  assert(!localControlPending() && submitted.size()==beforeGraphicsCommand);
  old.authGeneration=pollAuthorization().generation;
  assert(dispatch(old)==DispatchResult::stale);
  RuntimeTestAccess::restorePixels(settingsUi,savedPixels);somfy.transceiver.room=true;
  idle();assert(RuntimeTestAccess::view(settingsUi).screen==Screen::status);
  assert(RuntimeTestAccess::view(settingsUi).notice==Notice::none && !captureProtected);
  rotate(1);idle();rotate(1);idle();click();idle();click();idle();
  assert(lastQuery.openedToken>old.token);
  old.authGeneration=pollAuthorization().generation;
  assert(dispatch(old)==DispatchResult::stale && submitted.size()==beforeGraphicsCommand);
  somfy.transceiver.room=false;rotate(-1);idle();click();
  assert(localControlPending() && lastLocalToken>oldIntentToken);
  back();somfy.transceiver.room=true;idle();assert(!localControlPending() && submitted.size()==beforeGraphicsCommand);
  free(allocated);allocated=nullptr;hasPsram=false;
  Runtime missing;missing.begin();assert(missing.failure()==GraphicsFailure::missing && !missing.tick(testNow,status,false,send,light));
  hasPsram=true;allocateOK=false;Runtime failed;failed.begin();assert(failed.failure()==GraphicsFailure::allocation && !failed.tick(testNow,status,false,send,light));
  puts("Production runtime/adapter: dynamic navigation, presented target/STOP, context change, unlock, capture transitions, 30 s lease, standalone settings/presentation/wipe and fallback passed.");
}
