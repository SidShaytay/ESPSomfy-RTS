#include "Arduino.h"
#include "SomfyFrame.h"
#include "SettingsLoadState.h"
#include "RadioScheduler.h"
#include "LiftCalibrationGate.h"
#include "esp_log.h"
#include <cassert>
#include <atomic>
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
static unsigned counterPeeks=0;
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
bool SomfyRemote::peekNextRollingCode(uint16_t &out) {++counterPeeks;out=lastRollingCode+1;return true;}
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

namespace GuideAdapterFixture {
std::atomic<bool> dispatching{false};
struct SendBoundary {
  virtual ~SendBoundary()=default;
  virtual send_result_t send(uint8_t,somfy_commands,bool)=0;
};
struct Backend:SendBoundary {
  struct { uint64_t generation=1; } owner;
  bool held=true;
  bool owns(uint64_t value) { return held && value==owner.generation; }
#include "production-guide-send.inc"
};
}

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
static void savedCalibrations() {
  auto &fast=somfy.shades[0];
  fast.upTime=fast.downTime=10000;
  fast.calibration.kind=LiftKind::segmented;
  fast.calibration.up=fast.calibration.down={1000,3000,1000,200};
  auto &slow=somfy.shades[1];
  slow.upTime=slow.downTime=20000;
  slow.calibration.kind=LiftKind::segmented;
  slow.calibration.up=slow.calibration.down={2000,6000,2000,500};
}
static size_t acceptedCommands(uint32_t address,somfy_commands command) {
  size_t count=0;
  for(const auto &job:accepted)if(job.frame.remoteAddress==address && job.frame.cmd==command)++count;
  return count;
}
static void tickUntil(uint32_t absolute) {
  assert(absolute>=nowMs);
  while(nowMs<absolute)tick(std::min(10u,absolute-nowMs));
}
int main(int argc,char **argv) {
  assert(argc==2);const std::string mode=argv[1];
  auto &group=setup(mode=="mixed"?20:0,mode=="mixed"?80:0);
  if(mode=="guide-raw-start-stop" || mode=="guide-raw-stop-takeup" || mode=="guide-zero-timer-stop") {
    const bool takeup=mode=="guide-raw-stop-takeup";
    const bool zero=mode=="guide-zero-timer-stop";
    if(takeup)savedCalibrations();
    if(zero)somfy.shades[0].upTime=somfy.shades[0].downTime=0;
    GuideAdapterFixture::Backend backend;
    LiftCalibrationGate::active.store(true);
    LiftCalibrationGate::permitted=+[]{return GuideAdapterFixture::dispatching.load();};
    backend.held=false;
    assert(!backend.send(1,somfy_commands::Down,false).sent && accepted.empty());
    backend.held=true;
    const auto start=backend.send(1,somfy_commands::Down,false);
    assert(start.sent && start.ticket!=0 && !GuideAdapterFixture::dispatching.load());
    assert(accepted.size()==1 && accepted[0].ticket==start.ticket);assert(press());
    if(!takeup && !zero)tick(2126); // First member is now25% down from actual RF start.
    auto &shade=somfy.shades[0];
    const float before=shade.state().currentPos;
    if(!zero)assert(takeup ? before==0 : before>0);
    const auto stop=backend.send(1,somfy_commands::My,true);
    assert(stop.sent && stop.ticket!=0 && stop.ticket!=start.ticket);
    assert(!GuideAdapterFixture::dispatching.load());
    assert(aired.size()==1);tick(100); // STOP is accepted but not on air yet.
    if(zero)assert(shade.state().currentPos==before);
    else assert(takeup ? shade.state().currentPos==0 : shade.state().currentPos>before);
    somfy_tx_job_t job;uint32_t wait;
    assert(radio.next(nowMs,job,wait));assert(job.ticket==stop.ticket && job.priority);
    assert(job.frame.cmd==somfy_commands::My);
    assert(radio.start(job.ticket,nowMs));aired.push_back(job);events();
    const float stoppedAt=shade.state().currentPos;
    nowMs+=274;radio.ended(job.ticket,true,nowMs);events();tick();
    assert(shade.isIdle());
    assert(std::fabs(shade.state().target-stoppedAt)<0.05f);
    assert(std::fabs(shade.state().currentPos-stoppedAt)<0.05f);
    if(takeup)assert(shade.state().referenceRequired);
    if(zero)assert(!shade.state().endpointConfirmed);
    tick(5000);
    assert(shade.isIdle() && std::fabs(shade.state().currentPos-stoppedAt)<0.05f);
    assert(airedCommands(100,somfy_commands::Down)==1 && airedCommands(100,somfy_commands::My)==1);
    assert(accepted.size()==2 && shade.state().myPos==70);
    LiftCalibrationGate::permitted=nullptr;
    LiftCalibrationGate::active.store(false);
  } else if(mode=="calibration-exclusive-gate") {
    LiftCalibrationGate::active.store(true);
    LiftCalibrationGate::permitted=nullptr;
    const auto first=somfy.shades[0].state(),second=somfy.shades[1].state();
    const auto peeks=counterPeeks;
    const auto firstCode=somfy.shades[0].lastRollingCode,secondCode=somfy.shades[1].lastRollingCode,groupCode=group.lastRollingCode;
    assert(!somfy.shades[0].sendCommand(somfy_commands::Down,1));
    assert(!group.sendCommand(somfy_commands::Down,1));
    assert(!target(group,50));
    assert(accepted.empty() && aired.empty());assert(counterPeeks==peeks);
    assert(somfy.shades[0].lastRollingCode==firstCode && somfy.shades[1].lastRollingCode==secondCode && group.lastRollingCode==groupCode);
    assert(somfy.shades[0].state().target==first.target && somfy.shades[0].state().currentPos==first.currentPos);
    assert(somfy.shades[1].state().target==second.target && somfy.shades[1].state().currentPos==second.currentPos);
    assert(somfy.shades[0].state().direction==first.direction && somfy.shades[1].state().direction==second.direction);
    // Only the service's explicitly scoped permission may reach the same
    // production transport while a calibration lease blocks ordinary callers.
    LiftCalibrationGate::permitted=+[]{return true;};
    const auto scoped=somfy.shades[0].sendCommandAfter(somfy_commands::Down,3,0,0);
    assert(scoped.sent && scoped.ticket!=0);assert(counterPeeks==peeks+1);
    assert(accepted.size()==1 && accepted[0].ticket==scoped.ticket);
    assert(accepted[0].frame.remoteAddress==somfy.shades[0].getRemoteAddress());
    assert(accepted[0].frame.cmd==somfy_commands::Down && accepted[0].repeats==3);
    assert(somfy.shades[0].lastRollingCode==uint16_t(firstCode+1));
    assert(somfy.shades[1].lastRollingCode==secondCode && group.lastRollingCode==groupCode);
    LiftCalibrationGate::permitted=nullptr;
    LiftCalibrationGate::active.store(false);
  } else if(mode=="calibrated-raw-group-reversal") {
    savedCalibrations();
    const auto startedAt=nowMs;
    const auto down=group.sendCommandReceipt(somfy_commands::Down,7,0);
    assert(down.sent && down.ticket!=0);
    assert(accepted.size()==1 && accepted[0].ticket==down.ticket && accepted[0].repeats==7);
    assert(press());
    assert(aired.size()==1 && aired[0].ticket==down.ticket && aired[0].repeats==7);
    tickUntil(startedAt+2500);
    assert(std::fabs(somfy.shades[0].state().currentPos-25)<0.05f);
    const float before=somfy.shades[0].state().currentPos;
    const auto up=group.sendCommandReceipt(somfy_commands::Up,5,0);
    assert(up.sent);
    // The replacement is only admitted; no Up or Stop has gone on air.
    // Existing Down travel must continue while its Stop waits in the queue.
    tick(200);
    assert(aired.size()==1);
    assert(somfy.shades[0].state().currentPos>=before);
    assert(somfy.shades[0].state().direction>0);
    // Drain the accepted Stop/reversal sequence, observing the actual shared
    // Up start. Both members owe reversal take-up, not a fresh endpoint phase.
    bool reversed=false;
    for(unsigned attempt=0;attempt<8 && !reversed;++attempt) {
      somfy_tx_job_t job;uint32_t wait;
      assert(radio.next(nowMs,job,wait));assert(radio.start(job.ticket,nowMs));aired.push_back(job);events();
      if(job.frame.remoteAddress==900 && job.frame.cmd==somfy_commands::Up) {
        const float fast=somfy.shades[0].state().currentPos,slow=somfy.shades[1].state().currentPos;
        tick(150);
        assert(std::fabs(somfy.shades[0].state().currentPos-fast)<0.05f);
        assert(std::fabs(somfy.shades[1].state().currentPos-slow)<0.05f);
        nowMs+=124;reversed=true;
      } else nowMs+=274;
      radio.ended(job.ticket,true,nowMs);events();tick();nowMs+=100;
    }
    assert(reversed);finishTravel(25000);
    assert(airedCommands(900,somfy_commands::Up)==1);
    assert(airedCommands(100,somfy_commands::Up)==0 && airedCommands(101,somfy_commands::Up)==0);
    assert(somfy.shades[0].state().currentPos==0 && somfy.shades[1].state().currentPos==0);
    assert(somfy.shades[0].state().myPos==70 && somfy.shades[1].state().myPos==70);
  } else if(mode=="calibrated-deadlines") {
    savedCalibrations();
    const auto start=nowMs;
    assert(target(group,50));assert(accepted.size()==1);assert(press());
    assert(airedCommands(900,somfy_commands::Down)==1);
    tickUntil(start+3990);
    assert(accepted.size()==1);
    tickUntil(start+4000);
    assert(acceptedCommands(100,somfy_commands::My)==1);
    assert(acceptedCommands(101,somfy_commands::My)==0);
    assert(press());
    tickUntil(start+7990);
    assert(acceptedCommands(101,somfy_commands::My)==0);
    tickUntil(start+8000);
    assert(acceptedCommands(101,somfy_commands::My)==1);assert(press());
    assert(airedCommands(100,somfy_commands::My)==1 && airedCommands(101,somfy_commands::My)==1);
    assert(airedCommands(100,somfy_commands::Down)==0 && airedCommands(101,somfy_commands::Down)==0);
    assert(airedCommands(900,somfy_commands::Down)==1);
  } else if(mode=="calibrated-endpoint-one-settling" || mode=="calibrated-endpoint-both-settling") {
    savedCalibrations();
    const bool both=mode=="calibrated-endpoint-both-settling";
    if(both) {
      auto &slow=somfy.shades[1];slow.upTime=slow.downTime=10000;
      slow.calibration.up=slow.calibration.down={1500,2500,2000,300};
    }
    const auto start=nowMs;
    assert(target(group,100));assert(press());
    tickUntil(start+(both?9500:18500));
    // Visible lift completion does not finish the motor's settling phase.
    assert(somfy.shades[0].state().currentPos==100 && somfy.shades[1].state().currentPos==100);
    assert(somfy.shades[0].idleForAdministration()==!both);
    assert(!somfy.shades[1].idleForAdministration());
    const auto before=accepted.size();
    assert(target(group,100));assert(accepted.size()==before);
    assert(target(group,100));assert(accepted.size()==before);
    assert(airedCommands(900,somfy_commands::Down)==1);
    assert(acceptedCommands(100,somfy_commands::My)==0 && acceptedCommands(101,somfy_commands::My)==0);
    tickUntil(start+(both?10000:20000));
    assert(somfy.shades[0].idleForAdministration() && somfy.shades[1].idleForAdministration());
    // Once both timed runs have completed, an endpoint refresh remains a
    // legitimate native group press; RTS has no physical position feedback.
    assert(target(group,100));assert(accepted.size()==before+1);assert(press());
    assert(airedCommands(900,somfy_commands::Down)==2);
  } else if(mode=="calibrated-refused-start") {
    savedCalibrations();refuse=true;
    const auto groupCode=group.lastRollingCode;
    assert(!target(group,50));assert(accepted.empty());
    tick(25000);
    assert(group.lastRollingCode==groupCode && aired.empty());
    for(unsigned i=0;i<2;++i) {
      assert(somfy.shades[i].state().currentPos==0 && somfy.shades[i].state().target==0);
      assert(somfy.shades[i].state().myPos==70 && somfy.shades[i].isIdle());
    }
  } else if(mode=="calibrated-refused-retarget") {
    savedCalibrations();
    assert(target(group,50));assert(press());tick(2000);
    const auto before=accepted.size();
    refuse=true;assert(!target(group,0));assert(accepted.size()==before);
    assert(somfy.shades[0].state().target==50 && somfy.shades[1].state().target==50);
    refuse=false;finishTravel(25000);
    assert(airedCommands(900,somfy_commands::Down)==1 && airedCommands(900,somfy_commands::Up)==0);
    assert(airedCommands(100,somfy_commands::My)==1 && airedCommands(101,somfy_commands::My)==1);
    assert(somfy.shades[0].state().myPos==70 && somfy.shades[1].state().myPos==70);
  } else if(mode=="calibrated-dropped-start") {
    savedCalibrations();assert(target(group,50));radio.flush(nowMs);events();
    finishTravel(25000);assert(aired.empty());
    for(unsigned i=0;i<2;++i) {
      assert(somfy.shades[i].state().currentPos==0 && somfy.shades[i].state().myPos==70);
      assert(somfy.shades[i].isIdle());
    }
  } else if(mode=="calibrated-aborted-start") {
    savedCalibrations();assert(target(group,50));
    somfy_tx_job_t job;uint32_t wait;
    assert(radio.next(nowMs,job,wait));assert(radio.start(job.ticket,nowMs));aired.push_back(job);events();
    // A partial press during mechanical take-up may leave the motor idle.
    // Never follow an ambiguous start with My, which could recall a favourite.
    nowMs+=100;radio.ended(job.ticket,false,nowMs);events();finishTravel(25000);
    assert(aired.size()==1);
    assert(airedCommands(100,somfy_commands::My)==0 && airedCommands(101,somfy_commands::My)==0);
    assert(somfy.shades[0].state().myPos==70 && somfy.shades[1].state().myPos==70);
  } else if(mode=="calibrated-aborted-stop") {
    savedCalibrations();assert(target(group,100));assert(press());tick(3000);
    assert(target(group,0));
    somfy_tx_job_t stop;uint32_t wait;
    assert(radio.next(nowMs,stop,wait));assert(stop.frame.cmd==somfy_commands::My);
    assert(radio.start(stop.ticket,nowMs));aired.push_back(stop);events();
    nowMs+=100;radio.ended(stop.ticket,false,nowMs);events();nowMs+=100;
    finishTravel(25000);
    assert(airedCommands(900,somfy_commands::Up)==0);
    assert(airedCommands(100,somfy_commands::Up)==0 && airedCommands(101,somfy_commands::Up)==0);
    assert(airedCommands(100,somfy_commands::My)==1 && airedCommands(101,somfy_commands::My)==1);
    assert(somfy.shades[0].state().myPos==70 && somfy.shades[1].state().myPos==70);
  } else if(mode=="shared") {
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
