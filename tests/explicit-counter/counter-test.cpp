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
#include "CounterCommandGuard.h"
#include "CounterTxPolicy.h"
#include "LoopQueue.h"
#include "ModelCommitService.h"
#include "SomfyEditor.h"
#include "CounterResults.h"
#include "SafeRestart.h"
#include "LocalTargetRead.h"
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
  uint64_t configurationRevision=1;
  RadioScheduler sched;CounterEditSafety counterEdits;
  CounterEditAdmission reserveCounterEdit(uint32_t,bool,uint16_t,bool);
  void finishCounterEdit(CounterEditResult,ModelCommitResult);
  CounterEditAdmission reserveCounterFileRecovery(bool);
  void finishCounterFileRecovery(ModelCommitResult);
  uint32_t counterFileRecoveryAddress();CounterQuarantine counterQuarantine();
  bool counterBlocksIdentity(uint32_t);bool counterEditUncertain();
  uint32_t blockedCounterAddress();
  uint32_t submitTx(somfy_tx_job_t &,bool=false,uint16_t=0);
  void serviceRepeater();bool txSuspended();bool available(){return true;}
};
#include "Somfy.h"
// The extracted unlink helper only constructs this storage boundary; it writes no NVS.
class Preferences {};
static somfy_tx_queue_t tx_queue;
static QueueHandle_t modelQueue=nullptr;
static QueueHandle_t trackQueueCreate(unsigned capacity,unsigned bytes){modelQueue=xQueueCreate(capacity,bytes);return modelQueue;}
static bool requireCounterLease=true;
static unsigned modelReplies=0,modelEvents=0,editReplies=0;
static edit_result_t lastEdit;
static ModelCommitReport lastModelReport;
#define PROTO_SOCKET_H
struct ModelSocket {
  void replyEdit(const proto_reply_to_t &,model_edit_type_t,const edit_result_t &result){++editReplies;lastEdit=result;}
  void room(SomfyRoom &){assert(false);}void order(sort_list_t){assert(false);}void repeaters(){assert(false);}
  void shadeRemoved(uint8_t){assert(false);}void groupRemoved(uint8_t){assert(false);}void roomRemoved(uint8_t){assert(false);}
  void replyModelCommit(const proto_reply_to_t &,ModelCommitReport report){++modelReplies;lastModelReport=report;}
  void modelPersistence(ModelCommitReport,uint32_t){++modelEvents;}
} protoSocket;
RestartResult requestRestart(uint32_t){assert(false);return {};}
RestartResult requestOwnedRestart(const GitUpdateOwnerToken &,uint32_t){assert(false);return {};}

static unsigned notifies=0;
static void *txTaskHandle=reinterpret_cast<void *>(1);
static void xTaskNotifyGive(void *){++notifies;}
static constexpr uint32_t TX_QUEUE_DELAY=100;
static std::mutex txLock;
struct TxQueueLock {std::lock_guard<std::mutex> held;TxQueueLock():held(txLock){}};
SomfyShadeController::SomfyShadeController() : startingAddress(100) {modelLoad.bootResult(ModelLoadStatus::loaded);}
SomfyShadeController somfy;
static bool pendingRestart=false;
bool restartPending(){return pendingRestart;}
// Queue/history forwarding is exercised by the actual queue/service harness.
void noteRestartOperationAccepted(){}
static unsigned fileWrites=0,forbidden=0,setupFrames=0;
static bool allowSetupSend=false,setupEcho=false;
static unsigned long modelNow=1;
static unsigned long modelClock(){return modelNow;}
LoopQueue loopQueue;
static ModelCommitResult fileResult{ModelCommitState::saved};
static uint16_t diskShade[SOMFY_MAX_SHADES],diskGroup[SOMFY_MAX_GROUPS];
static uint32_t diskLinks[SOMFY_MAX_SHADES][SOMFY_MAX_LINKED_REMOTES];
ModelCommitResult SomfyShadeController::commit() {
  ++fileWrites;
  if(requireCounterLease) {
    assert(gitUpdate.busy() && somfy.transceiver.counterEdits.active());GitUpdateOwnerToken competing;
    assert(!gitUpdate.acquireExternal(GitUpdateExternalOwner::manual,competing));
  }
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
  if(allowSetupSend){++setupFrames;lastFrame.valid=true;lastFrame.cmd=cmd;lastFrame.remoteAddress=getRemoteAddress();lastFrame.rollingCode=100;if(setupEcho)somfy.processFrame(lastFrame,true);return true;}
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
#define xQueueCreate trackQueueCreate
#define millis modelClock
#include "production-counter.inc"
#undef millis
#undef xQueueCreate
#include "../../src/ExplicitCounter.cpp"
#include "../../src/CounterCommandGuard.cpp"
#include "../../src/ModelCommitService.cpp"
#include "production-link-dispatch.inc"
#include "production-link-fanout.inc"
static constexpr uint32_t address=12345;
static void reset() {
  assert(!gitUpdate.busy() && !forbidden);requireCounterLease=true;modelReplies=modelEvents=editReplies=0;lastModelReport={};lastEdit={};FakeNvs::reset();pendingRestart=false;fileWrites=forbidden=setupFrames=0;allowSetupSend=setupEcho=false;modelNow=1;fileResult={ModelCommitState::saved};
  memset(diskShade,0,sizeof(diskShade));memset(diskGroup,0,sizeof(diskGroup));memset(diskLinks,0,sizeof(diskLinks));
  tx_queue.length=0;somfy.transceiver.sched={};somfy.transceiver.counterEdits={};somfy.configurationRevision=1;
  for(auto &s:somfy.shades){s.clear();s.lastFrame={};s.gpioRelease=0;s.configurationRevision=1;for(auto &remote:s.linkedRemotes)remote.lastRollingCode=0;}
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
  if(argc>1 && !strcmp(argv[1],"badLoad")) {
    reset();somfy.modelLoad.fail();assert(!correctExplicitCounter(7,100).saved());assert(!linkExplicitRemote(7,999,100).saved());assert(!recoverExplicitCounterFile().saved());
    assert(!FakeNvs::opens && !forbidden);return 0;
  }
  if(argc>1 && !strcmp(argv[1],"sharedLink")) {
    for(unsigned failure=0;failure<6;++failure) {
      reset();auto &other=somfy.shades[1];other.setShadeId(22);other.setRemoteAddress(999);
      if(failure==0)const_cast<ShadeBehaviour::State &>(other.state()).direction=1;
      if(failure==1){assert(persistExplicitCounter(555,500).saved());}
      if(failure==2)FakeNvs::failRead=1;
      if(failure==5)FakeNvs::failSet=1;
      model_edit_t edit;edit.type=model_edit_type_t::link_remote;edit.doc["shadeId"]=7;edit.doc["remoteAddress"]=555;edit.doc["rollingCode"]=100;
      const auto before=somfy.configurationRevision,entity=somfy.shades[0].configurationRevision;
      const auto sets=FakeNvs::sets;
      if(failure==3)fileResult={ModelCommitState::failed,ModelCommitStage::write};
      const auto result=applyModelEdit(edit);
      if(failure<3 || failure==5) {
        assert(!result.applied && somfy.configurationRevision==before && somfy.shades[0].configurationRevision==entity);
        assert(FakeNvs::sets==sets+(failure==5?1:0) && !fileWrites && editReplies==1 && !modelEvents && !somfy.shades[0].linkedRemotes[0].getRemoteAddress());
        assert(result.failure==(failure==0?edit_failure_t::busy:failure==1?edit_failure_t::badRequest:edit_failure_t::internal));
      } else {
        assert(result.applied && result.ok==(failure==4) && result.id==7 && fileWrites==1 && FakeNvs::sets==sets+1);
        assert(somfy.configurationRevision==before+1 && somfy.shades[0].configurationRevision>entity && modelEvents==1 && editReplies==1);
        assert(somfy.transceiver.counterQuarantine()==(failure==4?CounterQuarantine::none:CounterQuarantine::filePending));
      }
    }
    puts("Production dispatcher prefix/link case + complete apply fanout preserve refused history and publish applied file failure once");return 0;
  }
  if(argc>1 && !strcmp(argv[1],"globalIdle")) {
    for(unsigned busy=0;busy<3;++busy) {
      reset();auto &other=somfy.shades[1];other.setShadeId(22);other.setRemoteAddress(999);
      if(busy==0)const_cast<ShadeBehaviour::State &>(other.state()).direction=1;
      else if(busy==1){other.lastFrame.await=1;other.lastFrame.processed=false;}
      else other.gpioRelease=1;
      LocalUi::RawTargetContext before,after;
      assert(LocalUi::readRawTargetContext(somfy,{LocalUi::EntityKind::device,22},before));
      for(bool link:{false,true}) {
        const auto result=link ? linkExplicitRemote(7,555,100) : correctExplicitCounter(7,100);
        assert(result.state==CounterDomainState::busy && !result.applied && !FakeNvs::opens && !fileWrites);
        assert(LocalUi::readRawTargetContext(somfy,{LocalUi::EntityKind::device,22},after) && before.same(after));
        assert(somfy.configurationRevision==1 && somfy.shades[0].lastFrame.valid && !gitUpdate.busy());
      }
    }
    puts("Unrelated moving/held/pulse target preserves raw STOP context; counter/link refuse before owner/NVS/history");return 0;
  }
  if(argc>1 && !strcmp(argv[1],"browserCommit")) {
    reset();fileResult={ModelCommitState::failed,ModelCommitStage::write};assert(linkExplicitRemote(7,555,100).applied);
    const auto sets=FakeNvs::sets;const auto revision=somfy.configurationRevision;
    fileResult={ModelCommitState::saved};requireCounterLease=false;
    assert(applyModelCommit().file.saved());requireCounterLease=true;
    assert(somfy.transceiver.counterQuarantine()==CounterQuarantine::filePending);
    assert(submitModelCommit({8,99},ModelCommitMode::counterFileRecovery));
    loop_item_t marker;assert(xQueueReceive(modelQueue,&marker,0));
    assert(marker.op==uint8_t(ModelCommitMode::counterFileRecovery) && !marker.externalOwner.generation);
    runModelCommit(marker);
    assert(lastModelReport.file.saved() && somfy.transceiver.counterQuarantine()==CounterQuarantine::none);
    assert(FakeNvs::sets==sets && somfy.configurationRevision==revision && modelReplies==2 && modelEvents==2);
    reset();FakeNvs::failSet=1;assert(correctExplicitCounter(7,100).state==CounterDomainState::nvsUncertain);
    assert(submitModelCommit({},ModelCommitMode::counterFileRecovery));assert(xQueueReceive(modelQueue,&marker,0));runModelCommit(marker);
    assert(!fileWrites && FakeNvs::sets==1 && somfy.configurationRevision==1 && lastModelReport.refusal==RestartReason::counterUncertain);
    assert(somfy.transceiver.counterQuarantine()==CounterQuarantine::nvsUnknown && modelReplies==1 && modelEvents==1);
    puts("Real model + queue + commit service: explicit file-only recovery, unrelated-save isolation, unknown NVS refusal and no replay/history/NVS writes passed");return 0;
  }
  if(argc>1 && !strcmp(argv[1],"learnBound")) {
    reset();assert(linkExplicitRemote(7,555,500).saved());assert(somfy.shades[0].unlinkRemote(555));
    const auto sets=FakeNvs::sets;const auto revision=somfy.configurationRevision;const auto writableBefore=FakeNvs::writeOpens;
    assert(linkExplicitRemote(7,555,400).state==CounterDomainState::stale);assert(FakeNvs::writeOpens==writableBefore);
    assert(FakeNvs::sets==sets && somfy.configurationRevision==revision && !somfy.shades[0].linkedRemotes[0].getRemoteAddress());
    assert(linkExplicitRemote(7,555,65535).saved());assert(somfy.shades[0].unlinkRemote(555));
    const auto maxSets=FakeNvs::sets;assert(linkExplicitRemote(7,555,0).state==CounterDomainState::stale && FakeNvs::sets==maxSets);
    reset();auto &alias=somfy.shades[1];alias.setShadeId(22);alias.setRemoteAddress(555);alias.lastRollingCode=700;
    assert(linkExplicitRemote(7,555,600).state==CounterDomainState::stale && !FakeNvs::sets && !FakeNvs::opens && somfy.configurationRevision==1);
    reset();auto &group=somfy.groups[0];group.setGroupId(3);group.setRemoteAddress(555);group.lastRollingCode=700;group.linkedShades[0]=7;
    assert(linkExplicitRemote(7,555,600).state==CounterDomainState::stale && !FakeNvs::sets);
    reset();FakeNvs::failRead=1;assert(linkExplicitRemote(7,555,100).state==CounterDomainState::nvsFailed);
    assert(!FakeNvs::sets && !FakeNvs::writeOpens && somfy.configurationRevision==1 && somfy.transceiver.counterQuarantine()==CounterQuarantine::none);
    reset();FakeNvs::readOnlyNamespaceMissing=true;
    assert(linkExplicitRemote(7,555,100).saved() && FakeNvs::readOnlyOpens==1 && FakeNvs::writeOpens==1 && FakeNvs::sets==1);
    reset();FakeNvs::beforeGet=[] {
      FakeNvs::beforeGet=nullptr;assert(gitUpdate.busy() && somfy.transceiver.counterBlocksIdentity(555));
      assert(linkExplicitRemote(7,777,50).state==CounterDomainState::busy && !FakeNvs::sets);
    };
    assert(linkExplicitRemote(7,555,100).saved());assert(FakeNvs::sets==1);
    assert(somfy.shades[0].unlinkRemote(555));assert(persistExplicitCounter(555,800).saved()); // Newer retained identity since a candidate was captured.
    const auto laterSets=FakeNvs::sets;assert(linkExplicitRemote(7,555,100).state==CounterDomainState::stale && FakeNvs::sets==laterSets);
    assert(correctExplicitCounter(7,10).saved()); // Deliberate primary correction is a separate contract.
    assert(!forbidden);puts("Checked received link: retained NVS/aliases, no wrap/read-error fallback, reservation freshness and explicit-lower separation passed");return 0;
  }
  if(argc>1 && !strcmp(argv[1],"aliasRisk")) {
    for(bool quarantined:{false,true}) {
      reset();if(quarantined){FakeNvs::failSet=1;assert(correctExplicitCounter(7,100).state==CounterDomainState::nvsUncertain);}
      auto &primary=somfy.shades[0];const_cast<ShadeBehaviour::State &>(primary.state()).direction=1;
      auto &alias=somfy.shades[1];alias.setShadeId(22);alias.setRemoteAddress(555);alias.restore(20,0,50,-1);
      auto &group=somfy.groups[0];group.setGroupId(3);group.setRemoteAddress(555);group.linkedShades[0]=7;
      allowSetupSend=setupEcho=true;const bool sent=group.sendCommand(somfy_commands::My,1);allowSetupSend=setupEcho=false;
      if(quarantined)assert(!sent && setupFrames==0 && alias.state().target==20 && primary.state().direction==1);
      else {assert(sent && alias.state().target==50);modelNow=2;alias.checkMovement();assert(alias.state().direction==1);}
      assert(!forbidden);
    }
    puts("Actual group/echo/behaviour: idle alias favorite hazard preserved normally, refused under counter STOP exception");return 0;
  }
  if(argc>1 && !strcmp(argv[1],"commandGuard")) {
    reset();FakeNvs::failSet=1;assert(correctExplicitCounter(7,100).state==CounterDomainState::nvsUncertain);
    const auto opens=FakeNvs::opens,reads=FakeNvs::reads,sets=FakeNvs::sets;
    assert(!counterCommandAllowed(address,somfy_commands::My));assert(counterCommandAllowed(999,somfy_commands::My));
    auto &other=somfy.shades[1];other.setShadeId(22);other.setRemoteAddress(999);
    auto &group=somfy.groups[15];group.setGroupId(16);group.setRemoteAddress(555);group.proto=radio_proto::RTS;
    group.linkedShades[0]=7;group.linkedShades[31]=22;
    assert(!counterCommandAllowed(555,somfy_commands::Up) && !counterCommandAllowed(555,somfy_commands::My));
    const_cast<ShadeBehaviour::State &>(somfy.shades[0].state()).direction=1;
    assert(!counterCommandAllowed(555,somfy_commands::My)); // mixed group never idle My.
    const_cast<ShadeBehaviour::State &>(other.state()).direction=1;
    assert(counterCommandAllowed(555,somfy_commands::My));
    assert(!counterCommandAllowed(555,somfy_commands::Sensor));
    other.shadeType=shade_types::drycontact;assert(!counterCommandAllowed(555,somfy_commands::My));
    other.shadeType=shade_types::drycontact2;assert(!counterCommandAllowed(555,somfy_commands::My));
    other.shadeType=shade_types::garage1;assert(!counterCommandAllowed(555,somfy_commands::My));
    other.shadeType=shade_types::roller;other.bitLength=80;assert(!counterCommandAllowed(555,somfy_commands::My));
    other.bitLength=56;other.proto=radio_proto::GP_Remote;assert(!counterCommandAllowed(555,somfy_commands::My));
    other.proto=radio_proto::RTS;group.linkedShades[1]=31;assert(!counterCommandAllowed(555,somfy_commands::My));group.linkedShades[1]=0;
    other.linkedRemotes[0].setRemoteAddress(555);assert(!counterCommandAllowed(555,somfy_commands::My));other.linkedRemotes[0].setRemoteAddress(0);
    somfy.groups[0].setGroupId(3);somfy.groups[0].setRemoteAddress(555);assert(!counterCommandAllowed(555,somfy_commands::My));
    assert(FakeNvs::opens==opens && FakeNvs::reads==reads && FakeNvs::sets==sets);
    puts("Counter group guard: independent all-moving STOP only, mixed/idle/ambiguous/types refused");return 0;
  }
  if(argc>1 && !strcmp(argv[1],"sink")) {
    reset();assert(somfy.transceiver.counterEdits.reserve(address,true,false,false)==CounterEditAdmission::admitted);
    somfy.transceiver.counterEdits.finish({CounterEditState::uncertain},{});auto &radio=somfy.transceiver;
    somfy_tx_job_t job;job.frame.remoteAddress=address;job.tx.bit_length=56;job.ticket=999;
    assert(!radio.submitTx(job) && !job.ticket && !notifies && !radio.sched.hasPendingFor(address));
    job.frame.remoteAddress=999;assert(radio.submitTx(job) && radio.sched.hasPendingFor(999));radio.sched={};
    somfy_frame_t frame;frame.remoteAddress=address;frame.rollingCode=100;frame.cmd=somfy_commands::Up;frame.bitLength=56;
    encodeTxJob(job,frame,1);job.repeater=true;job.frame={};
    byte original[10];memcpy(original,job.tx.payload,10);assert(!radio.submitTx(job) && !memcmp(original,job.tx.payload,10));
    frame.remoteAddress=999;encodeTxJob(job,frame,1);job.repeater=true;job.frame={};assert(radio.submitTx(job));radio.sched={};
    byte bad[10]={};tx_queue.push(4,bad,56);
    frame.remoteAddress=999;frame.cmd=somfy_commands::My;encodeTxJob(job,frame,1);tx_queue.push(4,job.tx.payload,56);
    radio.serviceRepeater();assert(tx_queue.length==1 && !radio.sched.hasRepeaterWork());
    radio.serviceRepeater();assert(!tx_queue.length && radio.sched.hasRepeaterWork());radio.sched={};
    auto &group=somfy.groups[0];group.setGroupId(3);group.setRemoteAddress(555);group.linkedShades[0]=7;
    frame.remoteAddress=555;frame.cmd=somfy_commands::Up;encodeTxJob(job,frame,1);tx_queue.push(4,job.tx.payload,56);
    radio.serviceRepeater();assert(!tx_queue.length && !radio.sched.hasRepeaterWork());
    const_cast<ShadeBehaviour::State &>(somfy.shades[0].state()).direction=1;
    frame.cmd=somfy_commands::My;encodeTxJob(job,frame,1);tx_queue.push(4,job.tx.payload,56);
    radio.serviceRepeater();assert(!tx_queue.length && radio.sched.hasRepeaterWork());
    assert(!FakeNvs::opens && !forbidden);puts("Actual TX sink/raw head: exact identity, immutable payload, one refused head, unrelated/qualified STOP preserved");return 0;
  }
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
