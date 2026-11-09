#include <assert.h>
#include <stdio.h>
#include <mutex>
#include "ExplicitCounter.h"
#include "SomfyFrame.h"
#include "CounterEdit.h"
#include "RadioScheduler.h"
#include "GitUpdate.h"
#include "Utils.h"
#include "RollingCode.h"
#include "LoopQueue.h"
#include "SafeRestart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <nvs.h>
#include "esp_log.h"
static const char *TAG="Counter test";
#define TRANSCEIVER_H
// Replace only the hardware container; policy and reservation bodies below
// are the actual production methods operating on the actual scheduler.
struct Transceiver {
  struct {uint8_t type=56;radio_proto proto=radio_proto::RTS;} config;
  RadioScheduler sched;CounterEditSafety counterEdits;
  CounterEditAdmission reserveCounterEdit(uint32_t,bool,uint16_t,bool);
  void finishCounterEdit(CounterEditResult,ModelCommitResult);
  CounterEditAdmission reserveCounterFileRecovery(bool);
  void finishCounterFileRecovery(ModelCommitResult);
  uint32_t counterFileRecoveryAddress();CounterQuarantine counterQuarantine();
  bool counterBlocksIdentity(uint32_t);bool counterEditUncertain();
};
#include "Somfy.h"
static somfy_tx_queue_t tx_queue;
static std::mutex txLock;
struct TxQueueLock {std::lock_guard<std::mutex> held;TxQueueLock():held(txLock){}};
SomfyShadeController::SomfyShadeController() : startingAddress(100) {}
SomfyShadeController somfy;
static bool pendingRestart=false;
bool restartPending(){return pendingRestart;}
// Queue/history forwarding is exercised by the actual queue/service harness.
void noteRestartOperationAccepted(){}
static unsigned fileWrites=0,forbidden=0,setupFrames=0;
static bool allowSetupSend=false;
static unsigned long modelNow=1;
static unsigned long modelClock(){return modelNow;}
LoopQueue loopQueue;
static ModelCommitResult fileResult{ModelCommitState::saved};
static uint16_t diskShade[SOMFY_MAX_SHADES],diskGroup[SOMFY_MAX_GROUPS];
static uint32_t diskLinks[SOMFY_MAX_SHADES][SOMFY_MAX_LINKED_REMOTES];
ModelCommitResult SomfyShadeController::commit() {
  ++fileWrites;assert(gitUpdate.busy() && somfy.transceiver.counterEdits.active());GitUpdateOwnerToken competing;
  assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,competing));
  assert(FakeNvs::sets>=1 && FakeNvs::commits>=1 && FakeNvs::reads>=1);
  if(fileResult.saved()) {
    for(unsigned i=0;i<SOMFY_MAX_SHADES;++i) {
      diskShade[i]=shades[i].lastRollingCode;
      for(unsigned j=0;j<SOMFY_MAX_LINKED_REMOTES;++j)diskLinks[i][j]=shades[i].linkedRemotes[j].getRemoteAddress();
    }
    for(unsigned i=0;i<SOMFY_MAX_GROUPS;++i)diskGroup[i]=groups[i].lastRollingCode;
  }
  return fileResult;
}
bool SomfyRemote::isLastCommand(somfy_commands){++forbidden;return false;}
uint16_t SomfyRemote::setRollingCode(uint16_t){++forbidden;return 0;}
bool SomfyRemote::sendCommand(somfy_commands){++forbidden;return false;}
bool SomfyRemote::sendCommand(somfy_commands cmd,uint8_t,uint8_t){
  if(allowSetupSend){++setupFrames;lastFrame.valid=true;lastFrame.cmd=cmd;return true;}
  ++forbidden;return false;
}
void SomfyRemote::triggerGPIOs(somfy_frame_t &){++forbidden;}
bool SomfyShade::sendCommand(somfy_commands){++forbidden;return false;}
bool SomfyShade::sendCommand(somfy_commands,uint8_t,uint8_t){++forbidden;return false;}
void SomfyShade::triggerGPIOs(somfy_frame_t &){++forbidden;}
bool SomfyShade::gpioCommandAllowed(somfy_commands){++forbidden;return false;}
send_result_t SomfyShade::send(somfy_commands,uint8_t,uint8_t,uint16_t,bool){++forbidden;return {};}
void SomfyShade::onSaveWanted(){somfy.isDirty=true;}
void SomfyShade::onSensorFlags(){++forbidden;}
void SomfyShade::emitCommand(somfy_commands,const char*,uint32_t){}
#define millis modelClock
#include "production-counter.inc"
#undef millis
#include "../../src/ExplicitCounter.cpp"
static constexpr uint32_t address=12345;
static void reset() {
  assert(!gitUpdate.busy() && !forbidden);FakeNvs::reset();pendingRestart=false;fileWrites=forbidden=setupFrames=0;allowSetupSend=false;modelNow=1;fileResult={ModelCommitState::saved};
  memset(diskShade,0,sizeof(diskShade));memset(diskGroup,0,sizeof(diskGroup));memset(diskLinks,0,sizeof(diskLinks));
  tx_queue.length=0;somfy.transceiver.sched={};somfy.transceiver.counterEdits={};somfy.configurationRevision=1;
  for(auto &s:somfy.shades){s.clear();s.lastFrame={};s.gpioRelease=0;s.configurationRevision=1;}
  for(auto &g:somfy.groups){g.clear();g.lastFrame={};g.direction=0;g.configurationRevision=1;}
  auto &s=somfy.shades[0];s.setShadeId(7);s.setRemoteAddress(address);s.lastRollingCode=1000;s.lastFrame.valid=true;diskShade[0]=1000;
}
static void aliases() {
  auto &s=somfy.shades[1];s.setShadeId(22);s.setRemoteAddress(address);s.lastRollingCode=1200;
  s.linkedRemotes[4].setRemoteAddress(address);s.linkedRemotes[4].lastRollingCode=1300;
  auto &other=somfy.shades[2];other.setShadeId(31);other.setRemoteAddress(999);other.lastRollingCode=777;other.lastFrame.valid=true;
  other.linkedRemotes[6].setRemoteAddress(address);other.linkedRemotes[6].lastRollingCode=1400;
  auto &g=somfy.groups[3];g.setGroupId(12);g.setRemoteAddress(address);g.lastRollingCode=1500;g.linkedShades[0]=7;
}
int main(int argc,char **argv) {
  loopQueue.begin();
  if(argc>1 && !strcmp(argv[1],"completedGroup")) {
    for(bool up:{false,true}) {
      reset();auto &group=somfy.groups[15];group.setGroupId(16);group.setRemoteAddress(address);
      group.linkedShades[30]=255;group.linkedShades[31]=7;
      auto &shade=somfy.shades[0];shade.upTime=shade.downTime=100;shade.restore(up?100:0,0,-1,-1);
      allowSetupSend=true;assert(group.sendCommand(up?somfy_commands::Up:somfy_commands::Down,1));allowSetupSend=false;
      assert(group.direction==(up?-1:1) && !shade.idleForAdministration());
      modelNow=2;shade.checkMovement();modelNow=1002;shade.checkMovement();
      assert(group.direction==(up?-1:1) && shade.idleForAdministration() && setupFrames==1);
      assert(correctExplicitCounter(7,100).saved() && setupFrames==1 && !forbidden);
    }
    puts("Completed production group movement permits checked alias correction");return 0;
  }
  if(argc>1 && !strcmp(argv[1],"unknownGroup")) {
    for(auto cmd:{somfy_commands::Up,somfy_commands::Down,somfy_commands::My,somfy_commands::StepUp,
        somfy_commands::StepDown,somfy_commands::Toggle,somfy_commands::Favorite}) {
      reset();auto &group=somfy.groups[15];group.setGroupId(16);group.setRemoteAddress(address);
      group.lastFrame.valid=true;group.lastFrame.cmd=cmd;
      assert(!correctExplicitCounter(7,100).saved() && !FakeNvs::opens && somfy.configurationRevision==1);
      assert(!linkExplicitRemote(7,address,100).saved() && !FakeNvs::opens && somfy.configurationRevision==1);
    }
    reset();auto &group=somfy.groups[15];group.setGroupId(16);group.setRemoteAddress(address);group.linkedShades[31]=31;
    assert(!correctExplicitCounter(7,100).saved() && !FakeNvs::opens); // Missing is stricter even without cached motion evidence.
    group.linkedShades[31]=255;group.lastFrame.valid=false;group.lastFrame.cmd=somfy_commands::Down;
    assert(correctExplicitCounter(7,100).saved()); // No members/evidence; known primary remains tracked.
    puts("Shared group classifier refuses unknown motion/missing members without inventing cached motion");return 0;
  }
  if(argc>1 && !strcmp(argv[1],"pendingRecovery")) {
    reset();fileResult={ModelCommitState::failed,ModelCommitStage::write};
    assert(correctExplicitCounter(7,100).applied);const auto revision=somfy.configurationRevision;
    loop_item_t stop;stop.type=loop_item_type_t::local_control;stop.localToken=77;stop.localRadio=true;
    assert(loopQueue.submit(stop));const auto sets=FakeNvs::sets;
    fileResult={ModelCommitState::saved};assert(recoverExplicitCounterFile().saved());
    assert(somfy.configurationRevision==revision && loopQueue.pending()==1 && lastLocalToken==77 && FakeNvs::sets==sets);
    puts("File-only recovery preserves accepted STOP/history without repeating NVS");return 0;
  }
  if(argc>1 && !strcmp(argv[1],"pending")) {
    reset();loop_item_t stop;stop.type=loop_item_type_t::local_control;stop.localToken=77;stop.localRadio=true;
    assert(loopQueue.submit(stop));const auto revision=somfy.configurationRevision;
    const auto entity=somfy.shades[0].configurationRevision;
    for(bool link:{false,true}) {
      const auto result=link ? linkExplicitRemote(7,555,100) : correctExplicitCounter(7,100);
      assert(result.state==CounterDomainState::busy && !result.applied && !FakeNvs::opens && !fileWrites);
      assert(somfy.configurationRevision==revision && somfy.shades[0].configurationRevision==entity && somfy.shades[0].lastFrame.valid);
      assert(loopQueue.pending()==1 && lastLocalToken==77 && !gitUpdate.busy());
    }
    puts("Real LoopQueue preserves accepted unrelated STOP while correction/link refuse");return 0;
  }
  reset();aliases();auto result=correctExplicitCounter(7,100);
  assert(result.saved() && result.applied && fileWrites==1 && FakeNvs::sets==1 && FakeNvs::commits==1);
  assert(!gitUpdate.busy() && !somfy.transceiver.counterEditUncertain());
  assert(somfy.shades[0].lastRollingCode==100 && somfy.shades[1].lastRollingCode==100);
  assert(somfy.shades[1].linkedRemotes[4].lastRollingCode==100 && somfy.shades[2].linkedRemotes[6].lastRollingCode==100);
  assert(somfy.groups[3].lastRollingCode==100 && somfy.shades[2].lastRollingCode==777 && !somfy.shades[2].lastFrame.valid);
  assert(rollingCodeAfterLoad(100,diskShade[0])==100 && rollingCodeAfterLoad(100,diskShade[1])==100 && rollingCodeAfterLoad(100,diskGroup[3])==100);
  assert(!somfy.shades[0].lastFrame.valid && somfy.shades[0].lastFrame.processed && !somfy.shades[0].lastFrame.await);
  assert(somfy.shades[0].configurationRevision>1 && somfy.shades[1].configurationRevision>1 && somfy.groups[3].configurationRevision>1);
  reset();FakeNvs::failOpen=1;result=correctExplicitCounter(7,10);
  assert(result.state==CounterDomainState::nvsFailed && !result.applied && !fileWrites && !FakeNvs::sets);
  assert(somfy.shades[0].lastRollingCode==1000 && somfy.shades[0].lastFrame.valid && somfy.configurationRevision==1 && !gitUpdate.busy());
  for(unsigned failure=0;failure<4;++failure) {
    reset();if(failure==0)FakeNvs::failSet=1;else if(failure==1)FakeNvs::failCommit=1;else if(failure==2)FakeNvs::failRead=1;else FakeNvs::corruptRead=1;
    result=correctExplicitCounter(7,10);
    assert(result.state==CounterDomainState::nvsUncertain && !result.applied && !fileWrites && FakeNvs::sets==1);
    assert(somfy.shades[0].lastRollingCode==1000 && somfy.transceiver.counterQuarantine()==CounterQuarantine::nvsUnknown);
    const auto sets=FakeNvs::sets;assert(!recoverExplicitCounterFile().saved());assert(FakeNvs::sets==sets && !fileWrites && !gitUpdate.busy());
  }
  for(unsigned state=1;state<=2;++state)for(unsigned stage=1;stage<=unsigned(ModelCommitStage::replace);++stage) {
    reset();fileResult={ModelCommitState(state),ModelCommitStage(stage)};result=correctExplicitCounter(7,100);
    assert(!result.saved() && result.applied && result.file.state==fileResult.state && result.file.stage==fileResult.stage);
    assert(somfy.shades[0].lastRollingCode==100 && somfy.transceiver.counterQuarantine()==CounterQuarantine::filePending);
    const auto sets=FakeNvs::sets;assert(!recoverExplicitCounterFile().saved() && FakeNvs::sets==sets);
    fileResult={ModelCommitState::saved};assert(recoverExplicitCounterFile().saved() && FakeNvs::sets==sets);
    assert(!somfy.transceiver.counterEditUncertain() && !gitUpdate.busy());
  }
  reset();fileResult={ModelCommitState::failed,ModelCommitStage::write};result=correctExplicitCounter(7,100);
  assert(result.applied && rollingCodeAfterLoad(100,diskShade[0])==1000); // Power-loss gap remains explicit.
  fileResult={ModelCommitState::saved};assert(recoverExplicitCounterFile().saved());
  assert(rollingCodeAfterLoad(100,diskShade[0])==100 && FakeNvs::sets==1);
  reset();result=linkExplicitRemote(7,555,42);assert(result.saved() && result.applied && somfy.shades[0].linkedRemotes[0].getRemoteAddress()==555);
  reset();fileResult={ModelCommitState::failed,ModelCommitStage::replace};result=linkExplicitRemote(7,555,42);
  assert(result.applied && !result.saved() && somfy.shades[0].linkedRemotes[0].getRemoteAddress()==555 && !diskLinks[0][0]);
  fileResult={ModelCommitState::saved};assert(recoverExplicitCounterFile().saved());
  assert(diskLinks[0][0]==555 && FakeNvs::sets==1);
  reset();FakeNvs::failCommit=1;result=linkExplicitRemote(7,555,42);
  assert(!result.applied && somfy.shades[0].linkedRemotes[0].getRemoteAddress()==0 && !fileWrites);
  reset();for(unsigned i=0;i<SOMFY_MAX_LINKED_REMOTES;++i)somfy.shades[0].linkedRemotes[i].setRemoteAddress(200+i);
  result=linkExplicitRemote(7,555,42);assert(result.state==CounterDomainState::full && !FakeNvs::opens && !fileWrites);
  reset();somfy.shades[0].lastRollingCode=1000;result=correctExplicitCounter(7,1000);
  assert(result.saved() && FakeNvs::sets==1 && !somfy.shades[0].lastFrame.valid); // Same-value correction still verifies and revokes repeats.
  reset();somfy.shades[0].gpioRelease=1;assert(!correctExplicitCounter(7,100).saved() && !FakeNvs::opens);
  reset();somfy.shades[0].lastFrame.processed=false;somfy.shades[0].lastFrame.await=1;assert(!correctExplicitCounter(7,100).saved() && !FakeNvs::opens);
  reset();somfy.shades[0].markTxPending(99);assert(!correctExplicitCounter(7,100).saved() && !FakeNvs::opens);
  somfy_tx_event_t cancelled;cancelled.ticket=99;cancelled.type=tx_event_t::aborted;somfy.shades[0].processTxEvent(cancelled);
  reset();
  const_cast<ShadeBehaviour::State &>(somfy.shades[0].state()).direction=1;
  assert(!correctExplicitCounter(7,100).saved() && !FakeNvs::opens);
  reset();GitUpdateOwnerToken foreign;assert(gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,foreign));
  assert(correctExplicitCounter(7,100).state==CounterDomainState::busy && !FakeNvs::opens);
  assert(gitUpdate.ownsExternal(foreign));assert(gitUpdate.releaseExternal(foreign));
  reset();pendingRestart=true;assert(!correctExplicitCounter(7,100).saved() && !FakeNvs::opens);
  reset();auto &related=somfy.groups[2];related.setGroupId(9);related.setRemoteAddress(654);related.linkedShades[0]=7;
  somfy_tx_job_t job;job.frame.remoteAddress=654;
  assert(somfy.transceiver.sched.submit(job,false,0,0));
  assert(!correctExplicitCounter(7,100).saved() && !FakeNvs::opens);
  uint32_t wait=0;assert(somfy.transceiver.sched.next(0,job,wait));
  somfy.transceiver.sched.flush(0);
  assert(!correctExplicitCounter(7,100).saved() && !FakeNvs::opens); // Taken survives flush.
  somfy.transceiver.sched.ended(job.ticket,false,0);
  assert(correctExplicitCounter(7,100).saved());
  reset();job={};job.repeater=true;assert(somfy.transceiver.sched.submit(job,false,0,0));
  assert(!correctExplicitCounter(7,100).saved() && !FakeNvs::opens);
  reset();tx_queue.length=1;assert(!correctExplicitCounter(7,100).saved() && !FakeNvs::opens);
  reset();somfy.transceiver.sched.suspend(1,0);assert(!correctExplicitCounter(7,100).saved() && !FakeNvs::opens);
  reset();assert(correctExplicitCounter(255,100).state==CounterDomainState::missing && !FakeNvs::opens);
  assert(linkExplicitRemote(7,0,100).state==CounterDomainState::invalid && !FakeNvs::opens);
  reset();for(unsigned i=0;i<SOMFY_MAX_LINKED_REMOTES;++i)somfy.shades[0].linkedRemotes[i].setRemoteAddress(200+i);
  assert(linkExplicitRemote(7,203,55).saved() && somfy.shades[0].linkedRemotes[3].lastRollingCode==55);
  reset();for(unsigned i=0;i<SOMFY_MAX_SHADES;++i) {
    auto &shade=somfy.shades[i];shade.setShadeId(i+1);shade.setRemoteAddress(address);shade.lastRollingCode=1000+i;
    for(auto &remote:shade.linkedRemotes){remote.setRemoteAddress(address);remote.lastRollingCode=2000+i;}
  }
  for(unsigned i=0;i<SOMFY_MAX_GROUPS;++i) {
    auto &group=somfy.groups[i];group.setGroupId(i+1);group.setRemoteAddress(address);group.lastRollingCode=3000+i;
    for(unsigned j=0;j<SOMFY_MAX_GROUPED_SHADES;++j)group.linkedShades[j]=j+1;
  }
  assert(correctExplicitCounter(7,65535).saved());
  for(auto &shade:somfy.shades){assert(shade.lastRollingCode==65535);for(auto &remote:shade.linkedRemotes)assert(remote.lastRollingCode==65535);}
  for(auto &group:somfy.groups)assert(group.lastRollingCode==65535);
  assert(!forbidden);puts("Production explicit counter: verified aliases/current-file, no replay, typed failure and file-only recovery passed.");
}
