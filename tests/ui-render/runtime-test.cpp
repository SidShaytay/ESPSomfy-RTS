#include "LocalUiRuntime.h"
#include "LocalUiAdapter.h"
#include "LocalUiCapture.h"
#include "LocalUiAuthorizationAdapter.h"
#include "../ui-adapter/Model.h"
#include <assert.h>
#include <vector>
#include <string.h>
#include <stdio.h>
bool hasPsram=true,allocateOK=true;
void *allocated=nullptr;
int testPins[64]={};uint32_t testNow=0;
SomfyShadeController somfy;
static LocalUi::Authorization authorization;
static LocalUi::View lastQuery;
static std::vector<shade_request_t> submitted;
static unsigned bands=0,presentations=0,locks=0;
static bool busBusy=false,captureProtected=true;
struct RecordingRequests {shade_request_result_t run(const shade_request_t &r){submitted.push_back(r);return shade_request_result_t::done;}} shadeRequests;
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
  return {authorization.configuredMode(),allowed,authorization.stopGraceAllowed(),authorization.generation(),authorization.configurationGeneration(),authorization.retryAfter(testNow)};
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
int main(){
  using namespace LocalUi;
  authorization.configure(AuthMode::none,"","");
  auto &one=somfy.shades[13];one.id=7;one.address=700;one.shadeType=shade_types::shutter;one.bitLength=56;one.idle=false;strcpy(one.name,"Bedroom");
  auto &two=somfy.shades[31];two.id=22;two.address=2200;two.shadeType=shade_types::shutter;two.bitLength=56;two.idle=false;strcpy(two.name,"Bathroom");
  testPins[4]=testPins[5]=testPins[6]=1;
  StatusDisplay::Snapshot status;status.running=true;status.link=StatusDisplay::Link::connected;
  status.web=StatusDisplay::Web::listening;status.radio=StatusDisplay::Radio::ready;
  Runtime ui;ui.begin();assert(ui.failure()==GraphicsFailure::none && allocated);
  int ab=3;
  auto tick=[&](bool pressed=false,unsigned dt=50){testNow+=dt;ui.tick(testNow,status,pressed,send,light);};
  auto idle=[&](unsigned duration=140){for(unsigned t=0;t<duration;t+=2)tick(false,2);};
  auto rotate=[&](int direction){
    const int middle=direction>0?(ab==3?1:2):(ab==3?2:1);
    testPins[4]=middle>>1;testPins[5]=middle&1;tick();
    ab=ab==3?0:3;testPins[4]=ab>>1;testPins[5]=ab&1;tick();
  };
  auto click=[&]{tick(true);tick(true);tick(false);tick(false);};
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
  free(allocated);allocated=nullptr;hasPsram=false;
  Runtime missing;missing.begin();assert(missing.failure()==GraphicsFailure::missing && !missing.tick(testNow,status,false,send,light));
  hasPsram=true;allocateOK=false;Runtime failed;failed.begin();assert(failed.failure()==GraphicsFailure::allocation && !failed.tick(testNow,status,false,send,light));
  puts("Production runtime/adapter: dynamic navigation, presented target/STOP, context change, unlock, capture transitions, 30 s lease and fallback passed.");
}
