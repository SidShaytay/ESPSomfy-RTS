#include "ModelCommitService.h"
#include "LoopQueue.h"
#include "Somfy.h"
#include "Utils.h"
#include "outputs/ProtoSocket.h"
extern LoopQueue loopQueue;
extern SomfyShadeController somfy;
extern rebootDelay_t rebootDelay;
bool submitModelCommit(const proto_reply_to_t &to, ModelCommitMode mode) {
  if (static_cast<uint8_t>(mode) >
      static_cast<uint8_t>(ModelCommitMode::applicationRecoveryAndRestart))
    return false;
  loop_item_t item;
  item.type = loop_item_type_t::model_commit;
  item.replyTo = to;
  item.restartAfterCommit =
      mode == ModelCommitMode::applicationRecoveryAndRestart;
  // Only upload recovery bypasses a held RF head. Ordinary retries keep FIFO.
  return loopQueue.submit(item, mode != ModelCommitMode::save);
}
void runModelCommit(const loop_item_t &item) {
  ModelCommitReport report{somfy.commit()};
  const bool requested = item.restartAfterCommit && report.file.saved();
  report.restartDeferred = requested && loopQueue.pending() != 0;
  report.rebooting = requested && !report.restartDeferred;
  protoSocket.replyModelCommit(item.replyTo, report);
  protoSocket.modelPersistence(report, item.replyTo.client);
  if (report.rebooting) {
    rebootDelay.rebootTime = millis() + 500;
    rebootDelay.reboot = true;
  }
}
