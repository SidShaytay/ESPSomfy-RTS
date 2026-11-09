#include "LocalReceive.h"
#include "PulseDecoder.h"
#include "SomfyFrame.h"
#include <stdint.h>
namespace LocalUi {
namespace {
uint8_t channelIndex(ReceiveChannel channel) {
  switch (channel) {
  case ReceiveChannel::learning:
    return 0;
  case ReceiveChannel::diagnostics:
    return 1;
  }
  return 255;
}
} // namespace
bool ReceiveHub::subscribe(ReceiveChannel channel, ReceiveSink &sink,
                           ReceiveSubscription &out) {
  const auto index = channelIndex(channel);
  if (index >= 2)
    return false;
  auto &slot = slots[index];
  if (slot.sink) {
    if (slot.sink != &sink)
      return false;
    out = {slot.generation, channel};
    return true;
  }
  if (registrations == UINT64_MAX)
    return false;
  slot = {&sink, ++registrations};
  out = {slot.generation, channel};
  return true;
}
bool ReceiveHub::observing(ReceiveSubscription token,
                           const ReceiveSink &sink) const {
  const auto index = channelIndex(token.channel);
  return index < 2 && token.generation && sequence != UINT64_MAX &&
         slots[index].sink == &sink &&
         slots[index].generation == token.generation;
}
bool ReceiveHub::unsubscribe(ReceiveSubscription token, ReceiveSink &sink) {
  const auto index = channelIndex(token.channel);
  if (index >= 2 || !token.generation)
    return false;
  auto &slot = slots[index];
  if (slot.sink != &sink || slot.generation != token.generation)
    return false;
  slot = {};
  return true;
}
bool ReceiveHub::publish(ReceiveObservation observation, ReceivePulses pulses) {
  if (delivering || !active() || sequence == UINT64_MAX)
    return false;
  observation.sequence = ++sequence;
  if (!pulses.values && pulses.count) {
    pulses.count = 0;
    pulses.clipped = true;
  }
  if (pulses.count > MAX_TIMINGS) {
    pulses.count = MAX_TIMINGS;
    pulses.clipped = true;
  }
  const uint64_t current[] = {slots[0].generation, slots[1].generation};
  delivering = true;
  for (uint8_t i = 0; i < 2; i++) {
    // Re-read the slot: an earlier callback may have detached/destroyed it.
    // A replacement at the same address still needs a fresh registration.
    if (slots[i].sink && slots[i].generation == current[i])
      slots[i].sink->received(observation, pulses);
  }
  delivering = false;
  return true;
}
ReceiveHub &receiveHub() {
  static ReceiveHub hub;
  return hub;
}
bool publishDecodedReceive(ReceiveHub &hub, const somfy_frame_t &frame,
                           const somfy_rx_t &raw, uint64_t at, bool scanning) {
  if (!hub.active())
    return false;
  ReceiveObservation observation;
  observation.observedAt = at;
  observation.address = frame.remoteAddress;
  observation.rollingCode = frame.rollingCode;
  observation.rssi =
      static_cast<int16_t>(frame.rssi < INT16_MIN   ? INT16_MIN
                           : frame.rssi > INT16_MAX ? INT16_MAX
                                                    : frame.rssi);
  observation.bits = raw.bit_length;
  observation.protocol = static_cast<uint8_t>(frame.proto);
  observation.command = static_cast<uint8_t>(frame.cmd);
  observation.rawCommand = frame.rawCmd;
  observation.encodingKey = frame.encKey;
  observation.hardwareSync = frame.hwsync;
  observation.flags = (frame.valid ? ReceiveObservation::validFlag : 0) |
                      (scanning ? ReceiveObservation::scanningFlag : 0);
  observation.stepSize = frame.cmd == somfy_commands::StepUp ||
                                 frame.cmd == somfy_commands::StepDown
                             ? frame.stepSize
                             : 0;
  return hub.publish(observation, {raw.pulses, raw.pulseCount, false});
}
} // namespace LocalUi
