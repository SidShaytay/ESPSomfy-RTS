#include "Somfy.h"

send_result_t SomfyShade::send(somfy_commands cmd, uint8_t repeat, uint8_t stepSize, uint16_t delayMs, bool priority) {
  // The shared behaviour encodes a 56-bit one-button Toggle as legacy MY.
  // GPIO remotes have no RF encoding: restore the existing Toggle handler,
  // which pulses their configured gpioDown output, before frame dispatch.
  if(this->proto == radio_proto::GP_Remote && cmd == somfy_commands::My &&
      (isToggleShade(this->shadeType) || this->shadeType == shade_types::drycontact))
    cmd = somfy_commands::Toggle;
  return SomfyRemote::sendCommandAfter(cmd, repeat, stepSize, delayMs, priority, true);
}
