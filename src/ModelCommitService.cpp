#include "ModelCommitService.h"
#include "LoopQueue.h"
#include "Somfy.h"
#include "Utils.h"
#include "SafeRestart.h"
#include "ExplicitCounter.h"
#include "outputs/ProtoSocket.h"
#include <esp_timer.h>
extern LoopQueue loopQueue;
extern SomfyShadeController somfy;

namespace {
uint64_t now() { return uint64_t(esp_timer_get_time())/1000; }
void notify(const proto_reply_to_t &to,ModelCommitReport report) {
  protoSocket.replyModelCommit(to,report);protoSocket.modelPersistence(report,to.client);
}
ModelCommitReport commit(const proto_reply_to_t &to,ModelCommitMode mode,const GitUpdateOwnerToken *owner) {
  ModelCommitReport report;
  if(mode==ModelCommitMode::counterFileRecovery) {
    const auto cause=somfy.transceiver.counterQuarantine();
    if(cause==CounterQuarantine::nvsUnknown) {
      report.refusal=RestartReason::counterUncertain;notify(to,report);return report;
    }
    if(cause==CounterQuarantine::filePending) {
      const auto recovered=recoverExplicitCounterFile();report.file=recovered.file;
      if(recovered.state==CounterDomainState::busy)report.refusal=RestartReason::persistenceBusy;
      else if(recovered.state==CounterDomainState::quarantined || recovered.state==CounterDomainState::invalid)report.refusal=RestartReason::counterUncertain;
    } else report.file=somfy.commit();
  } else report.file=somfy.commit();
  const bool requested=mode==ModelCommitMode::applicationRecoveryAndRestart && report.file.saved();
  if(requested) {
    RestartResult admission;
    if(owner) {
      // Retire this exact file-operation claim before handing the source to
      // restart admission; no callback/cleanup can overtake a held claim.
      admission=gitUpdate.finishTransfer(*owner,now(),false)
          ? requestOwnedRestart(*owner,500) : RestartResult{false,RestartReason::stale};
    } else admission=requestRestart(500);
    report.restartDeferred=!admission.admitted;
    report.refusal=admission.reason;
    report.rebooting=admission.admitted;
  }
  notify(to,report);
  return report;
}
}
bool submitModelCommit(const proto_reply_to_t &to,ModelCommitMode mode,const GitUpdateOwnerToken &owner) {
  if(static_cast<uint8_t>(mode)>static_cast<uint8_t>(ModelCommitMode::counterFileRecovery)) return false;
  const bool application=mode==ModelCommitMode::applicationRecovery || mode==ModelCommitMode::applicationRecoveryAndRestart;
  if((application && (!owner.generation || owner.kind!=GitUpdateExternalOwner::manual)) || (!application && owner.generation)) return false;
  loop_item_t item;item.type=loop_item_type_t::model_commit;item.replyTo=to;item.op=uint8_t(mode);
  item.restartAfterCommit=mode==ModelCommitMode::applicationRecoveryAndRestart;
  item.externalOwner=owner;
  // Preserve the existing upload-recovery exception; no new priority bypass.
  return loopQueue.submit(item,application);
}
ModelCommitReport applyModelCommit(const proto_reply_to_t &to,ModelCommitMode mode) {
  return commit(to,mode,nullptr);
}
void runModelCommit(const loop_item_t &item) {
  const auto &owner=item.externalOwner;
  if(item.op>uint8_t(ModelCommitMode::counterFileRecovery)) {
    ModelCommitReport refused;refused.refusal=RestartReason::invalidRequest;notify(item.replyTo,refused);return;
  }
  const auto mode=ModelCommitMode(item.op);
  const bool application=mode==ModelCommitMode::applicationRecovery || mode==ModelCommitMode::applicationRecoveryAndRestart;
  if(item.restartAfterCommit!=(mode==ModelCommitMode::applicationRecoveryAndRestart) ||
     (application && (!owner.generation || owner.kind!=GitUpdateExternalOwner::manual)) ||
     (!application && owner.generation) || (application && !gitUpdate.claimTransferRecovery(owner,now()))) {
    ModelCommitReport refused;refused.refusal=RestartReason::stale;notify(item.replyTo,refused);return;
  }
  const auto result=commit(item.replyTo,mode,owner.generation ? &owner : nullptr);
  if(!owner.generation) return;
  if(!result.rebooting && gitUpdate.closeTransfer(owner)) gitUpdate.releaseExternal(owner);
}
