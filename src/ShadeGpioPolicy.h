#pragma once
#include "SomfyFrame.h"
namespace ShadeGpioPolicy {
enum Pad : uint8_t { up = 1, down = 2, my = 4 };
// Physical setup/deassertion touches these pads independently of which logical
// command/category uses an asserted pulse. Keep logical sequencing separate.
inline uint8_t physicalMask(radio_proto protocol) {
  return protocol == radio_proto::GP_Remote  ? up | down | my
         : protocol == radio_proto::GP_Relay ? up | down
                                             : 0;
}
inline bool duplicate(radio_proto protocol, uint8_t upPin, uint8_t downPin,
                      uint8_t myPin) {
  const uint8_t mask = physicalMask(protocol), pins[] = {upPin, downPin, myPin};
  for (uint8_t i = 0; i < 3; i++)
    if ((mask & (1u << i)) && pins[i] != 255)
      for (uint8_t j = 0; j < i; j++)
        if ((mask & (1u << j)) && pins[j] == pins[i])
          return true;
  return false;
}
} // namespace ShadeGpioPolicy
