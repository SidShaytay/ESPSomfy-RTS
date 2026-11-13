#include "esp_log.h"
#include "Somfy.h"
#include "ModelEditor.h"
#include "CounterCommandGuard.h"
#include "GroupMotion.h"
#include "BitLength.h"


static const char *TAG = "Somfy";

extern SomfyShadeController somfy;
bool stopTargetMembers(SomfyGroup &group);

void SomfyGroup::clear() {
  ++configurationRevision;
  this->targetActive = false;
  this->replanPending = false;

  this->setGroupId(255);
  this->setRemoteAddress(0);
  this->repeats = 0;
  this->roomId = 0;
  this->name[0] = 0x00;
  memset(&this->linkedShades, 0x00, sizeof(this->linkedShades));
}
// False when the group is full.  The caller saves the change.
bool SomfyGroup::linkShade(uint8_t shadeId) { return addToList(this->linkedShades, SOMFY_MAX_GROUPED_SHADES, shadeId); }
// False when the shade wasn't in the group.  The caller saves the change.
bool SomfyGroup::unlinkShade(uint8_t shadeId) { return removeFromList(this->linkedShades, SOMFY_MAX_GROUPED_SHADES, shadeId); }
// Linked shade ids are kept packed so a search can stop at the first 0.
void SomfyGroup::compressLinkedShadeIds() { packList(this->linkedShades, SOMFY_MAX_GROUPED_SHADES); }
bool SomfyGroup::hasShadeId(uint8_t shadeId) {
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPED_SHADES; i++) {
    if(this->linkedShades[i] == 0) break;
    if(this->linkedShades[i] == shadeId) return true;
  }
  return false;
}
void SomfyGroup::updateFlags() { 
  uint8_t oldFlags = this->flags;
  this->flags = 0;
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPED_SHADES; i++) {
    if(this->linkedShades[i] != 0) {
      SomfyShade *shade = somfy.getShadeById(this->linkedShades[i]);
      if(shade) this->flags |= shade->getFlags();
    }
    else break;
  }
  if(oldFlags != this->flags) this->emitState();
}
int8_t SomfyGroup::p_direction(int8_t dir) {
  int8_t old = this->direction;
  if(old != dir) {
    this->direction = dir;
    this->changed |= GROUP_CHANGED_DIRECTION;
  }
  return old;
}
bool SomfyGroup::sendCommand(somfy_commands cmd) { return this->sendCommand(cmd, this->repeats); }
bool SomfyGroup::sendCommand(somfy_commands cmd, uint8_t repeat, uint8_t stepSize) { return this->sendCommandReceipt(cmd,repeat,stepSize).sent; }
send_result_t SomfyGroup::sendCommandReceipt(somfy_commands cmd, uint8_t repeat, uint8_t stepSize) {
  if(!somfy.modelLoad.usable())return {};
  if(!counterCommandAllowed(this->getRemoteAddress(),cmd))return {};
  ESP_LOGI(TAG, "[Group %u] sendCommand cmd=%s repeat=%u", this->getGroupId(), translateSomfyCommand(cmd).c_str(), repeat);
  // This sendCommand function will always be called externally. sendCommand at the remote level
  // is expected to be called internally when the motor needs commanded.
  const bool stop = cmd == somfy_commands::My || cmd == somfy_commands::Stop;
  bool programming = false;
  bool unfinished = false; // a member start is queued or not yet fully on air
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPED_SHADES && this->linkedShades[i]; i++) {
    SomfyShade *shade = somfy.getShadeById(this->linkedShades[i]);
    if(shade && shade->hasPendingProgramming()) programming = true;
    if(shade && (shade->hasPendingGroupStart() || shade->startQueuedOnly())) unfinished = true;
  }
  if(programming && !stop && commandSupersedesPositioning(cmd)) return {};
  if(stop) this->cancelReplan();
  if(this->bitLength == 0) this->bitLength = somfy.transceiver.config.type;
  if(stop && (this->targetActive || unfinished || programming)) {
    // A shared My would recall the favorite on a member which has already
    // stopped or never started. Drop a shared start still queued, cancel
    // queued member starts, and stop only members actually moving, after an
    // unfinished start's outcome is known.
    somfy.transceiver.cancelQueuedForRemote(this->getRemoteAddress());
    const bool accepted = stopTargetMembers(*this);
    this->updateTargetDirection();
    return {accepted,0};
  }
  // Up/Down replace an unfinished start; other commands wait for its outcome.
  if(unfinished && commandSupersedesPositioning(cmd) && cmd != somfy_commands::Up && cmd != somfy_commands::Down) return {};
  const send_result_t result = this->sendCommandAfter(cmd, repeat, stepSize, 0);
  if(!result.sent) return {};
  if(commandSupersedesPositioning(cmd)) {
    this->cancelReplan();
    somfy.transceiver.cancelQueuedForRemote(this->getRemoteAddress(), result.ticket);
    for(uint8_t i = 0; i < SOMFY_MAX_GROUPED_SHADES && this->linkedShades[i]; i++) {
      SomfyShade *shade = somfy.getShadeById(this->linkedShades[i]);
      if(shade) {
        somfy.cancelGroupStartsForShade(shade->getShadeId(), result.ticket);
        somfy.transceiver.cancelQueuedForRemote(shade->getRemoteAddress());
        shade->cancelPositioning();
      }
    }
    this->targetActive = false;
  }


  switch(cmd) {
    case somfy_commands::My:
      this->p_direction(0);
      break;
    case somfy_commands::Up:
      this->p_direction(-1);
      break;
    case somfy_commands::Down:
      this->p_direction(1);
      break;
    default:
      break;
  }
  
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPED_SHADES; i++) {
    if(this->linkedShades[i] != 0) {
      SomfyShade *shade = somfy.getShadeById(this->linkedShades[i]);
      if(shade) {
        ESP_LOGI(TAG, "[Group %u] processInternalCommand on shade %u cmd=%s", this->getGroupId(), shade->getShadeId(), translateSomfyCommand(cmd).c_str());
        shade->processInternalCommand(shade->transformCommand(this->transformCommand(cmd)), repeat, stepSize);
        if(result.ticket && startsMovement(this->transformCommand(cmd))) {
          shade->markTxPending(result.ticket);
          if((cmd == somfy_commands::Up || cmd == somfy_commands::Down) && shade->supportsGroupTarget())
            shade->observeGroupStart(result.ticket);
        }

        shade->emitCommand(cmd, "group", this->getRemoteAddress());
      }
    }
  }
  this->updateFlags();
  this->emitState();
  return result;
}
