#include <Preferences.h>
#include "esp_log.h"
#include "Somfy.h"
#include "MotionDiagnosticsService.h"
#include "GroupMotion.h"

static const char *TAG = "Somfy";

extern SomfyShadeController somfy;

void SomfyShade::onChanged(uint16_t bits) {
  this->changed |= bits;
  if(bits & (SHADE_CHANGED_DIRECTION | SHADE_CHANGED_POSITION | SHADE_CHANGED_STATE))
    MotionTrace::record(1,uint8_t(state().direction+1),getShadeId(),0,
      int32_t(state().currentPos*100),millis());
  if(bits & (SHADE_CHANGED_MY_POS | SHADE_CHANGED_MY_TILT_POS)) {
    ++configurationRevision;
    ++somfy.configurationRevision;
  }
}

void SomfyShade::clear() {
  ++configurationRevision;
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
  this->calibration = {};
  this->tiltTime = 7000;
  this->stepSize = 100;
  this->repeats = 1;
  this->sortOrder = 255;
}
bool SomfyShade::unlinkRemote(uint32_t address) {
  Preferences pref;
  for(uint8_t i = 0; i < SOMFY_MAX_LINKED_REMOTES; i++) {
    if(this->linkedRemotes[i].getRemoteAddress() == address) {
      this->linkedRemotes[i].setRemoteAddress(0);
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
void SomfyShade::processInternalCommand(somfy_commands cmd, uint8_t repeat, uint8_t stepSize) { this->behaviour.onGroupCommand(cmd, repeat, millis(), stepSize); }
void SomfyShade::checkMovement() { this->behaviour.tick(millis()); }
void SomfyShade::markTxPending(uint32_t ticket) { this->behaviour.onTxQueued(ticket, millis()); }
void SomfyShade::processTxEvent(const somfy_tx_event_t &evt) { this->behaviour.onTxEvent(evt); }
void SomfyShade::setPosition(float pos, float tilt) { this->behaviour.setPosition(pos, tilt); }
void SomfyShade::onSaveWanted() { somfy.isDirty = true; }
void SomfyShade::onSensorFlags() { somfy.updateGroupFlags(); }
