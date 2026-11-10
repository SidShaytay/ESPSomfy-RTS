#include "LocalControlPolicy.h"
#include "BitLength.h"

namespace LocalUi {
namespace {
bool radio(radio_proto proto) {
  return proto == radio_proto::RTS || proto == radio_proto::RTW || proto == radio_proto::RTV;
}
bool gpio(radio_proto proto) { return proto == radio_proto::GP_Relay || proto == radio_proto::GP_Remote; }
constexpr ControlMask groupActions = controlActionBit(ControlAction::up) | controlActionBit(ControlAction::stop) |
  controlActionBit(ControlAction::down) | controlActionBit(ControlAction::my) |
  controlActionBit(ControlAction::stepUp) | controlActionBit(ControlAction::stepDown) | controlActionBit(ControlAction::favourite);
}
ControlMask controlCapabilities(const ControlDescriptor &d) {
  const bool rf = radio(d.proto);
  if(!rf && !gpio(d.proto)) return 0;
  if(rf && !supportedBitLength(d.bitLength)) return 0;
  if(d.group) {
    // Group commands assume members hear the same RF identity. GPIO groups do
    // not have a member GPIO trigger path in SomfyGroup::sendCommand.
    if(!rf || !d.memberCount || d.memberCount > SOMFY_MAX_GROUPED_SHADES) return 0;
    ControlMask mask = d.commonActions & groupActions;
    if(d.bitLength != 80) mask &= ~(controlActionBit(ControlAction::stepUp) | controlActionBit(ControlAction::stepDown) | controlActionBit(ControlAction::favourite));
    // The web group flag is an any-member sensor aggregate, not an intersection.
    if(d.sunSensor) mask |= controlActionBit(ControlAction::sunOn) | controlActionBit(ControlAction::sunOff);
    return mask;
  }
  if(uint8_t(d.shadeType) > uint8_t(shade_types::rgate1) || uint8_t(d.tiltType) > uint8_t(tilt_types::euromode) ||
     (rf && !d.paired)) return 0;
  // Match everyday web buttons and the shared domain's single-button semantics.
  // A My on a one-button device is a toggle, never a safe ordinary STOP.
  if(isToggleShade(d.shadeType) || d.shadeType == shade_types::drycontact) return controlActionBit(ControlAction::toggle);
  ControlMask mask = controlActionBit(ControlAction::up) | controlActionBit(ControlAction::down);
  if(d.shadeType == shade_types::drycontact2) return mask;
  mask |= controlActionBit(ControlAction::stop) | controlActionBit(ControlAction::my);
  if(d.tiltType != tilt_types::tiltonly) mask |= controlActionBit(ControlAction::position);
  if(d.tiltType != tilt_types::none)
    mask |= controlActionBit(ControlAction::tiltUp) | controlActionBit(ControlAction::tiltStop) |
            controlActionBit(ControlAction::tiltDown) | controlActionBit(ControlAction::tiltPosition);
  if(rf && d.bitLength == 80)
    mask |= controlActionBit(ControlAction::favourite) | controlActionBit(ControlAction::stepUp) | controlActionBit(ControlAction::stepDown);
  if(d.sunSensor && d.shadeType != shade_types::garage3)
    mask |= controlActionBit(ControlAction::sunOn) | controlActionBit(ControlAction::sunOff);
  return mask;
}
ControlMask repeatableControls(const ControlDescriptor &d) {
  if(!radio(d.proto)) return 0;
  // Long MY can program/clear favourite; keep it in the staged admin flow.
  constexpr ControlMask mask=controlActionBit(ControlAction::up)|controlActionBit(ControlAction::down)|
    controlActionBit(ControlAction::favourite)|controlActionBit(ControlAction::toggle)|
    controlActionBit(ControlAction::stepUp)|controlActionBit(ControlAction::stepDown)|
    controlActionBit(ControlAction::sunOn)|controlActionBit(ControlAction::sunOff);
  return controlCapabilities(d)&mask;
}
bool allows(ControlAction action, const ControlDescriptor &d) {
  return (controlCapabilities(d) & controlActionBit(action)) != 0;
}
ControlResult prepareControlRequest(ControlAction action, const ControlDescriptor &d, int32_t id,
                                    int32_t value, int32_t step, shade_request_t &out, int32_t repeat) {
  if(id <= 0 || id >= 255) return ControlResult::invalid;
  if(!allows(action, d)) return ControlResult::unsupported;
  if(repeat != -1) {
    if(repeat<1 || repeat>127) return ControlResult::invalid;
    if(!(repeatableControls(d)&controlActionBit(action))) return ControlResult::unsupported;
  }
  const bool target = action == ControlAction::position || action == ControlAction::tiltPosition;
  const bool stepping = action == ControlAction::stepUp || action == ControlAction::stepDown;
  if((target ? value < 0 || value > 100 : value != -1) ||
     (stepping ? step < 1 || step > 127 : step != 0)) return ControlResult::invalid;
  if((action == ControlAction::stop && !d.moving) ||
     (action == ControlAction::tiltStop && !d.tiltMoving)) return ControlResult::idle;
  shade_request_t request;
  request.id = uint8_t(id);
  request.repeat = int8_t(repeat);
  request.type = d.group ? shade_request_type_t::group_command : shade_request_type_t::shade_command;
  switch(action) {
  case ControlAction::up: request.command = somfy_commands::Up; break;
  case ControlAction::down: request.command = somfy_commands::Down; break;
  case ControlAction::stop:
  case ControlAction::my: request.command = somfy_commands::My; break;
  case ControlAction::favourite: request.command = somfy_commands::Favorite; break;
  case ControlAction::toggle: request.command = somfy_commands::Toggle; break;
  case ControlAction::position: request.target = int8_t(value); break;
  case ControlAction::tiltPosition:
    request.type = shade_request_type_t::tilt_command; request.target = int8_t(value); break;
  case ControlAction::tiltUp:
  case ControlAction::tiltDown:
  case ControlAction::tiltStop:
    request.type = shade_request_type_t::tilt_command;
    request.command = action == ControlAction::tiltUp ? somfy_commands::Up :
                      action == ControlAction::tiltDown ? somfy_commands::Down : somfy_commands::My;
    break;
  case ControlAction::stepUp:
  case ControlAction::stepDown:
    request.command = action == ControlAction::stepUp ? somfy_commands::StepUp : somfy_commands::StepDown;
    request.stepSize = uint8_t(step); break;
  case ControlAction::sunOn:
  case ControlAction::sunOff: request.command = sunFlagCommand(action == ControlAction::sunOn); break;
  default: return ControlResult::unsupported;
  }
  out = request;
  return ControlResult::done;
}
}
