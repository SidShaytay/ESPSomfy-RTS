#include <Preferences.h>
#include "esp_log.h"
#include "Somfy.h"
#include "GroupMotion.h"

static const char *TAG = "Somfy";

extern SomfyShadeController somfy;

void SomfyShade::clear() {
  this->setShadeId(255);
  this->setRemoteAddress(0);
  this->behaviour.clear();
  this->flipPosition = false;
  this->flipCommands = false;
  this->lastRollingCode = 0;
  this->shadeType = shade_types::roller;
  this->tiltType = tilt_types::none;
  //this->txQueue.clear();
  this->bitLength = somfy.transceiver.config.type;
  this->proto = somfy.transceiver.config.proto;
  for(uint8_t i = 0; i < SOMFY_MAX_LINKED_REMOTES; i++)
    this->linkedRemotes[i].setRemoteAddress(0);
  this->paired = false;
  this->name[0] = 0x00;
  this->upTime = 10000;
  this->downTime = 10000;
  this->tiltTime = 7000;
  this->stepSize = 100;
  this->repeats = 1;
  this->sortOrder = 255;
}
bool SomfyShade::linkRemote(uint32_t address, uint16_t rollingCode) {
  Preferences pref;
  // Check to see if the remote is already linked. If it is
  // just return true after setting the rolling code
  for(uint8_t i = 0; i < SOMFY_MAX_LINKED_REMOTES; i++) {
    if(this->linkedRemotes[i].getRemoteAddress() == address) {
      this->linkedRemotes[i].setRollingCode(rollingCode);
      return true;
    }
  }
  for(uint8_t i = 0; i < SOMFY_MAX_LINKED_REMOTES; i++) {
    if(this->linkedRemotes[i].getRemoteAddress() == 0) {
      this->linkedRemotes[i].setRemoteAddress(address);
      this->linkedRemotes[i].setRollingCode(rollingCode);
      this->commit();
      return true;
    }
  }
  return false;
}
void SomfyShade::commit() { somfy.commit(); }
bool SomfyShade::unlinkRemote(uint32_t address) {
  Preferences pref;
  for(uint8_t i = 0; i < SOMFY_MAX_LINKED_REMOTES; i++) {
    if(this->linkedRemotes[i].getRemoteAddress() == address) {
      this->linkedRemotes[i].setRemoteAddress(0);
      this->commit();
      return true;
    }
  }
  return false;
}
bool SomfyShade::isInGroup() {
  if(this->getShadeId() == 255) return false;
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPS; i++) {
    if(somfy.groups[i].getGroupId() != 255 && somfy.groups[i].hasShadeId(this->getShadeId())) return true;
  }
  return false;
}
uint16_t SomfyShade::p_lastRollingCode(uint16_t code) {
  uint16_t old = SomfyRemote::p_lastRollingCode(code);
  if(old != code) this->changed |= SHADE_CHANGED_ROLLING_CODE;
  return old;
}
uint8_t SomfyShade::getFlags() { return joinShadeFlags(*this, this->light, this->state().stateFlags); }
void SomfyShade::setFlags(uint8_t flags) {
  uint8_t stateFlags = 0;
  splitShadeFlags(flags, *this, this->light, stateFlags);
  this->behaviour.setStateFlags(stateFlags);
}
int8_t SomfyShade::transformPosition(float fpos) { return displayPosition(fpos, this->flipPosition); }
SomfyRemote *SomfyShade::findRemote(uint32_t address) {
  if(this->getRemoteAddress() == address) return this;
  for(uint8_t i = 0; i < SOMFY_MAX_LINKED_REMOTES; i++) {
    if(this->linkedRemotes[i].getRemoteAddress() == address) return &this->linkedRemotes[i];
  }
  return nullptr;
}
void SomfyShade::processFrame(somfy_frame_t &frame, bool internal) {
  // The reason why we are processing all frames here is so
  // any linked remotes that may happen to be on the same ESPSomfy RTS
  // device can trigger the appropriate actions.
  if(this->shadeId == 255) return; 
  SomfyRemote *remote = this->findRemote(frame.remoteAddress);
  if(!remote) return;
  if(remote != this && frame.cmd != somfy_commands::Sensor) remote->setRollingCode(frame.rollingCode);
  if(!internal && commandSupersedesPositioning(frame.cmd)) { this->userOverride(); this->prepareCommand(); }
  this->behaviour.onFrame(frame, internal, millis());
}
void SomfyShade::processWaitingFrame() { this->behaviour.onWaitingFrame(millis()); }
void SomfyShade::processInternalCommand(somfy_commands cmd, uint8_t repeat) { this->behaviour.onGroupCommand(cmd, repeat, millis()); }
void SomfyShade::checkMovement() { this->behaviour.tick(millis()); }
void SomfyShade::markTxPending(uint32_t ticket) { this->behaviour.onTxQueued(ticket, millis()); }
void SomfyShade::processTxEvent(const somfy_tx_event_t &evt) { this->behaviour.onTxEvent(evt); }
void SomfyShade::prepareCommand(uint32_t exceptTicket) {
  somfy.cancelGroupStartsForShade(this->getShadeId());
  somfy.transceiver.cancelTx(this->getRemoteAddress(), exceptTicket);
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
bool SomfyShade::sendCommand(somfy_commands cmd, uint8_t repeat, uint8_t stepSize) {
  if(commandSupersedesPositioning(cmd)) this->userOverride();
  if((cmd == somfy_commands::My || cmd == somfy_commands::Stop) &&
      (this->hasPendingGroupStart() || this->startQueuedOnly() || this->behaviour.ownsGroupPositioning()))
    return this->stopGroupTarget();
  if(commandSupersedesPositioning(cmd) && this->hasPendingGroupStart()) {
    // Coalesce a directional press with the unfinished start; it is applied
    // as an endpoint target once the start's outcome is known.
    if((cmd == somfy_commands::Up || cmd == somfy_commands::Down) && this->supportsGroupTarget())
      return this->behaviour.moveToGroupTarget(cmd == somfy_commands::Up ? 0 : 100, millis());
    return false;
  }
  // This sendCommand function will always be called externally. sendCommand at the remote level
  // is expected to be called internally when the motor needs commanded.
  if(this->bitLength == 0) this->bitLength = somfy.transceiver.config.type;
  this->prepareOnSend = commandSupersedesPositioning(cmd);
  const bool accepted = this->behaviour.sendCommand(cmd, repeat, stepSize, millis());
  this->prepareOnSend = false;
  return accepted;
}
bool SomfyShade::sendTiltCommand(somfy_commands cmd) {
  if(this->hasPendingGroupStart()) return false;
  this->userOverride();
  this->prepareOnSend = true;
  const bool accepted = this->behaviour.sendTiltCommand(cmd, millis());
  this->prepareOnSend = false;
  return accepted;
}
bool SomfyShade::moveToTarget(float pos, float tilt) {
  if(this->hasPendingGroupStart()) {
    // Keep the unfinished start; apply this member's latest target after it.
    if(tilt < 0 && this->supportsGroupTarget()) {
      this->userOverride();
      return this->behaviour.moveToGroupTarget(pos, millis());
    }
    return false;
  }
  this->userOverride();
  if(this->behaviour.ownsGroupPositioning() && tilt < 0) return this->moveToGroupTarget(pos);
  this->prepareOnSend = true;
  const bool accepted = this->behaviour.moveToTarget(pos, tilt, millis());
  this->prepareOnSend = false;
  return accepted;
}
bool SomfyShade::moveToGroupTarget(float pos) {
  // A press still queued has not moved the motor: cancel it so it is
  // replaced rather than stopped with My.
  this->cancelQueuedStart();
  this->prepareOnSend = true;
  const bool accepted = this->behaviour.moveToGroupTarget(pos, millis());
  this->prepareOnSend = false;
  return accepted;
}
bool SomfyShade::stopGroupTarget() {
  if(this->hasPendingGroupStart()) {
    // Drop this member's own start if it is still queued; a shared start
    // already on air is stopped once its outcome is known.
    somfy.transceiver.cancelTx(this->getRemoteAddress());
    return this->behaviour.stopGroupTarget(millis());
  }
  if(this->startQueuedOnly()) {
    this->prepareCommand();
    this->emitState();
    return true;
  }
  this->prepareOnSend = true;
  const bool accepted = this->behaviour.stopGroupTarget(millis());
  this->prepareOnSend = false;
  return accepted;
}
bool SomfyShade::moveToTiltTarget(float target) {
  if(this->hasPendingGroupStart()) return false;
  this->userOverride();
  this->prepareOnSend = true;
  const bool accepted = this->behaviour.moveToTiltTarget(target, millis());
  this->prepareOnSend = false;
  return accepted;
}
bool SomfyShade::setMyPosition(int8_t pos, int8_t tilt) {
  if(this->hasPendingGroupStart()) return false;
  this->userOverride();
  return this->behaviour.setMyPosition(pos, tilt, millis());
}
bool SomfyShade::moveToMyPosition() {
  if(this->hasPendingGroupStart()) return false;
  this->userOverride();
  this->prepareOnSend = true;
  const bool accepted = this->behaviour.moveToMyPosition(millis());
  this->prepareOnSend = false;
  return accepted;
}
void SomfyShade::setPosition(float pos, float tilt) { this->behaviour.setPosition(pos, tilt); }
send_result_t SomfyShade::send(somfy_commands cmd, uint8_t repeat, uint8_t stepSize, uint16_t delayMs, bool priority) {
  const send_result_t sent = SomfyRemote::sendCommandAfter(cmd, repeat, stepSize, delayMs, priority, true);
  // Only supersede the old target after admission. A refused replacement must
  // leave its queued stop and timing intact. Behaviour hears the new frame
  // after this returns, so clearing old timing cannot erase the new plan.
  if(sent.sent && this->prepareOnSend) {
    this->prepareOnSend = false;
    this->prepareCommand(sent.ticket);
  }
  return sent;
}
void SomfyShade::onSaveWanted() { somfy.isDirty = true; }
void SomfyShade::onSensorFlags() { somfy.updateGroupFlags(); }
bool SomfyShade::save() {
  Preferences pref;
  this->commit();
  this->publish();
  return true;
}
