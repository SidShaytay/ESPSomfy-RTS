#include "SomfyShadeOps.h"
#include "Somfy.h"
#include "CounterResults.h"
#include "CounterRuntime.h"
#include "ShadeChanges.h"

extern SomfyShadeController somfy;

// ShadeRequests only calls these for a shade or group it has just found.
bool SomfyShadeOps::findShade(uint8_t shadeId, shade_info_t &info) {
  if(!somfy.modelLoad.usable())return false;
  SomfyShade *shade = somfy.getShadeById(shadeId);
  if(!shade) return false;
  info.flipPosition = shade->flipPosition;
  info.shadeType = shade->shadeType;
  info.tiltType = shade->tiltType;
  info.repeats = shade->repeats;
  info.bitLength = shade->bitLength;
  info.currentPos = shade->state().currentPos;
  info.myPos = shade->state().myPos;
  info.myTiltPos = shade->state().myTiltPos;
  info.idle = shade->isIdle();
  return true;
}
bool SomfyShadeOps::findGroup(uint8_t groupId, uint8_t &repeats) {
  if(!somfy.modelLoad.usable())return false;
  SomfyGroup *group = somfy.getGroupById(groupId);
  if(!group) return false;
  repeats = group->repeats;
  return true;
}
bool SomfyShadeOps::moveToTarget(uint8_t shadeId, float pos) { return somfy.getShadeById(shadeId)->moveToTarget(pos); }
bool SomfyShadeOps::moveToTiltTarget(uint8_t shadeId, float tilt) { return somfy.getShadeById(shadeId)->moveToTiltTarget(tilt); }
bool SomfyShadeOps::sendCommand(uint8_t shadeId, somfy_commands cmd, uint8_t repeat, uint8_t stepSize) {
  return somfy.getShadeById(shadeId)->sendCommand(cmd, repeat, stepSize);
}
bool SomfyShadeOps::sendTiltCommand(uint8_t shadeId, somfy_commands cmd) { return somfy.getShadeById(shadeId)->sendTiltCommand(cmd); }
bool SomfyShadeOps::isLastCommand(uint8_t shadeId, somfy_commands cmd) { return somfy.getShadeById(shadeId)->isLastCommand(cmd); }
bool SomfyShadeOps::repeatFrame(uint8_t shadeId, uint8_t repeat) { return somfy.getShadeById(shadeId)->repeatFrame(repeat); }
bool SomfyShadeOps::sendSensorCommand(uint8_t shadeId, int8_t windy, int8_t sunny, uint8_t repeat) {
  SomfyShade *shade = somfy.getShadeById(shadeId);
  const bool sent = shade->sendSensorCommand(windy, sunny, repeat);
  if(sent)shade->emitState();
  return sent;
}
bool SomfyShadeOps::setMyPosition(uint8_t shadeId, int8_t pos, int8_t tilt) {
  const bool accepted = somfy.getShadeById(shadeId)->setMyPosition(pos, tilt);
  if(accepted) {
    ++somfy.getShadeById(shadeId)->configurationRevision;
    ++somfy.configurationRevision;
  }
  return accepted;
}
bool SomfyShadeOps::programMyPositionHere(uint8_t shadeId, bool clear) {
  auto *shade = somfy.getShadeById(shadeId);
  const bool accepted = shade && shade->programMyPositionHere(clear);
  if(accepted) { ++shade->configurationRevision; ++somfy.configurationRevision; }
  return accepted;
}
void SomfyShadeOps::setPositions(uint8_t shadeId, float pos, float tilt) {
  SomfyShade *shade = somfy.getShadeById(shadeId);
  ++somfy.configurationRevision;
  ++shade->configurationRevision;
  shade->setPosition(pos, tilt);
  shade->emitState();
}
void SomfyShadeOps::setUnpaired(uint8_t shadeId) {
  ++somfy.configurationRevision;
  SomfyShade *shade = somfy.getShadeById(shadeId);
  shade->paired = false;
  shade->save();
  shade->configChanged();
}
shade_request_result_t SomfyShadeOps::setRollingCode(uint8_t shadeId, uint16_t code) {
  return counterRequestResult(applyPrimaryCounterCorrection(shadeId,code));
}

bool SomfyShadeOps::sendGroupCommand(uint8_t groupId, somfy_commands cmd, uint8_t repeat, uint8_t stepSize) {
  return somfy.getGroupById(groupId)->sendCommand(cmd, repeat, stepSize);
}
bool SomfyShadeOps::isLastGroupCommand(uint8_t groupId, somfy_commands cmd) { return somfy.getGroupById(groupId)->isLastCommand(cmd); }
bool SomfyShadeOps::repeatGroupFrame(uint8_t groupId, uint8_t repeat) { return somfy.getGroupById(groupId)->repeatFrame(repeat); }
bool SomfyShadeOps::sendGroupSensorCommand(uint8_t groupId, int8_t windy, int8_t sunny, uint8_t repeat) {
  SomfyGroup *group = somfy.getGroupById(groupId);
  const bool sent = group->sendSensorCommand(windy, sunny, repeat);
  if(sent)group->emitState();
  return sent;
}

uint32_t SomfyShadeOps::sendOwnedRepeat(const shade_request_t &req) {
  if(!somfy.modelLoad.usable() || !validOwnedRepeatRequest(req)) return 0;
  const bool grouped=req.type==shade_request_type_t::group_repeat || req.type==shade_request_type_t::group_command;
  if(!grouped && req.type!=shade_request_type_t::shade_repeat && req.type!=shade_request_type_t::shade_command) return 0;
  auto *shade=grouped?nullptr:somfy.getShadeById(req.id);
  auto *group=grouped?somfy.getGroupById(req.id):nullptr;
  SomfyRemote *remote=grouped?static_cast<SomfyRemote *>(group):static_cast<SomfyRemote *>(shade);
  if(!remote || (remote->proto!=radio_proto::RTS && remote->proto!=radio_proto::RTW && remote->proto!=radio_proto::RTV)) return 0;
  const auto bits=remote->bitLength?remote->bitLength:somfy.transceiver.config.type;
  if(bits!=56 && bits!=80) return 0;
  if((req.command==somfy_commands::Toggle || req.command==somfy_commands::Favorite) && bits!=80) return 0;
  if(shade && shade->tiltType==tilt_types::euromode && (req.command==somfy_commands::Up || req.command==somfy_commands::Down)) return 0;
  const bool step=req.command==somfy_commands::StepUp || req.command==somfy_commands::StepDown;
  if(step ? req.stepSize<1 || req.stepSize>127 : req.stepSize!=0) return 0;
  const auto wire=remote->transformCommand(req.command);
  if((req.type==shade_request_type_t::shade_repeat || req.type==shade_request_type_t::group_repeat) &&
     remote->isLastCommand(wire) && remote->lastFrame.stepSize==req.stepSize &&
     remote->lastFrame.remoteAddress==remote->getRemoteAddress() &&
     remote->lastFrame.proto==remote->proto && remote->lastFrame.bitLength==bits)
    return remote->repeatFrameReceipt(req.repeat).ticket;
  return grouped?group->sendCommandReceipt(req.command,req.repeat,req.stepSize).ticket:
                 shade->sendCommandReceipt(req.command,req.repeat,req.stepSize).ticket;
}
