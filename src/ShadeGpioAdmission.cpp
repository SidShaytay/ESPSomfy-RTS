#ifndef SHADE_GPIO_ADMISSION_HOST_TEST
#include "ConfigSettings.h"
#include "Somfy.h"
#include "ESPNetwork.h"
#include <driver/gpio.h>
#endif
#include "ModelEditor.h"
#include "ShadeGpioPolicy.h"
extern SomfyShadeController somfy;
extern ConfigSettings settings;
#if HAS_ETHERNET
extern ESPNetwork net;
#endif
int8_t SomfyShade::validateGPIOs(radio_proto protocol, uint8_t upPin,
                                 uint8_t downPin, uint8_t myPin) {
  if(ShadeGpioPolicy::pinClaims(protocol,upPin,downPin,myPin) &&
     (!somfy.transceiver.config.loadStatus.usable()
#if HAS_ETHERNET
      || !settings.loadStatus.usable() || !settings.Ethernet.loadStatus.usable()
#endif
     ))return PIN_STORAGE_UNAVAILABLE;
  pin_users_t radio, ethernet, others, invalid;
  radio.uses = [](uint8_t pin, void *) {
    return somfy.transceiver.usesPin(pin);
  };
  invalid.uses = [](uint8_t pin, void *) {
    return !GPIO_IS_VALID_OUTPUT_GPIO(pin);
  };
#if HAS_ETHERNET
  ethernet.uses = [](uint8_t pin,void *) {
    return LivePinClaims::contains(net.retainedPinClaims(),pin) ||
      ((settings.connType==conn_types_t::ethernet || settings.connType==conn_types_t::ethernetpref) && settings.Ethernet.usesPin(pin));
  };
#endif
  others.ctx = this;
  others.uses = [](uint8_t pin, void *context) {
    const auto *self = static_cast<SomfyShade *>(context);
    for (auto &shade : somfy.shades) {
      if (&shade == self || shade.getShadeId() == 255)
        continue;
      if (shade.usesPin(pin))
        return true;
    }
    return false;
  };
  return gpioPinConflict(protocol, upPin, downPin, myPin, radio, ethernet,
                         others, invalid);
}
