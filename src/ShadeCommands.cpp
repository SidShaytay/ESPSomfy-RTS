#include "Somfy.h"
#include "GroupMotion.h"
#include "CounterCommandGuard.h"
extern SomfyShadeController somfy;

void SomfyShade::prepareCommand(uint32_t exceptTicket) {
  somfy.cancelGroupStartsForShade(this->getShadeId());
  somfy.transceiver.cancelQueuedForRemote(this->getRemoteAddress(), exceptTicket);
  this->behaviour.cancelPositioning();
}
void SomfyShade::userOverride() { somfy.cancelGroupReplansForShade(this->getShadeId()); }
bool SomfyShade::supportsGroupTarget() const {
  const bool percentageType = this->shadeType == shade_types::roller || this->shadeType == shade_types::blind ||
    this->shadeType == shade_types::awning || this->shadeType == shade_types::shutter ||
    this->shadeType == shade_types::ldrapery || this->shadeType == shade_types::rdrapery || this->shadeType == shade_types::cdrapery;
  return percentageType && this->tiltType == tilt_types::none && this->upTime > 0 && this->downTime > 0;
}
bool SomfyShade::sendCommand(somfy_commands cmd) { return this->sendCommand(cmd, this->repeats); }
bool SomfyShade::sendCommand(somfy_commands cmd, uint8_t repeat, uint8_t stepSize) { return sendCommandReceipt(cmd,repeat,stepSize).sent; }
send_result_t SomfyShade::sendCommandReceipt(somfy_commands cmd, uint8_t repeat, uint8_t stepSize) {
  if(!somfy.modelLoad.usable() || !counterCommandAllowed(this->getRemoteAddress(),cmd) || !this->canGpioCommand(cmd)) return {};
  if(cmd==somfy_commands::My && !this->isToggle() && this->shadeType!=shade_types::drycontact && (this->simMy || this->proto==radio_proto::GP_Relay) && this->isIdle() && !this->canGpioSeek(this->tiltType!=tilt_types::none)) return {};
  if((cmd == somfy_commands::My || cmd == somfy_commands::Stop) &&
      (this->hasPendingGroupStart() || this->startQueuedOnly() || this->behaviour.ownsGroupPositioning()))
    { const bool accepted = this->stopGroupTarget(); if(accepted && commandSupersedesPositioning(cmd)) userOverride(); return {accepted,0}; }
  if(commandSupersedesPositioning(cmd) && this->hasPendingGroupStart()) {
    // Coalesce a directional press with the unfinished start; it is applied
    // as an endpoint target once the start's outcome is known.
    if((cmd == somfy_commands::Up || cmd == somfy_commands::Down) && this->supportsGroupTarget())
      { const bool accepted = this->behaviour.moveToGroupTarget(cmd == somfy_commands::Up ? 0 : 100, millis()); if(accepted && commandSupersedesPositioning(cmd)) userOverride(); return {accepted,0}; }
    return {};
  }
  // This sendCommand function will always be called externally. sendCommand at the remote level
  // is expected to be called internally when the motor needs commanded.
  if(this->bitLength == 0) this->bitLength = somfy.transceiver.config.type;
  this->prepareOnSend = commandSupersedesPositioning(cmd);
  send_result_t accepted;
  accepted.sent = this->behaviour.sendCommand(cmd, repeat, stepSize, millis(), &accepted.ticket);
  this->prepareOnSend = false;
  if(accepted && commandSupersedesPositioning(cmd)) userOverride();
  return accepted;
}
bool SomfyShade::sendTiltCommand(somfy_commands cmd) {
  if(!somfy.modelLoad.usable() || !counterCommandAllowed(this->getRemoteAddress(),cmd) || !this->canGpioTilt(cmd)) return false;
  if(this->hasPendingGroupStart()) return false;
  this->prepareOnSend = true;
  const bool accepted = this->behaviour.sendTiltCommand(cmd, millis());
  this->prepareOnSend = false;
  if(accepted) userOverride();
  return accepted;
}
bool SomfyShade::moveToTarget(float pos, float tilt) {
  if(!somfy.modelLoad.usable() || !counterCommandAllowed(this->getRemoteAddress(),somfy_commands::UpDown) || !this->canGpioSeek(tilt>=0 || this->tiltType==tilt_types::tiltonly)) return false;
  if(this->hasPendingGroupStart()) {
    // Keep the unfinished start; apply this member's latest target after it.
    if(tilt < 0 && this->supportsGroupTarget()) {
          const bool accepted = this->behaviour.moveToGroupTarget(pos, millis());
      if(accepted) userOverride();
      return accepted;
    }
    return false;
  }
  if(this->behaviour.ownsGroupPositioning() && tilt < 0) {
    const bool accepted = this->moveToGroupTarget(pos);
    if(accepted) userOverride();
    return accepted;
  }
  this->prepareOnSend = true;
  const bool accepted = this->behaviour.moveToTarget(pos, tilt, millis());
  this->prepareOnSend = false;
  if(accepted) userOverride();
  return accepted;
}
void SomfyShade::cancelQueuedStart() {
  if(!behaviour.startQueuedOnly()) return;
  if(somfy.transceiver.cancelUnstartedTx(behaviour.queuedStartTicket())) behaviour.cancelPositioning();
  else awaitQueuedStart();
}
bool SomfyShade::moveToGroupTarget(float pos) {
  if(!somfy.modelLoad.usable() || !counterCommandAllowed(this->getRemoteAddress(),somfy_commands::UpDown)) return false;
  // A press still queued has not moved the motor: cancel it so it is
  // replaced rather than stopped with My.
  this->cancelQueuedStart();
  if(behaviour.ownsGroupPositioning() && behaviour.hasStopPending()) {
    behaviour.refreshTravel(millis());
    const auto &s = state();
    if(((s.direction > 0 && pos > s.currentPos) || (s.direction < 0 && pos < s.currentPos)) &&
        somfy.transceiver.cancelUnstartedTx(behaviour.queuedStopTicket())) {
      behaviour.cancelGroupStop(pos, millis());
      return true;
    }
  }
  this->prepareOnSend = true;
  const bool accepted = this->behaviour.moveToGroupTarget(pos, millis());
  this->prepareOnSend = false;
  return accepted;
}
bool SomfyShade::stopGroupTarget() {
  if(!somfy.modelLoad.usable() || !counterCommandAllowed(this->getRemoteAddress(),somfy_commands::My)) return false;
  if(this->hasPendingGroupStart()) {
    // Drop this member's own start if it is still queued; a shared start
    // already on air is stopped once its outcome is known.
    somfy.transceiver.cancelQueuedForRemote(this->getRemoteAddress());
    return this->behaviour.stopGroupTarget(millis());
  }
  if(this->startQueuedOnly()) {
    cancelQueuedStart();
    if(hasPendingGroupStart()) return behaviour.stopGroupTarget(millis());
    this->emitState();
    return true;
  }
  this->prepareOnSend = true;
  const bool accepted = this->behaviour.stopGroupTarget(millis());
  this->prepareOnSend = false;
  return accepted;
}
bool SomfyShade::moveToTiltTarget(float target) {
  if(!somfy.modelLoad.usable() || !counterCommandAllowed(this->getRemoteAddress(),somfy_commands::UpDown) || !this->canGpioSeek(true)) return false;
  if(this->hasPendingGroupStart()) return false;
  this->prepareOnSend = true;
  const bool accepted = this->behaviour.moveToTiltTarget(target, millis());
  this->prepareOnSend = false;
  if(accepted) userOverride();
  return accepted;
}
bool SomfyShade::setMyPosition(int8_t pos, int8_t tilt) {
  if(!somfy.modelLoad.usable() || !counterCommandAllowed(this->getRemoteAddress(),somfy_commands::UpDown) || !this->canGpioSeek(this->tiltType!=tilt_types::none)) return false;
  if(this->hasPendingGroupStart()) return false;
  const bool accepted = this->behaviour.setMyPosition(pos, tilt, millis());
  if(accepted) userOverride();
  return accepted;
}
bool SomfyShade::moveToMyPosition() {
  if(!somfy.modelLoad.usable() || !counterCommandAllowed(this->getRemoteAddress(),somfy_commands::My) || !this->canGpioCommand(somfy_commands::My)) return false;
  if(this->hasPendingGroupStart() || ((this->simMy || this->proto==radio_proto::GP_Relay) && !this->canGpioSeek(this->tiltType!=tilt_types::none))) return false;
  this->prepareOnSend = true;
  const bool accepted = this->behaviour.moveToMyPosition(millis());
  this->prepareOnSend = false;
  if(accepted) userOverride();
  return accepted;
}
bool SomfyShade::programMyPositionHere(bool clear) {
  if(!somfy.modelLoad.usable() || !counterCommandAllowed(this->getRemoteAddress(),somfy_commands::My) ||
     !this->canGpioCommand(somfy_commands::My) || !this->canGpioSeek(this->tiltType!=tilt_types::none)) return false;
  return this->behaviour.programMyPositionHere(clear,millis());
}
