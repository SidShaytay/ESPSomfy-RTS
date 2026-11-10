#pragma once
#include <stdint.h>
#include "ShadeRequests.h"

namespace LocalUi {
enum class ControlAction : uint8_t {
  up = 0, stop = 1, down = 2, position, tiltUp, tiltStop, tiltDown,
  tiltPosition, my, favourite, toggle, stepUp, stepDown, sunOn, sunOff, setFavouriteHere, clearFavouriteHere, count
};
inline bool favouriteProgramming(ControlAction action) { return action==ControlAction::setFavouriteHere || action==ControlAction::clearFavouriteHere; }
using ControlMask = uint32_t;
constexpr ControlMask controlActionBit(ControlAction action) {
  return uint8_t(action) < uint8_t(ControlAction::count) ? ControlMask(1) << uint8_t(action) : 0;
}
constexpr ControlMask allControlActions = (ControlMask(1) << uint8_t(ControlAction::count)) - 1;
struct ControlDescriptor {
  shade_types shadeType = shade_types::roller;
  tilt_types tiltType = tilt_types::none;
  radio_proto proto = radio_proto::RTS;
  uint8_t bitLength = 56; // Effective RF bits after resolving the inherited radio setting.
  bool paired = false, sunSensor = false, group = false;
  bool moving = false, tiltMoving = false;
  uint8_t memberCount = 0; // Groups only: 1..32, zero for incomplete/incompatible membership.
  // For a nonempty group, intersect controlCapabilities(member) for every fresh
  // RF-compatible member. Incomplete membership must set memberCount to zero.
  // Sun flags use the group's any-sensor aggregate separately. Never OR this mask.
  ControlMask commonActions = 0;
};
enum class ControlResult : uint8_t { unsupported, invalid, idle, done };
ControlMask controlCapabilities(const ControlDescriptor &);
bool allows(ControlAction, const ControlDescriptor &);
// Pure construction only: no model lookup, mutation, RF/GPIO, queue or storage.
// value is -1 except for target actions (0..100); step is zero except for
// step actions (1..127). On failure out stays unchanged. Only done authorizes using the constructed
// request; the caller still revalidates identity, authorization and admission.
ControlResult prepareControlRequest(ControlAction, const ControlDescriptor &, int32_t stableId,
                                    int32_t value, int32_t step, shade_request_t &out);
}
