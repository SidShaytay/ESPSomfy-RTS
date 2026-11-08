#ifndef SHADE_GPIO_CONFIG_HOST_TEST
#include "Somfy.h"
#include <driver/gpio.h>
#endif
#include "ModelEditor.h"
#include "ShadeGpioPolicy.h"
// Read-only projected admission shared by the domain edit and local preflight.
// It never changes the live object, reserves a model slot or calls GPIO/NVS.
int8_t SomfyShade::validateJSON(JsonObject &obj) {
  if (!obj["proto"].isNull() && !obj["proto"].is<uint8_t>())
    return PIN_INVALID_OUTPUT;
  const auto protocol =
      !obj["proto"].isNull()
          ? static_cast<radio_proto>(obj["proto"].as<uint8_t>())
          : this->proto;
  const uint8_t mask = ShadeGpioPolicy::physicalMask(protocol);
  const char *keys[] = {"gpioUp", "gpioDown", "gpioMy"};
  for (uint8_t i = 0; i < 3; i++)
    if ((mask & (1u << i)) && !obj[keys[i]].isNull() &&
        !obj[keys[i]].is<uint8_t>())
      return PIN_INVALID_OUTPUT;
  const auto upPin =
      !obj["gpioUp"].isNull() ? obj["gpioUp"].as<uint8_t>() : this->gpioUp;
  const auto downPin = !obj["gpioDown"].isNull() ? obj["gpioDown"].as<uint8_t>()
                                                 : this->gpioDown;
  const auto myPin =
      !obj["gpioMy"].isNull() ? obj["gpioMy"].as<uint8_t>() : this->gpioMy;
  return this->validateGPIOs(protocol, upPin, downPin, myPin);
}
