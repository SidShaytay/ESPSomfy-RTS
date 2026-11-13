#include "Arduino.h"
#include "SomfyFrame.h"
#include "SettingsLoadState.h"
#include "RadioScheduler.h"
#include "esp_log.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <type_traits>
#include <utility>

static uint32_t nowMs=1000;
static unsigned long clockNow() { return nowMs; }
static RadioScheduler radio;
static bool refuse=false;
static uint8_t radioBitLength=56;
template<class T,class=void>struct HasCancelRemote:std::false_type{};
template<class T>struct HasCancelRemote<T,std::void_t<decltype(std::declval<T&>().cancelRemote(1,1,1))>>:std::true_type{};
template<class T>void cancelRemote(T &scheduler,uint32_t address,uint32_t except) {
  if constexpr(HasCancelRemote<T>::value)scheduler.cancelRemote(address,nowMs,except);
}
static std::vector<somfy_tx_job_t> accepted, aired;
#define TRANSCEIVER_H
struct Transceiver {
  struct {
    SettingsLoadStatus loadStatus{SettingsLoadState::loaded};
    uint8_t type=56;
    radio_proto proto=radio_proto::RTS;
  } config;
  bool counterBlocksIdentity(uint32_t) const { return false; }
  bool counterEditUncertain() const { return false; }
  bool cancelTx(uint32_t ticket) { return radio.cancel(ticket,nowMs); }
  bool cancelUnstartedTx(uint32_t ticket) { return radio.cancelUnstarted(ticket,nowMs); }
  void cancelQueuedForRemote(uint32_t address,uint32_t except=0) {cancelRemote(radio,address,except);}
  bool available() const { return !refuse; }
  bool txHasRoom() const { return !refuse && radio.hasRoom(); }
  uint32_t submitTx(somfy_tx_job_t &job,bool priority=false,uint16_t delayMs=0) {
    if(refuse)return 0;
    const auto ticket=radio.submit(job,priority,delayMs,nowMs);
    if(ticket)accepted.push_back(job);
    return ticket;
  }
};
#include "Somfy.h"
#include "CounterCommandGuard.h"
#if __has_include("GroupMotion.h")
#include "GroupMotion.h"
#endif

bool counterCommandAllowed(uint32_t,somfy_commands){return true;}
SomfyShadeController::SomfyShadeController():startingAddress(100) {modelLoad.bootResult(ModelLoadStatus::loaded);}
SomfyShadeController somfy;
SomfyShade *SomfyShadeController::getShadeById(uint8_t id) {
  for(auto &shade:shades)if(id && id!=255 && shade.getShadeId()==id)return &shade;
  return nullptr;
}
SomfyGroup *SomfyShadeController::getGroupById(uint8_t id) {
  for(auto &group:groups)if(id && id!=255 && group.getGroupId()==id)return &group;
  return nullptr;
}
void SomfyShadeController::updateGroupFlags() {}
uint32_t SomfyShadeController::sendFrame(somfy_frame_t &frame,uint8_t repeat,uint16_t delayMs,bool priority) {
  somfy_tx_job_t job;encodeTxJob(job,frame,repeat);
  return transceiver.submitTx(job,priority,delayMs);
}
bool SomfyRemote::peekNextRollingCode(uint16_t &out) {out=lastRollingCode+1;return true;}
void SomfyRemote::commitRollingCode(uint16_t value) {p_lastRollingCode(value);}
uint16_t SomfyRemote::setRollingCode(uint16_t value) {return p_lastRollingCode(value);}
void SomfyRemote::triggerGPIOs(somfy_frame_t &) {assert(false);}
void SomfyShade::triggerGPIOs(somfy_frame_t &) {assert(false);}
bool SomfyShade::gpioCommandAllowed(somfy_commands) {return true;}
bool SomfyShade::canGpioCommand(somfy_commands) {return true;}
bool SomfyShade::canGpioSeek(bool) {return true;}
bool SomfyShade::canGpioTilt(somfy_commands) {return true;}
void SomfyShade::onSensorFlags() {}
void SomfyShade::emitState(){changed|=SHADE_CHANGED_STATE;}
void SomfyShade::emitCommand(somfy_commands,const char *,uint32_t) {}
void SomfyGroup::emitState(){changed|=GROUP_CHANGED_STATE;}
#define millis clockNow
static const char *TAG="Group target fixture";
#include "production.inc"
#undef TAG
#define TAG groupTag
#include "../../src/SomfyGroup.cpp"
#undef TAG
#define TAG groupTargetTag
#if __has_include("../../src/SomfyGroupTarget.cpp")
#include "../../src/SomfyGroupTarget.cpp"
#endif
#undef TAG
#define TAG commandTag
#include "../../src/ShadeCommands.cpp"
#undef TAG
#include "../../src/ShadeTransport.cpp"
#undef millis

// The baseline has no group percentage operation. A false fallback makes
// absence a behavioral failure rather than a compiler diagnostic.
template<class T,class=void>struct HasTarget:std::false_type{};
template<class T>struct HasTarget<T,std::void_t<decltype(std::declval<T&>().moveToTarget(50.0f))>>:std::true_type{};
template<class T>bool target(T &group,float value) {
  if constexpr(HasTarget<T>::value)return group.moveToTarget(value);
  return false;
}
template<class T,class=void>struct HasService:std::false_type{};
template<class T>struct HasService<T,std::void_t<decltype(std::declval<T&>().serviceTarget())>>:std::true_type{};
template<class T>void service(T &group) {if constexpr(HasService<T>::value)group.serviceTarget();}
static void events() {
  somfy_tx_event_t event;
  while(radio.popEvent(event)) {
    somfy.processTxEvent(event);
  }
}
static void tick(uint32_t delta=0) {
  nowMs+=delta;events();
  for(auto &shade:somfy.shades)if(shade.getShadeId()!=255)shade.checkMovement();
  for(auto &group:somfy.groups)if(group.getGroupId()!=255) {
    service(group);
  }
  events();
}
static bool press() {
  somfy_tx_job_t job;uint32_t wait;
  if(!radio.next(nowMs,job,wait))return false;
  assert(radio.start(job.ticket,nowMs));aired.push_back(job);events();
  nowMs+=274;radio.ended(job.ticket,true,nowMs);events();tick();
  nowMs+=100;return true;
}
static SomfyGroup &setup(float first=0,float second=0) {
  auto &group=somfy.groups[0];group.setGroupId(1);group.setRemoteAddress(900);group.bitLength=56;group.repeats=1;
  for(unsigned i=0;i<2;i++) {
    auto &shade=somfy.shades[i];shade.setShadeId(i+1);shade.setRemoteAddress(100+i);
    shade.shadeType=shade_types::roller;shade.tiltType=tilt_types::none;shade.bitLength=56;shade.repeats=1;
    shade.upTime=shade.downTime=i?20000:10000;
    shade.restore(i?second:first,0,70,-1);group.linkedShades[i]=i+1;
  }
  return group;
}
static size_t airedCommands(uint32_t address,somfy_commands command) {
  size_t n=0;for(const auto &job:aired)if(job.frame.remoteAddress==address && job.frame.cmd==command)n++;return n;
}
static void finishTravel(uint32_t ms) {
  for(uint32_t elapsed=0;elapsed<ms;elapsed+=10) {tick(10);press();}
}
int main(int argc,char **argv) {
  assert(argc==2);const std::string mode=argv[1];
  auto &group=setup(mode=="mixed"?20:0,mode=="mixed"?80:0);
  if(mode=="shared") {
    assert(target(group,100));assert(accepted.size()==1);assert(press());
    assert(aired[0].frame.remoteAddress==900 && aired[0].frame.cmd==somfy_commands::Down);
    assert(somfy.shades[0].state().currentPos>0 && somfy.shades[1].state().currentPos>0);
    assert(somfy.shades[0].state().currentPos>somfy.shades[1].state().currentPos);
  } else if(mode=="unequal") {
    assert(target(group,50));assert(press());finishTravel(21000);
    assert(airedCommands(900,somfy_commands::Down)==1);
    assert(airedCommands(100,somfy_commands::My)==1 && airedCommands(101,somfy_commands::My)==1);
    assert(somfy.shades[0].isIdle() && somfy.shades[1].isIdle());
    assert(std::fabs(somfy.shades[0].state().currentPos-50)<5 && std::fabs(somfy.shades[1].state().currentPos-50)<5);
  } else if(mode=="repeated") {
    assert(target(group,100));assert(target(group,100));assert(accepted.size()==1);assert(press());
    tick(1000);assert(target(group,100));assert(accepted.size()==1);
    finishTravel(21000);assert(target(group,100));assert(accepted.size()==2);
    assert(press());assert(airedCommands(900,somfy_commands::Down)==2);
  } else if(mode=="pending-replace") {
    assert(target(group,100));assert(target(group,50));
    assert(press());finishTravel(21000);
    assert(airedCommands(900,somfy_commands::Down)==1);
    assert(airedCommands(100,somfy_commands::My)==1 && airedCommands(101,somfy_commands::My)==1);
  } else if(mode=="queued-stop") {
    assert(target(group,50));assert(group.sendCommand(somfy_commands::My,1));events();
    finishTravel(1000);assert(aired.empty());
    assert(somfy.shades[0].state().currentPos==0 && somfy.shades[1].state().currentPos==0);
  } else if(mode=="onair-stop") {
    assert(target(group,50));
    somfy_tx_job_t job;uint32_t wait;
    assert(radio.next(nowMs,job,wait));assert(radio.start(job.ticket,nowMs));aired.push_back(job);
    // TX has started atomically but loop() has not observed its event yet.
    // Treating the member as merely queued would lose the eventual STOP.
    assert(group.sendCommand(somfy_commands::My,1));
    events();nowMs+=274;radio.ended(job.ticket,true,nowMs);events();nowMs+=100;
    finishTravel(2000);
    assert(airedCommands(900,somfy_commands::Down)==1);
    assert(airedCommands(100,somfy_commands::My)==1 && airedCommands(101,somfy_commands::My)==1);
  } else if(mode=="member-onair-stop") {
    assert(somfy.shades[0].moveToTarget(100));
    somfy_tx_job_t job;uint32_t wait;
    assert(radio.next(nowMs,job,wait));assert(radio.start(job.ticket,nowMs));aired.push_back(job);
    assert(group.sendCommand(somfy_commands::My,1));
    events();nowMs+=274;radio.ended(job.ticket,true,nowMs);events();nowMs+=100;
    finishTravel(2000);
    assert(airedCommands(100,somfy_commands::My)==1);
    assert(airedCommands(101,somfy_commands::My)==0);
  } else if(mode=="member-queued") {
    assert(somfy.shades[0].moveToTarget(100));
    assert(target(group,50));events();finishTravel(21000);
    assert(airedCommands(100,somfy_commands::Down)==1);
    assert(airedCommands(101,somfy_commands::Down)==1);
    assert(airedCommands(900,somfy_commands::Down)==0);
    assert(airedCommands(100,somfy_commands::My)==1 && airedCommands(101,somfy_commands::My)==1);
  } else if(mode=="idle-stop") {
    somfy.shades[0].restore(50,0,70,-1);
    assert(target(group,50));finishTravel(21000);
    assert(airedCommands(100,somfy_commands::My)==0);
    assert(airedCommands(101,somfy_commands::My)==1);
    assert(airedCommands(900,somfy_commands::My)==0);
  } else if(mode=="mixed") {
    assert(target(group,50));finishTravel(21000);
    assert(airedCommands(900,somfy_commands::Up)==0 && airedCommands(900,somfy_commands::Down)==0);
    assert(airedCommands(100,somfy_commands::Down)==1 && airedCommands(101,somfy_commands::Up)==1);
  } else if(mode=="refused") {
    refuse=true;assert(!target(group,50));assert(accepted.empty());
    assert(somfy.shades[0].isIdle() && somfy.shades[1].isIdle());
    refuse=false;assert(target(group,50));assert(press());tick(1000);
    const auto before=somfy.shades[0].state();refuse=true;
    assert(!target(group,0));assert(somfy.shades[0].state().target==before.target);
  } else if(mode=="refused-member-override" || mode=="refused-group-override") {
    assert(target(group,100));assert(press());tick(3000);assert(target(group,0));
    refuse=true;
    if(mode=="refused-member-override")assert(!somfy.shades[0].sendCommand(somfy_commands::Down,1));
    else assert(!group.sendCommand(somfy_commands::Down,1));
    refuse=false;finishTravel(21000);
    assert(airedCommands(900,somfy_commands::Up)==1);
    assert(somfy.shades[0].state().currentPos==0 && somfy.shades[1].state().currentPos==0);
  } else if(mode=="flag-preserves-reversal") {
    assert(target(group,100));assert(press());tick(3000);assert(target(group,0));
    assert(somfy.shades[0].sendCommand(somfy_commands::Flag,1));finishTravel(21000);
    assert(airedCommands(900,somfy_commands::Up)==1);
  } else if(mode=="deadband") {
    somfy.shades[0].setPosition(49.8f,0);somfy.shades[1].setPosition(50.2f,0);
    assert(target(group,50));finishTravel(1000);assert(aired.empty());
  } else if(mode=="flipped") {
    group.flipCommands=true;somfy.shades[0].flipCommands=somfy.shades[1].flipCommands=true;
    assert(target(group,50));finishTravel(21000);
    assert(airedCommands(900,somfy_commands::Up)==1 && airedCommands(900,somfy_commands::Down)==0);
    assert(airedCommands(100,somfy_commands::My)==1 && airedCommands(101,somfy_commands::My)==1);
    assert(std::fabs(somfy.shades[0].state().currentPos-50)<5 && std::fabs(somfy.shades[1].state().currentPos-50)<5);
  } else if(mode=="dropped-start") {
    assert(target(group,50));radio.flush(nowMs);events();finishTravel(21000);assert(aired.empty());
    assert(somfy.shades[0].state().currentPos==0 && somfy.shades[1].state().currentPos==0);
  } else if(mode=="aborted-start") {
    assert(target(group,50));somfy_tx_job_t job;uint32_t wait;
    assert(radio.next(nowMs,job,wait));assert(radio.start(job.ticket,nowMs));aired.push_back(job);events();
    nowMs+=100;radio.ended(job.ticket,false,nowMs);events();finishTravel(21000);
    assert(aired.size()==1);
  } else if(mode=="sparse-members") {
    group.linkedShades[1]=0;group.linkedShades[31]=2;
    assert(!target(group,50));finishTravel(1000);assert(aired.empty());
    assert(somfy.shades[0].isIdle() && somfy.shades[1].isIdle());
  } else if(mode=="favorite-program-queued" || mode=="favorite-program-aborted" || mode=="favorite-program-finished") {
    group.bitLength=somfy.shades[0].bitLength=somfy.shades[1].bitLength=80;
    auto &shade=somfy.shades[0];
    assert(shade.setMyPosition(0));assert(shade.state().myPos==0);
    // A nonmoving programming command may leave a hold until the next loop.
    // Clear that hold exactly as production tick does before the next request.
    shade.checkMovement();
    somfy_tx_job_t programming;uint32_t wait;
    const bool onAir=mode!="favorite-program-queued";
    if(onAir) {
      assert(radio.next(nowMs,programming,wait));assert(radio.start(programming.ticket,nowMs));aired.push_back(programming);events();
    }
    const auto admitted=accepted.size();
    assert(!target(group,50));
    assert(!group.sendCommand(somfy_commands::Favorite,1));assert(accepted.size()==admitted);
    if(onAir) {
      nowMs+=500;somfy.isDirty=false;
      radio.ended(programming.ticket,mode=="favorite-program-finished",nowMs);events();nowMs+=100;
      if(mode=="favorite-program-finished")assert(somfy.isDirty);
    } else {
      // Cancellation is an actual scheduler outcome, not inferred from the
      // refused replacement. Programming remains admitted until this event.
      assert(radio.cancel(accepted[0].ticket,nowMs));events();
    }
    const float expected=mode=="favorite-program-finished"?0:70;
    assert(shade.state().myPos==expected);
    assert(group.sendCommand(somfy_commands::Favorite,1));finishTravel(21000);
    assert(shade.state().myPos==expected && shade.state().currentPos==expected);
  } else if(mode=="stop-program-queued") {
    group.bitLength=somfy.shades[0].bitLength=somfy.shades[1].bitLength=80;
    assert(somfy.shades[1].moveToTarget(100));assert(press());
    auto &shade=somfy.shades[0];assert(shade.setMyPosition(0));shade.checkMovement();
    const auto programming=accepted.back().ticket;
    assert(group.sendCommand(somfy_commands::My,1));events();
    // Whichever Stop policy is chosen, the actual dropped programming event
    // must remain able to restore its old favourite.
    radio.cancel(programming,nowMs);events();finishTravel(1000);
    assert(shade.state().myPos==70);
    assert(airedCommands(100,somfy_commands::My)==0);
    assert(airedCommands(101,somfy_commands::My)==1);
  } else if(mode=="favorite") {
    group.bitLength=somfy.shades[0].bitLength=somfy.shades[1].bitLength=80;
    somfy.shades[0].simMy=true;
    assert(group.sendCommand(somfy_commands::Favorite,1));finishTravel(21000);
    assert(aired.size()==1 && airedCommands(900,somfy_commands::Favorite)==1);
    assert(somfy.shades[0].state().currentPos==70 && somfy.shades[1].state().currentPos==70);
  } else if(mode=="integrated-group") {
    for(auto &shade:somfy.shades)if(shade.getShadeId()!=255) {
      shade.tiltType=tilt_types::integrated;shade.tiltTime=2000;
      shade.restore(0,0,70,50);
    }
    assert(group.sendCommand(somfy_commands::Down,1));
    somfy_tx_job_t job;uint32_t wait;
    assert(radio.next(nowMs,job,wait));assert(radio.start(job.ticket,nowMs));aired.push_back(job);events();
    tick(100);
    assert(somfy.shades[0].state().currentPos==0 && somfy.shades[1].state().currentPos==0);
    assert(somfy.shades[0].state().currentTiltPos>0 && somfy.shades[1].state().currentTiltPos>0);
    nowMs+=174;radio.ended(job.ticket,true,nowMs);events();finishTravel(25000);
    assert(somfy.shades[0].state().currentPos==100 && somfy.shades[1].state().currentPos==100);
  } else if(mode=="short-step") {
    group.bitLength=somfy.shades[0].bitLength=somfy.shades[1].bitLength=80;
    somfy.shades[0].stepSize=somfy.shades[1].stepSize=100;
    assert(group.sendCommand(somfy_commands::StepDown,1,1));
    somfy_tx_job_t job;uint32_t wait;
    assert(radio.next(nowMs,job,wait));assert(radio.start(job.ticket,nowMs));aired.push_back(job);events();
    tick(200);
    assert(somfy.shades[0].state().currentPos==1 && somfy.shades[1].state().currentPos==0.5f);
    nowMs+=74;radio.ended(job.ticket,true,nowMs);events();finishTravel(1000);
    assert(aired.size()==1);
    assert(somfy.shades[0].state().currentPos==1 && somfy.shades[1].state().currentPos==0.5f);
  } else if(mode=="step") {
    group.bitLength=somfy.shades[0].bitLength=somfy.shades[1].bitLength=80;
    somfy.shades[0].stepSize=somfy.shades[1].stepSize=100;
    assert(group.sendCommand(somfy_commands::StepDown,1,5));finishTravel(1000);
    assert(aired.size()==1 && aired[0].frame.stepSize==5);
    assert(somfy.shades[0].state().currentPos==5 && somfy.shades[1].state().currentPos==2.5f);
  } else if(mode=="stale-stop") {
    assert(target(group,50));assert(press());tick(4800);
    assert(target(group,100));finishTravel(21000);
    assert(airedCommands(100,somfy_commands::My)==0 && airedCommands(101,somfy_commands::My)==0);
    assert(somfy.shades[0].state().currentPos==100 && somfy.shades[1].state().currentPos==100);
  } else if(mode=="reversal") {
    assert(target(group,100));assert(press());tick(3000);assert(target(group,0));finishTravel(21000);
    assert(somfy.shades[0].state().currentPos==0 && somfy.shades[1].state().currentPos==0);
    assert(airedCommands(900,somfy_commands::Up)==1);
  } else assert(false);
  std::printf("PASS group target: %s\n",mode.c_str());
}
