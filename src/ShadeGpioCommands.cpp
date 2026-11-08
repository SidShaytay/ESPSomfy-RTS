#include "Somfy.h"
extern SomfyShadeController somfy;
#include "ShadeGpioCommands.h"

bool SomfyShade::gpioCommandAllowed(somfy_commands wire) {
  if(!ShadeGpioPolicy::physicalMask(proto))return true;
  if(validateGPIOs(proto,gpioUp,gpioDown,gpioMy)!=0)return false;
  // Relay outputs follow logical model direction, whereas the remote pulses
  // the transformed wire command. Reverse the wire transform for relay only.
  if(proto==radio_proto::GP_Relay)wire=transformCommand(wire);
  return ShadeGpioPolicy::assigned(ShadeGpioPolicy::commandMask(proto,shadeType,wire),gpioUp,gpioDown,gpioMy);
}
bool SomfyShade::canGpioCommand(somfy_commands cmd) {
  if(!ShadeGpioPolicy::physicalMask(proto))return true;
  if(shadeType==shade_types::drycontact2 && cmd==somfy_commands::My)return false;
  const bool single=isToggle()||shadeType==shade_types::drycontact;
  if(cmd==somfy_commands::Prog && isToggle())cmd=somfy_commands::Toggle;
  if(cmd==somfy_commands::Toggle && (bitLength?bitLength:somfy.transceiver.config.type)!=80)cmd=somfy_commands::My;
  if(proto==radio_proto::GP_Remote && single && cmd==somfy_commands::My)cmd=somfy_commands::Toggle;
  return gpioCommandAllowed(transformCommand(cmd));
}
bool SomfyShade::canGpioTilt(somfy_commands cmd) {
  if(!ShadeGpioPolicy::physicalMask(proto))return true;
  if(tiltType==tilt_types::none || isToggle() || shadeType==shade_types::drycontact || shadeType==shade_types::drycontact2)return false;
  if(proto==radio_proto::GP_Relay && (tiltType==tilt_types::tiltmotor||tiltType==tilt_types::euromode))return false;
  if(cmd!=somfy_commands::Up && cmd!=somfy_commands::Down && cmd!=somfy_commands::My)return false;
  return gpioCommandAllowed(transformCommand(cmd));
}
bool SomfyShade::canGpioSeek(bool tilt) {
  if(!ShadeGpioPolicy::physicalMask(proto))return true;
  if(isToggle()||shadeType==shade_types::drycontact||shadeType==shade_types::drycontact2)return false;
  // A seek may reverse and sends an automatic stop later. Admit the complete
  // path before Behaviour changes any target (including tilt-only lift state).
  return tilt ? canGpioTilt(somfy_commands::Up)&&canGpioTilt(somfy_commands::Down)&&canGpioTilt(somfy_commands::My)
              : gpioCommandAllowed(transformCommand(somfy_commands::Up))&&
                gpioCommandAllowed(transformCommand(somfy_commands::Down))&&
                gpioCommandAllowed(transformCommand(somfy_commands::My));
}
