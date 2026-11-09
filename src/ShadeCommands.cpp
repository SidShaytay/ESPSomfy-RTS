#include "Somfy.h"
#include "CounterCommandGuard.h"
extern SomfyShadeController somfy;

bool SomfyShade::sendCommand(somfy_commands cmd) { return this->sendCommand(cmd, this->repeats); }
bool SomfyShade::sendCommand(somfy_commands cmd, uint8_t repeat, uint8_t stepSize) {
  if(!counterCommandAllowed(this->getRemoteAddress(),cmd))return false;
  if(!this->canGpioCommand(cmd))return false;
  if(cmd==somfy_commands::My && !this->isToggle() && this->shadeType!=shade_types::drycontact && (this->simMy || this->proto==radio_proto::GP_Relay) && this->isIdle() && !this->canGpioSeek(this->tiltType!=tilt_types::none))return false;
  // This sendCommand function will always be called externally. sendCommand at the remote level
  // is expected to be called internally when the motor needs commanded.
  if(this->bitLength == 0) this->bitLength = somfy.transceiver.config.type;
  return this->behaviour.sendCommand(cmd, repeat, stepSize, millis());
}
bool SomfyShade::sendTiltCommand(somfy_commands cmd) { if(!counterCommandAllowed(this->getRemoteAddress(),cmd))return false; if(!this->canGpioTilt(cmd))return false; return this->behaviour.sendTiltCommand(cmd, millis()); }
bool SomfyShade::moveToTarget(float pos, float tilt) { if(!counterCommandAllowed(this->getRemoteAddress(),somfy_commands::UpDown))return false; if(!this->canGpioSeek(tilt>=0 || this->tiltType==tilt_types::tiltonly))return false; return this->behaviour.moveToTarget(pos, tilt, millis()); }
bool SomfyShade::moveToTiltTarget(float target) { if(!counterCommandAllowed(this->getRemoteAddress(),somfy_commands::UpDown))return false; if(!this->canGpioSeek(true))return false; return this->behaviour.moveToTiltTarget(target, millis()); }
bool SomfyShade::setMyPosition(int8_t pos, int8_t tilt) { if(!counterCommandAllowed(this->getRemoteAddress(),somfy_commands::UpDown))return false; if(!this->canGpioSeek(this->tiltType!=tilt_types::none))return false; return this->behaviour.setMyPosition(pos, tilt, millis()); }
bool SomfyShade::moveToMyPosition() { if(!counterCommandAllowed(this->getRemoteAddress(),somfy_commands::My))return false; if(!this->canGpioCommand(somfy_commands::My) || ((this->simMy || this->proto==radio_proto::GP_Relay) && !this->canGpioSeek(this->tiltType!=tilt_types::none)))return false; return this->behaviour.moveToMyPosition(millis()); }
