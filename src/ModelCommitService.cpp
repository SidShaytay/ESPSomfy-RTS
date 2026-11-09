#include "ModelCommitService.h"
#include "LoopQueue.h"
#include "Somfy.h"
#include "Utils.h"
#include "LegacyRestartSource.h"
#include "LegacyRestartGuard.h"
#include "outputs/ProtoSocket.h"
#include <esp_timer.h>
extern LoopQueue loopQueue;
extern SomfyShadeController somfy;
extern rebootDelay_t rebootDelay;
namespace {
uint64_t now() { return uint64_t(esp_timer_get_time())/1000; }
void notify(const proto_reply_to_t &to,ModelCommitReport report) {
  protoSocket.replyModelCommit(to,report);protoSocket.modelPersistence(report,to.client);
}
ModelCommitReport commit(const proto_reply_to_t &to,ModelCommitMode mode,const GitUpdateOwnerToken *owner) {
  ModelCommitReport report{somfy.commit()};
  const bool requested=mode==ModelCommitMode::applicationRecoveryAndRestart && report.file.saved();
  RestartResult admission;
  if(requested) {
    if(loopQueue.pending()) admission={false,RestartReason::queuedOperations};
    else if(owner) admission={gitUpdate.transferRestartHeld(*owner,now()),RestartReason::stale};
    else admission=legacyGenericRestartAdmission();
    if(rebootDelay.reboot) admission={false,RestartReason::pending};
    report.restartDeferred=!admission.admitted;
    if(report.restartDeferred) report.refusal=admission.reason;
    report.rebooting=admission.admitted;
  }
  notify(to,report);
  // Owned recovery retains its operation claim through notification and source
  // publication; another upload/counter edit cannot replace that authority.
  if(report.rebooting) {
    const auto published=owner
        ? publishExternalLegacyRestart(rebootDelay,millis()+500,owner->generation,uint8_t(owner->kind))
        : publishGenericLegacyRestart(rebootDelay,millis()+500);
    if(!published.admitted) {
      report.rebooting=false;report.restartDeferred=true;report.refusal=published.reason;
      protoSocket.modelPersistence(report,to.client);
    }
  }
  return report;
}
}
bool submitModelCommit(const proto_reply_to_t &to,ModelCommitMode mode,const GitUpdateOwnerToken &owner) {
  if(static_cast<uint8_t>(mode)>static_cast<uint8_t>(ModelCommitMode::applicationRecoveryAndRestart) ||
     (mode!=ModelCommitMode::save && !owner.generation)) return false;
  loop_item_t item;item.type=loop_item_type_t::model_commit;item.replyTo=to;
  item.restartAfterCommit=mode==ModelCommitMode::applicationRecoveryAndRestart;
  item.externalOwner=owner;
  // Preserve the existing upload-recovery exception; no new priority bypass.
  return loopQueue.submit(item,mode!=ModelCommitMode::save);
}
ModelCommitReport applyModelCommit(const proto_reply_to_t &to,ModelCommitMode mode) {
  return commit(to,mode,nullptr);
}
void runModelCommit(const loop_item_t &item) {
  const auto &owner=item.externalOwner;
  if((item.restartAfterCommit && !owner.generation) || (owner.generation && !gitUpdate.claimTransferRecovery(owner,now()))) {
    ModelCommitReport refused;refused.refusal=RestartReason::stale;notify(item.replyTo,refused);return;
  }
  const auto mode=item.restartAfterCommit ? ModelCommitMode::applicationRecoveryAndRestart : ModelCommitMode::save;
  const auto result=commit(item.replyTo,mode,owner.generation ? &owner : nullptr);
  if(!owner.generation) return;
  if(result.rebooting) gitUpdate.finishTransfer(owner,now(),false);
  else if(gitUpdate.closeTransfer(owner)) gitUpdate.releaseExternal(owner);
}
