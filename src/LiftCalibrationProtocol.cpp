#include "LiftCalibrationProtocol.h"
#include "ShadeTypes.h"
const char *parseLiftCalibrationPayload(JsonObjectConst object, LiftCalibrationRequest &out) {
  if(object.isNull()) return "Calibration requires an object";
  const auto action = object[protoField(proto_field_t::calibrationAction).key];
  if(!action.is<uint8_t>() || (action.as<uint8_t>() > uint8_t(LiftGuideAction::startReverseDown) &&
      action.as<uint8_t>() != LiftCalibrationRequest::begin && action.as<uint8_t>() != LiftCalibrationRequest::poll))
    return "Unknown calibration action";
  LiftCalibrationRequest request;
  request.action = action.as<uint8_t>();
  bool tokenGiven = false, humanGiven = false;
  for(JsonPairConst value : object) {
    proto_field_t field;
    if(!protoFieldOfKey(value.key().c_str(),field)) return "Unknown calibration field";
    switch(field) {
      case proto_field_t::calibrationAction:break;
      case proto_field_t::calibrationSession:tokenGiven = true;break;
      case proto_field_t::calibrationHuman:humanGiven = true;break;
      case proto_field_t::shadeId:case proto_field_t::calibrationKind:
        if(request.action != LiftCalibrationRequest::begin) return "Target and hardware kind belong only to Begin";
        break;
      default:return "Field is not a calibration argument";
    }
  }
  const auto token = object[protoField(proto_field_t::calibrationSession).key];
  if(tokenGiven && !token.is<uint32_t>()) return "Calibration session must be an exact integer";
  if(tokenGiven) request.token = token.as<uint32_t>();
  const auto human = object[protoField(proto_field_t::calibrationHuman).key];
  if(humanGiven) {
    if(!human.is<bool>() && !(human.is<int>() && (human.as<int>() == 0 || human.as<int>() == 1)))
      return "Observation source must be a boolean";
    request.human = human.as<bool>();
  }
  if(request.action == LiftCalibrationRequest::begin) {
    const auto id = object[protoField(proto_field_t::shadeId).key];
    const auto kind = object[protoField(proto_field_t::calibrationKind).key];
    if(!id.is<uint8_t>() || id.as<uint8_t>() < 1 || id.as<uint8_t>() > SOMFY_MAX_SHADES ||
        !kind.is<uint8_t>() || (kind.as<uint8_t>() != uint8_t(LiftKind::direct) && kind.as<uint8_t>() != uint8_t(LiftKind::segmented)) || request.token)
      return "Begin requires a shutter and supported hardware kind";
    request.id = id.as<uint8_t>(); request.kind = static_cast<LiftKind>(kind.as<uint8_t>());
  } else if(!request.token) return "A current calibration session is required";
  out = request;
  return nullptr;
}
