#include <Preferences.h>
#include <nvs.h>
#include "esp_log.h"
#include "Somfy.h"
#include "RollingCode.h"
#include "CounterCommandGuard.h"

static const char *TAG = "Somfy";

extern SomfyShadeController somfy;

void SomfyRemote::setRemoteAddress(uint32_t address) { this->m_remoteAddress = address; this->m_rollingCodeSynced = false; snprintf(this->m_remotePrefId, sizeof(this->m_remotePrefId), "_%lu", (unsigned long)this->m_remoteAddress); }
uint32_t SomfyRemote::getRemoteAddress() { return this->m_remoteAddress; }
somfy_commands SomfyRemote::transformCommand(somfy_commands cmd) { return this->flipCommands ? flipCommand(cmd) : cmd; }
bool SomfyRemote::sendSensorCommand(int8_t isWindy, int8_t isSunny, uint8_t repeat) {
  if(!somfy.modelLoad.usable())return false;
  if(!counterCommandAllowed(this->getRemoteAddress(),somfy_commands::Sensor))return false;
  if(this->proto==radio_proto::GP_Remote || this->proto==radio_proto::GP_Relay) return false;
  uint8_t flags = (this->getFlags() >> 4) & 0x0F;
  if(isWindy > 0) flags |= 0x01;
  if(isSunny > 0) flags |= 0x02;
  if(isWindy == 0) flags &= ~0x01;
  if(isSunny == 0) flags &= ~0x02;

  // Now ship this off as an 80 bit command.  Built aside so lastFrame only
  // changes when the frame is queued.
  somfy_frame_t frame = this->lastFrame;
  frame.remoteAddress = this->getRemoteAddress();
  frame.repeats = repeat;
  frame.bitLength = this->bitLength;
  frame.rollingCode = (uint16_t)flags;
  frame.encKey = 160; // Sensor commands are always encryption code 160.
  frame.cmd = somfy_commands::Sensor;
  frame.processed = false;
  ESP_LOGD(TAG, "CMD:%s ADDR:%d RCODE:%d REPEAT:%d",
      translateSomfyCommand(frame.cmd).c_str(),
      frame.remoteAddress, frame.rollingCode, repeat);
  if(!somfy.sendFrame(frame, repeat)) return false;
  this->lastFrame = frame;
  somfy.processFrame(this->lastFrame, true);
  return true;
}
bool SomfyRemote::sendCommand(somfy_commands cmd) { return this->sendCommand(cmd, this->repeats); }
bool SomfyRemote::sendCommand(somfy_commands cmd, uint8_t repeat, uint8_t stepSize) { return this->sendCommandAfter(cmd, repeat, stepSize, 0).sent; }
send_result_t SomfyRemote::sendCommandAfter(somfy_commands cmd, uint8_t repeat, uint8_t stepSize, uint16_t delayMs, bool priority, bool hearsItself) {
  if(!somfy.modelLoad.usable())return {};
  if(!counterCommandAllowed(this->getRemoteAddress(),cmd))return {};
  if((this->proto==radio_proto::GP_Remote || this->proto==radio_proto::GP_Relay) &&
     !this->gpioCommandAllowed(this->transformCommand(cmd))) return {};
  // Build the frame aside and only take the rolling code once the frame is
  // queued so a dropped command leaves the remote untouched.
  somfy_frame_t frame = this->lastFrame;
  if(!this->peekNextRollingCode(frame.rollingCode)) return {};
  frame.remoteAddress = this->getRemoteAddress();
  frame.cmd = this->transformCommand(cmd);
  frame.repeats = repeat;
  frame.bitLength = this->bitLength;
  frame.stepSize = stepSize;
  frame.valid = true;
  // Match the encKey to the rolling code.  These keys range from 160 to 175.
  frame.encKey = 0xA0 | static_cast<uint8_t>(frame.rollingCode & 0x000F);
  frame.proto = this->proto;
  if(frame.bitLength == 0) frame.bitLength = radioBitLength;
  if(frame.rollingCode == 0) ESP_LOGE(TAG, "Setting rcode to 0");
  // We have to set the processed to clear this if we are sending
  // another command.
  frame.processed = false;
  send_result_t result;
  if(this->proto == radio_proto::GP_Relay) {
    ESP_LOGD(TAG, "CMD:%s ADDR:%d RCODE:%d SETTING GPIO",
        translateSomfyCommand(frame.cmd).c_str(),
        frame.remoteAddress, frame.rollingCode);
  }
  else if(this->proto == radio_proto::GP_Remote) {
    ESP_LOGD(TAG, "CMD:%s ADDR:%d RCODE:%d TRIGGER GPIO",
        translateSomfyCommand(frame.cmd).c_str(),
        frame.remoteAddress, frame.rollingCode);
    this->triggerGPIOs(frame);
  }
  else {
    ESP_LOGD(TAG, "CMD:%s ADDR:%d RCODE:%d REPEAT:%d",
        translateSomfyCommand(frame.cmd).c_str(),
        frame.remoteAddress, frame.rollingCode, repeat);
    // Only simulate the command if it is actually going on air.
    result.ticket = somfy.sendFrame(frame, repeat, delayMs, priority);
    if(result.ticket == 0) return result;
  }
  result.sent = true;
  this->commitRollingCode(frame.rollingCode);
  this->lastFrame = frame;
  // A shade that hears its own frame (ShadeBehaviour) is left out here.
  const SomfyRemote *skip = hearsItself ? this : nullptr;
  somfy.processFrame(this->lastFrame, true, skip);
  // Idle shades (and group members) this sets moving wait for the frame to go
  // on air.  Stops are sent ahead of everything else so they don't need to.
  if(result.ticket != 0 && !priority && startsMovement(frame.cmd)) somfy.markTxPending(frame.remoteAddress, result.ticket, skip);
  return result;
}
bool SomfyRemote::isLastCommand(somfy_commands cmd) {
  if(!this->lastFrame.valid || this->lastFrame.cmd != cmd || this->lastFrame.rollingCode != this->lastRollingCode) {
    ESP_LOGD(TAG, "Not the last command %d: %d - %d", static_cast<uint8_t>(this->lastFrame.cmd), this->lastFrame.rollingCode, this->lastRollingCode);
    return false;
  }
  return true;
}
bool SomfyRemote::repeatFrame(uint8_t repeat) {
  if(!somfy.modelLoad.usable())return false;
  if(!this->lastFrame.valid || !counterCommandAllowed(this->getRemoteAddress(),this->lastFrame.cmd) ||
     somfy.transceiver.counterBlocksIdentity(this->lastFrame.remoteAddress))return false;
  if((this->proto==radio_proto::GP_Remote || this->proto==radio_proto::GP_Relay) &&
     !this->gpioCommandAllowed(this->lastFrame.cmd)) return false;
  if(this->proto == radio_proto::GP_Relay)
    return true;
  else if(this->proto == radio_proto::GP_Remote) {
    this->triggerGPIOs(this->lastFrame);
    return true;
  }
  somfy_tx_job_t job;
  encodeRepeatJob(job, this->lastFrame, repeat);
  if(!somfy.transceiver.submitTx(job)) return false;
  noteRepeatQueued(this->lastFrame, job);
  //somfy.processFrame(this->lastFrame, true);
  return true;
}
// The next rolling code is consumed by commitRollingCode once the frame using
// it has been queued.  After that lastRollingCode holds what NVS does.
bool SomfyRemote::peekNextRollingCode(uint16_t &code) {
  uint16_t stored=0;
  if(!this->m_rollingCodeSynced) {
    nvs_handle_t handle=0;
    const auto opened=nvs_open("ShadeCodes",NVS_READONLY,&handle);
    if(opened!=ESP_OK && opened!=ESP_ERR_NVS_NOT_FOUND)return false;
    if(opened==ESP_OK) {
      const auto read=nvs_get_u16(handle,this->m_remotePrefId,&stored);
      nvs_close(handle);
      if(read!=ESP_OK && read!=ESP_ERR_NVS_NOT_FOUND)return false;
      if(read==ESP_ERR_NVS_NOT_FOUND)stored=0;
    }
  }
  // Exact absence starts at one; a verified 65535 intentionally wraps to zero.
  // No output or remote state changes when a storage read fails.
  code=nextRollingCode(this->m_rollingCodeSynced,this->lastRollingCode,[stored](){return stored;});
  return true;
}
void SomfyRemote::commitRollingCode(uint16_t code) {
  Preferences pref;
  if(!pref.begin("ShadeCodes")) {
    ESP_LOGE(TAG, "commitRollingCode() pref.begin(ShadeCodes) FAILED");
  }
  pref.putUShort(this->m_remotePrefId, code);
  pref.end();
  this->p_lastRollingCode(code);
  this->m_rollingCodeSynced = true;
}
uint16_t SomfyRemote::p_lastRollingCode(uint16_t code) { 
  uint16_t old = this->lastRollingCode;
  this->lastRollingCode = code; 
  return old;
}
uint16_t SomfyRemote::setRollingCode(uint16_t code) {
  if(somfy.transceiver.counterBlocksIdentity(this->getRemoteAddress()))return this->lastRollingCode;
  Preferences pref;
  if(this->lastRollingCode != code) {
    pref.begin("ShadeCodes");
    pref.putUShort(this->m_remotePrefId, code);
    pref.end();  
    this->lastRollingCode = code;
    this->m_rollingCodeSynced = true;
    ESP_LOGD(TAG, "Setting Last Rolling code %d", this->lastRollingCode);
  }
  return code;
}
SomfyLinkedRemote::SomfyLinkedRemote() {}
