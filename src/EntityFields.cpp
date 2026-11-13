#include "EntityFields.h"
#include "LiftCalibration.h"
#include "ProtoEdits.h"
#include "ShadeGpioPolicy.h"
#include <string.h>
namespace {
constexpr entity_choice_t protocols[] = {
    {0, "RTS"}, {1, "RTW"}, {2, "RTV"}, {8, "GPIO relay"}, {9, "GPIO remote"}};
constexpr entity_choice_t bits[] = {{56, "56 bit"}, {80, "80 bit"}};
constexpr entity_choice_t shades[] = {{0, "Roller shade"},
                                      {1, "Blind"},
                                      {2, "Drapery left"},
                                      {3, "Awning"},
                                      {4, "Shutter"},
                                      {5, "Garage 1-button"},
                                      {6, "Garage 3-button"},
                                      {7, "Drapery right"},
                                      {8, "Drapery center"},
                                      {9, "Dry contact"},
                                      {10, "Dry contact 2-button"},
                                      {11, "Gate left"},
                                      {12, "Gate center"},
                                      {13, "Gate right"},
                                      {14, "Gate 1-button left"},
                                      {15, "Gate 1-button center"},
                                      {16, "Gate 1-button right"}};
constexpr entity_choice_t calibrationKinds[] = {{0,"Legacy timing"},{1,"Direct travel"},{2,"Segmented / slack"}};
constexpr entity_choice_t tilts[] = {{0, "None"},
                                     {1, "Separate motor"},
                                     {2, "Integrated"},
                                     {3, "Tilt only"},
                                     {4, "Euro mode"}};
} // namespace
bool isLiftCalibrationField(proto_field_t field) {
  switch(field) {
    case proto_field_t::calibrationKind:
    case proto_field_t::upTakeupMs:
    case proto_field_t::upMidpointMs:
    case proto_field_t::upSettlingMs:
    case proto_field_t::upReversalMs:
    case proto_field_t::downTakeupMs:
    case proto_field_t::downMidpointMs:
    case proto_field_t::downSettlingMs:
    case proto_field_t::downReversalMs:
      return true;
    default: return false;
  }
}
const entity_field_t *entityField(model_edit_type_t type, proto_field_t field) {
  if(type==model_edit_type_t::link_to_group || type==model_edit_type_t::unlink_from_group) {
    static const entity_field_t groupId{proto_field_t::groupId,1,SOMFY_MAX_GROUPS};
    static const entity_field_t addId{proto_field_t::shadeId,1,SOMFY_MAX_SHADES};
    static const entity_field_t removeId{proto_field_t::shadeId,1,UINT8_MAX};
    if(field==proto_field_t::groupId)return &groupId;
    if(field==proto_field_t::shadeId)return type==model_edit_type_t::link_to_group?&addId:&removeId;
    return nullptr;
  }
  const auto *form = editForm(type);
  if (!form)
    return nullptr;
  bool allowed = type == model_edit_type_t::save_shade && isLiftCalibrationField(field);
  for (uint8_t i = 0; i < form->count; i++)
    allowed |= form->fields[i] == field;
  if (!allowed)
    return nullptr;
  static const entity_field_t fields[] = {
      {proto_field_t::name, 0, 20},
      {proto_field_t::sortOrder, INT8_MIN, INT8_MAX},
      {proto_field_t::roomId, 0, SOMFY_MAX_ROOMS},
      {proto_field_t::remoteAddress, 0, 0xffffff},
      {proto_field_t::shadeType, 0, 16, shades, 17},
      {proto_field_t::tiltType, 0, 4, tilts, 5},
      {proto_field_t::bitLength, 56, 80, bits, 2},
      {proto_field_t::proto, 0, 9, protocols, 5},
      {proto_field_t::upTime, 0, UINT32_MAX},
      {proto_field_t::downTime, 0, UINT32_MAX},
      {proto_field_t::calibrationKind, 0, 2, calibrationKinds, 3},
      {proto_field_t::upTakeupMs, 0, liftCalibrationMaxMs},
      {proto_field_t::upMidpointMs, 0, liftCalibrationMaxMs},
      {proto_field_t::upSettlingMs, 0, liftCalibrationMaxMs},
      {proto_field_t::upReversalMs, 0, liftCalibrationMaxMs},
      {proto_field_t::downTakeupMs, 0, liftCalibrationMaxMs},
      {proto_field_t::downMidpointMs, 0, liftCalibrationMaxMs},
      {proto_field_t::downSettlingMs, 0, liftCalibrationMaxMs},
      {proto_field_t::downReversalMs, 0, liftCalibrationMaxMs},
      {proto_field_t::tiltTime, 0, UINT32_MAX},
      {proto_field_t::stepSize, 0, UINT16_MAX},
      {proto_field_t::flipCommands, 0, 1},
      {proto_field_t::flipPosition, 0, 1},
      {proto_field_t::sunSensor, 0, 1},
      {proto_field_t::light, 0, 1},
      {proto_field_t::simMy, 0, 1},
      {proto_field_t::repeats, 0, UINT8_MAX},
      {proto_field_t::gpioUp, 0, UINT8_MAX},
      {proto_field_t::gpioDown, 0, UINT8_MAX},
      {proto_field_t::gpioMy, 0, UINT8_MAX},
      {proto_field_t::gpioLLTrigger, 0, 1}};
  static const entity_field_t groupProtocol{proto_field_t::proto, 0, 2,
                                            protocols, 3};
  if (field == proto_field_t::proto && (type == model_edit_type_t::add_group ||
                                        type == model_edit_type_t::save_group))
    return &groupProtocol;
  for (const auto &f : fields)
    if (f.field == field)
      return &f;
  return nullptr;
}
const char *entityFieldLabel(proto_field_t field) {
  switch (field) {
  case proto_field_t::name:
    return "Name";
  case proto_field_t::sortOrder:
    return "Sort order";
  case proto_field_t::roomId:
    return "Room";
  case proto_field_t::remoteAddress:
    return "Remote address";
  case proto_field_t::shadeType:
    return "Shade type";
  case proto_field_t::tiltType:
    return "Tilt type";
  case proto_field_t::bitLength:
    return "Frame size";
  case proto_field_t::proto:
    return "Protocol";
  case proto_field_t::upTime:
    return "Up time (ms)";
  case proto_field_t::downTime:
    return "Down time (ms)";
  case proto_field_t::calibrationKind:
    return "Calibration type";
  case proto_field_t::upTakeupMs:
    return "Up takeup (ms)";
  case proto_field_t::upMidpointMs:
    return "Up midpoint (ms)";
  case proto_field_t::upSettlingMs:
    return "Up settling (ms)";
  case proto_field_t::upReversalMs:
    return "Up reversal (ms)";
  case proto_field_t::downTakeupMs:
    return "Down takeup (ms)";
  case proto_field_t::downMidpointMs:
    return "Down midpoint (ms)";
  case proto_field_t::downSettlingMs:
    return "Down settling (ms)";
  case proto_field_t::downReversalMs:
    return "Down reversal (ms)";
  case proto_field_t::tiltTime:
    return "Tilt time (ms)";
  case proto_field_t::stepSize:
    return "Step size";
  case proto_field_t::flipCommands:
    return "Invert commands";
  case proto_field_t::flipPosition:
    return "Invert position";
  case proto_field_t::sunSensor:
    return "Sun sensor";
  case proto_field_t::light:
    return "Light support";
  case proto_field_t::simMy:
    return "Simulate favorite";
  case proto_field_t::repeats:
    return "Repeats";
  case proto_field_t::gpioUp:
    return "Up GPIO";
  case proto_field_t::gpioDown:
    return "Down GPIO";
  case proto_field_t::gpioMy:
    return "My GPIO";
  case proto_field_t::gpioLLTrigger:
    return "Low-level trigger";
  default:
    return protoField(field).name;
  }
}
bool validEntityValue(const entity_field_t &f, JsonVariantConst value) {
  if (f.schema().kind == 's') {
    if (!value.is<const char *>())
      return false;
    const auto text = value.as<JsonString>();
    return text.size() <= uint64_t(f.hi) &&
           !memchr(text.c_str(), 0, text.size());
  }
  if (f.schema().kind == 'b')
    return value.is<bool>() ||
           (value.is<int>() && (value.as<int>() == 0 || value.as<int>() == 1));
  if (!value.is<int64_t>())
    return false;
  const auto n = value.as<int64_t>();
  if (n < f.lo || n > f.hi)
    return false;
  if (!f.choiceCount)
    return true;
  for (uint8_t i = 0; i < f.choiceCount; i++)
    if (f.choices[i].value == n)
      return true;
  return false;
}

bool entityFieldActive(const entity_field_t &f, radio_proto protocol) {
  const auto mask = ShadeGpioPolicy::physicalMask(protocol);
  switch (f.field) {
  case proto_field_t::gpioUp:
    return mask & ShadeGpioPolicy::up;
  case proto_field_t::gpioDown:
    return mask & ShadeGpioPolicy::down;
  case proto_field_t::gpioMy:
    return mask & ShadeGpioPolicy::my;
  case proto_field_t::gpioLLTrigger:
    return mask != 0;
  default:
    return true;
  }
}
