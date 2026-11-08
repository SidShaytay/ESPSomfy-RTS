#include "Somfy.h"
extern SomfyShadeController somfy;

bool SomfyShade::sendCommand(somfy_commands cmd) { return this->sendCommand(cmd, this->repeats); }
bool SomfyShade::sendCommand(somfy_commands cmd, uint8_t repeat, uint8_t stepSize) {
  if(!this->canGpioCommand(cmd))return false;
  if(cmd==somfy_commands::My && !this->isToggle() && this->shadeType!=shade_types::drycontact && (this->simMy || this->proto==radio_proto::GP_Relay) && this->isIdle() && !this->canGpioSeek(this->tiltType!=tilt_types::none))return false;
  // This sendCommand function will always be called externally. sendCommand at the remote level
  // is expected to be called internally when the motor needs commanded.
  if(this->bitLength == 0) this->bitLength = somfy.transceiver.config.type;
  return this->behaviour.sendCommand(cmd, repeat, stepSize, millis());
}
bool SomfyShade::sendTiltCommand(somfy_commands cmd) { if(!this->canGpioTilt(cmd))return false; return this->behaviour.sendTiltCommand(cmd, millis()); }
bool SomfyShade::moveToTarget(float pos, float tilt) { if(!this->canGpioSeek(tilt>=0 || this->tiltType==tilt_types::tiltonly))return false; return this->behaviour.moveToTarget(pos, tilt, millis()); }
bool SomfyShade::moveToTiltTarget(float target) { if(!this->canGpioSeek(true))return false; return this->behaviour.moveToTiltTarget(target, millis()); }
bool SomfyShade::setMyPosition(int8_t pos, int8_t tilt) { if(!this->canGpioSeek(this->tiltType!=tilt_types::none))return false; return this->behaviour.setMyPosition(pos, tilt, millis()); }
bool SomfyShade::moveToMyPosition() { if(!this->canGpioCommand(somfy_commands::My) || ((this->simMy || this->proto==radio_proto::GP_Relay) && !this->canGpioSeek(this->tiltType!=tilt_types::none)))return false; return this->behaviour.moveToMyPosition(millis()); }
