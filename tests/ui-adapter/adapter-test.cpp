// Real loop adapter and model-mutation wrapper with a bounded fake model/sink.
#include "Model.h"
#include "LocalUiAdapter.h"
#include "LocalUiAuthorizationAdapter.h"
#include "ExplicitCounter.h"
#include <assert.h>
#include <vector>
#include <stdio.h>
SomfyShadeController somfy;
// This adapter/context fixture replaces only checked storage admission. The
// actual domain/NVS/owner/queue path is exercised by test-explicit-counter.sh.
CounterDomainResult correctExplicitCounter(uint8_t id,uint16_t code) {
  auto *shade=somfy.getShadeById(id);if(!shade)return {CounterDomainState::missing};
  ++somfy.configurationRevision;++shade->configurationRevision;shade->setRollingCode(code);
  return {CounterDomainState::saved,true,{ModelCommitState::saved}};
}
uint64_t authTestNow=0;
static LocalUi::SecuritySnapshot security{LocalUi::AuthMode::none,true,false,2,3,0};
LocalUi::SecuritySnapshot LocalUi::pollAuthorization(){return security;}
void LocalUi::lockAuthorization(){++security.generation;security.stopGrace=false;security.expiredSessionGeneration=0;if(security.mode!=LocalUi::AuthMode::none)security.allowed=false;}
static std::vector<shade_request_t> submitted;
static unsigned refuseAt=0;
struct RecordingRequests {
  shade_request_result_t run(const shade_request_t &r){
    submitted.push_back(r);
    return refuseAt && submitted.size()==refuseAt?shade_request_result_t::radio_refused:shade_request_result_t::done;
  }
} shadeRequests;
#include "LoopHarness.h"
#define ShadeRequests RecordingRequests
#define dispatch queuedDispatch
#include "../../src/LocalUiAdapter.cpp"
#undef dispatch
#undef ShadeRequests
LocalUi::DispatchResult LocalUi::dispatch(const Command &cmd) {
  const auto admitted=queuedDispatch(cmd);
  if(admitted!=DispatchResult::queued)return admitted;
  loopQueue.drain();DispatchResult completed;assert(takeLocalControlResult(completed));return completed;
}
#include "../../src/SomfyShadeOps.cpp"
// This control-adapter fixture never exposes the reviewed LCD correction;
// its complete real domain/witness path is tested by test-explicit-counter.
CounterDomainResult correctReviewedPrimaryCounter(uint8_t,uint16_t,const PrimaryCounterReview &,CounterCorrectionWitness &) {assert(false);return {};}
#include "../../src/CounterRuntime.cpp"
using namespace LocalUi;
static SomfyShade &device(uint8_t slot,uint8_t id,uint32_t address){
  auto &s=somfy.shades[slot];s.id=id;s.address=address;s.shadeType=shade_types::shutter;s.bitLength=56;
  return s;
}
static Command command(const View &view,Action action){return {view.entity,view.openedToken,action,-1,0,security.allowed?security.generation:security.expiredSessionGeneration};}
static Model open(View &view,EntityRef ref,bool authorized=true){
  view.entity=ref;view.screen=Screen::control;view.openedToken=0;
  auto m=snapshotModel(true,view);view.openedToken=m.target.token;
  view.authorizedSelection=authorized;view.selectionAuthGeneration=security.generation;
  markControlPresented(ref,view.openedToken,authorized,security.generation);
  return snapshotModel(true,view);
}
int main(){
  loopQueue.begin();
  View view;
  const auto boot=snapshotModel(false,view);
  assert(!boot.authorized && boot.authMode==AuthMode::unavailable && !boot.pageValid);
  auto &one=device(13,7,700);auto &two=device(31,22,2200);
  one.roomId=4; somfy.rooms[5].roomId=4; strcpy(somfy.rooms[5].name,"Bedroom");
  auto page=snapshotModel(true,view);
  assert(page.page.total==2 && page.page.rows[0].id==7 && page.page.rows[1].id==22);
  view.room=4;page=snapshotModel(true,view);assert(page.page.total==1 && page.page.rows[0].id==7);
  view.room=allRooms;
  auto model=snapshotModel(true,view);const auto stablePage=model.pageToken;
  one.value.currentPos=36;model=snapshotModel(true,view);assert(model.pageToken==stablePage);
  view.entity={EntityKind::device,7}; model=snapshotModel(true,view);
  const auto lightRequests=submitted.size();const auto lightCounter=one.lastRollingCode;
  const auto ordinaryCapabilities=model.target.capabilities;
  assert(!model.target.lightUnavailable);one.light=true;model=snapshotModel(true,view);
  assert(model.target.lightUnavailable&&one.light&&submitted.size()==lightRequests&&one.lastRollingCode==lightCounter);
  assert(model.target.capabilities==ordinaryCapabilities);one.light=false;model=snapshotModel(true,view);assert(!model.target.lightUnavailable);
  view.openedToken=model.target.token;view.authorizedSelection=true;view.selectionAuthGeneration=security.generation;
  const auto unseen=submitted.size();assert(dispatch(command(view,Action::up))==DispatchResult::stale);
  assert(submitted.size()==unseen); // never-presented target, including STOP, cannot emit
  one.idle=false;assert(dispatch(command(view,Action::stop))==DispatchResult::stale);
  markControlPresented(view.entity,view.openedToken,true,security.generation);
  assert(dispatch(command(view,Action::up))==DispatchResult::done);
  assert(submitted.back().id==7 && submitted.back().command==somfy_commands::Up);
  auto original=command(view,Action::up); ++one.configurationRevision; ++somfy.configurationRevision;
  assert(dispatch(original)==DispatchResult::stale);
  // Same ID/RF after re-pair/config does not silently grant a new context lease.
  model=snapshotModel(true,view);assert(model.target.token!=view.openedToken);
  security.expiredSessionGeneration=security.generation;security.allowed=false;security.stopGrace=true;++security.generation;
  view.openedToken=model.target.token;view.authorizedSelection=false;
  markControlPresented(view.entity,view.openedToken,false,security.generation);
  snapshotModel(true,view); assert(dispatch(command(view,Action::stop))==DispatchResult::denied);
  security.allowed=true;security.stopGrace=false;security.expiredSessionGeneration=0;++security.generation;
  open(view,{EntityKind::device,7});
  const auto token=view.openedToken;
  SomfyShadeOps ops;
  ops.sendCommand(7,somfy_commands::Up,1,0);one.value.currentPos+=1;
  assert(snapshotModel(true,view).target.token==token); // ordinary code/estimate advancement is not configuration
  const auto beforeCorrection=command(view,Action::stop);
  ops.setPositions(7,one.value.currentPos,-1); // explicit no-op correction still invalidates
  assert(dispatch(beforeCorrection)==DispatchResult::stale);
  open(view,{EntityKind::device,7});auto beforeCode=command(view,Action::up);ops.setRollingCode(7,100);assert(loopModelEvents==1);
  assert(dispatch(beforeCode)==DispatchResult::stale);
  open(view,{EntityKind::device,7});auto beforeFavourite=command(view,Action::up);ops.setMyPosition(7,30,20);
  assert(dispatch(beforeFavourite)==DispatchResult::stale);
  open(view,{EntityKind::device,7});auto beforeUnpair=command(view,Action::up);ops.setUnpaired(7);
  assert(dispatch(beforeUnpair)==DispatchResult::stale);one.paired=true;++one.configurationRevision;
  auto &group=somfy.groups[9];group.id=11;group.linkedShades[0]=22;group.linkedShades[1]=7;group.bitLength=56;
  one.idle=two.idle=false;open(view,{EntityKind::group,11});
  auto beforeGroup=submitted.size();assert(dispatch(command(view,Action::stop))==DispatchResult::done);
  assert(submitted.size()==beforeGroup+1 && submitted.back().type==shade_request_type_t::group_command && submitted.back().id==11);
  refuseAt=submitted.size()+1;beforeGroup=submitted.size();
  assert(dispatch(command(view,Action::stop))==DispatchResult::refused && submitted.size()==beforeGroup+1);
  refuseAt=0;two.idle=true;snapshotModel(true,view);beforeGroup=submitted.size();
  assert(dispatch(command(view,Action::stop))==DispatchResult::done && submitted.size()==beforeGroup+1);
  assert(submitted.back().type==shade_request_type_t::shade_command && submitted.back().id==7);
  one.idle=true;beforeGroup=submitted.size();assert(dispatch(command(view,Action::stop))==DispatchResult::idle);
  assert(submitted.size()==beforeGroup);
  one.idle=two.idle=false;open(view,{EntityKind::group,11});
  security.expiredSessionGeneration=security.generation;security.allowed=false;security.stopGrace=true;++security.generation;view.screen=Screen::home;
  assert(snapshotModel(true,view).target.stopLease);
  one.idle=true;snapshotModel(true,view);one.idle=false;
  beforeGroup=submitted.size();assert(dispatch(command(view,Action::stop))==DispatchResult::done);
  assert(submitted.size()==beforeGroup+1 && submitted.back().id==22 && submitted.back().type==shade_request_type_t::shade_command);
  two.idle=true;snapshotModel(true,view);two.idle=false;
  assert(dispatch(command(view,Action::stop))==DispatchResult::denied);
  security.allowed=true;security.stopGrace=false;security.expiredSessionGeneration=0;++security.generation;
  open(view,{EntityKind::group,11});const auto beforeMembership=command(view,Action::up);
  group.linkedShades[1]=0;++group.configurationRevision;++somfy.configurationRevision;
  assert(dispatch(beforeMembership)==DispatchResult::stale);
  open(view,{EntityKind::device,22});auto beforeRestore=command(view,Action::up);++somfy.restoreRevision;
  assert(dispatch(beforeRestore)==DispatchResult::stale);
  open(view,{EntityKind::device,22});const auto beforeFreeze=command(view,Action::up);setLocalControlsFrozen(true);beforeGroup=submitted.size();
  assert(dispatch(command(view,Action::up))==DispatchResult::refused && submitted.size()==beforeGroup);
  setLocalControlsFrozen(false);
  assert(dispatch(beforeFreeze)==DispatchResult::denied); // complete freeze period invalidates old intent
  assert(submitted.size()==beforeGroup);
  // Everyday actions preserve typed domain operations and their exact arguments.
  one.bitLength=80;one.tiltType=tilt_types::tiltmotor;one.sunSensor=true;one.value.tiltDirection=1;
  ++one.configurationRevision;++somfy.configurationRevision;open(view,{EntityKind::device,7});
  for(unsigned i=0;i<unsigned(Action::count);i++) {
    const auto action=Action(i);if(action==Action::toggle) continue;
    auto request=command(view,action);
    if(action==Action::position || action==Action::tiltPosition) request.value=42;
    if(action==Action::stepUp || action==Action::stepDown) request.step=7;
    const auto count=submitted.size();assert(dispatch(request)==DispatchResult::done && submitted.size()==count+1);
    const auto &sent=submitted.back();assert(sent.id==7);
    if(action==Action::position) assert(sent.type==shade_request_type_t::shade_command && sent.target==42);
    if(action==Action::tiltPosition) assert(sent.type==shade_request_type_t::tilt_command && sent.target==42);
    if(action==Action::tiltUp || action==Action::tiltStop || action==Action::tiltDown) assert(sent.type==shade_request_type_t::tilt_command);
    if(action==Action::my) assert(sent.command==somfy_commands::My);
    if(action==Action::favourite) assert(sent.command==somfy_commands::Favorite);
    if(action==Action::stepUp || action==Action::stepDown) assert(sent.stepSize==7);
    if(action==Action::sunOn) assert(sent.command==sunFlagCommand(true));
    if(action==Action::sunOff) assert(sent.command==sunFlagCommand(false));
  }
  // Exact group Favorite; incomplete, mixed and unsupported members fail closed.
  group.linkedShades[0]=22;group.linkedShades[1]=7;group.bitLength=80;two.bitLength=80;
  ++group.configurationRevision;++two.configurationRevision;++somfy.configurationRevision;
  model=open(view,{EntityKind::group,11});auto count=submitted.size();
  assert(dispatch(command(view,Action::favourite))==DispatchResult::done && submitted.size()==count+1);
  assert(submitted.back().type==shade_request_type_t::group_command && submitted.back().id==11 && submitted.back().command==somfy_commands::Favorite);
  two.bitLength=56;++two.configurationRevision;open(view,{EntityKind::group,11});count=submitted.size();
  assert(dispatch(command(view,Action::favourite))==DispatchResult::refused && submitted.size()==count);
  two.bitLength=80;two.shadeType=shade_types::garage1;++two.configurationRevision;open(view,{EntityKind::group,11});
  assert(dispatch(command(view,Action::favourite))==DispatchResult::refused && submitted.size()==count);
  two.shadeType=shade_types::shutter;group.linkedShades[1]=99;++group.configurationRevision;open(view,{EntityKind::group,11});
  assert(dispatch(command(view,Action::favourite))!=DispatchResult::done && submitted.size()==count);
  group.linkedShades[1]=7;++group.configurationRevision;open(view,{EntityKind::device,7});
  auto invalid=command(view,Action::position);invalid.value=101;beforeGroup=submitted.size();
  assert(dispatch(invalid)==DispatchResult::refused && submitted.size()==beforeGroup);
  assert(dispatch(command(view,Action::count))==DispatchResult::refused && submitted.size()==beforeGroup);
  one.proto=radio_proto::GP_Relay;++one.configurationRevision;++somfy.configurationRevision;
  model=open(view,{EntityKind::device,7});assert(model.target.pendingRelayTilt);
  assert(model.target.capabilities&controlActionBit(Action::up));
  assert(!(model.target.capabilities&controlActionBit(Action::tiltPosition)));
  assert(dispatch(command(view,Action::up))==DispatchResult::done);
  assert(dispatch(command(view,Action::tiltUp))==DispatchResult::refused);
  one.proto=radio_proto::GP_Remote;++one.configurationRevision;++somfy.configurationRevision;
  model=open(view,{EntityKind::device,7});assert(model.target.capabilities&controlActionBit(Action::tiltPosition));
  auto tilt=command(view,Action::tiltPosition);tilt.value=63;assert(dispatch(tilt)==DispatchResult::done);
  one.shadeType=shade_types::garage1;++one.configurationRevision;++somfy.configurationRevision;
  model=open(view,{EntityKind::device,7});assert(model.target.capabilities==controlActionBit(Action::toggle));
  assert(dispatch(command(view,Action::toggle))==DispatchResult::done && submitted.back().command==somfy_commands::Toggle);
  // GPIO capabilities use the same fresh, read-only domain admission as sends.
  one.shadeType=shade_types::roller;one.tiltType=tilt_types::none;
  one.gpioUp=255;one.gpioDown=40;one.gpioMy=255;++one.configurationRevision;
  model=open(view,{EntityKind::device,7});
  assert(model.target.capabilities==controlActionBit(Action::down));
  const auto noGpioSend=submitted.size();
  assert(dispatch(command(view,Action::up))==DispatchResult::refused && submitted.size()==noGpioSend);
  assert(dispatch(command(view,Action::down))==DispatchResult::done);
  one.gpioConflict=1;++one.configurationRevision;model=open(view,{EntityKind::device,7});
  assert(model.target.capabilities==0 && !model.target.valid);
  assert(dispatch(command(view,Action::down))==DispatchResult::refused);
  one.gpioConflict=0;one.proto=radio_proto::GP_Relay;one.idle=true;
  one.gpioUp=17;one.gpioDown=one.gpioMy=255;++one.configurationRevision;
  model=open(view,{EntityKind::device,7});
  assert(model.target.capabilities==(controlActionBit(Action::up)|controlActionBit(Action::stop)));
  one.gpioUp=one.gpioDown=one.gpioMy=0;
  // An old intent/lease must not borrow a later expired session's grace, even
  // when no adapter observation happened during that replacement session.
  one.shadeType=shade_types::shutter;one.proto=radio_proto::RTS;one.tiltType=tilt_types::none;one.bitLength=56;one.idle=false;
  ++one.configurationRevision;++somfy.configurationRevision;open(view,{EntityKind::device,7});
  const auto originalStop=command(view,Action::stop);const auto originalSession=security.generation;
  security.allowed=false;security.stopGrace=true;security.expiredSessionGeneration=originalSession;++security.generation;
  snapshotModel(true,view);++security.generation; // wrong unlock changes revision, not grace source
  assert(dispatch(originalStop)==DispatchResult::done);
  assert(dispatch(command(view,Action::my))==DispatchResult::denied); // valid source is STOP-only
  security.allowed=true;security.stopGrace=false;security.expiredSessionGeneration=0;++security.generation;
  const auto replacementSession=security.generation; // no snapshot/open/present between unlock and later expiry
  security.allowed=false;security.stopGrace=true;security.expiredSessionGeneration=replacementSession;++security.generation;
  beforeGroup=submitted.size();assert(dispatch(originalStop)==DispatchResult::denied);
  assert(!snapshotModel(true,view).target.stopLease && submitted.size()==beforeGroup);
  assert(dispatch(command(view,Action::stop))==DispatchResult::denied); // new source cannot seed an old focus
  security.allowed=true;security.stopGrace=false;security.expiredSessionGeneration=0;++security.generation;
  open(view,{EntityKind::device,7});const auto freshStop=command(view,Action::stop);
  security.allowed=false;security.stopGrace=true;security.expiredSessionGeneration=security.generation;++security.generation;
  assert(dispatch(freshStop)==DispatchResult::done);
  assert(dispatch(originalStop)==DispatchResult::denied); // current lease cannot bless an old ticket
  security.expiredSessionGeneration=0;assert(dispatch(freshStop)==DispatchResult::denied);
  puts("Dynamic adapter: stable sparse IDs, exact revisions/membership, presented target, mutation wrappers and scoped STOP passed.");
}
