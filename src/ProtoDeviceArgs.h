#pragma once
#include <ArduinoJson.h>
#include <stdint.h>
#include "ProtoSchema.h"
#include "LoopQueue.h"
// Argument parsing for the gitUpdate and battery ops. Kept free of the socket so
// it can be tested on the PC. Returns nullptr, or why the request is refused.
namespace ProtoDeviceArgs {
enum : int8_t { updateStatus = 0, updateCheck = 1, updateInstall = 2, updateCancel = 3 };
// Charge limits, as indices: 0 100% (charger default), 1 90%, 2 80%, 3 70%, 4 60%
// (BatteryCharger::Limit).
constexpr int8_t chargeLimitCount = 5;
// A nonzero uint64 as decimal text, as the browser cannot hold it as a number.
inline bool decimalId(JsonVariantConst value, uint64_t &out) {
  if(!value.is<const char *>()) return false;
  const auto text = value.as<JsonString>();
  if(!text.size() || text.size() > 20 || text.c_str()[0] == '0') return false;
  uint64_t parsed = 0;
  for(size_t i = 0; i < text.size(); ++i) {
    const char c = text.c_str()[i];
    if(c < '0' || c > '9' || parsed > (UINT64_MAX - uint64_t(c - '0')) / 10) return false;
    parsed = parsed * 10 + uint64_t(c - '0');
  }
  out = parsed;
  return parsed != 0;
}
inline const char *parse(uint8_t type, JsonObjectConst d, proto_device_args_t &args) {
  args = {};
  if(type == uint8_t(proto_type_t::gitUpdate)) {
    const auto action = d[protoField(proto_field_t::updateAction).key];
    if(action.isNull()) { args.action = updateStatus; }
    else if(!action.is<int>() || action.as<int>() < updateStatus || action.as<int>() > updateCancel) return "Unknown update action";
    else args.action = int8_t(action.as<int>());
    for(JsonPairConst pair : d) {
      const char *key = pair.key().c_str();
      if(!strcmp(key, protoField(proto_field_t::updateAction).key)) continue;
      if(!strcmp(key, protoField(proto_field_t::updateCandidate).key)) {
        if(args.action != updateInstall || !decimalId(pair.value(), args.id)) return "An exact release candidate is required";
      } else if(!strcmp(key, protoField(proto_field_t::updateOperation).key)) {
        if(args.action != updateCancel || !decimalId(pair.value(), args.id)) return "An exact update operation is required";
      } else return "Unknown update field";
    }
    if(args.action == updateInstall && !args.id) return "An exact release candidate is required";
    if(args.action == updateCancel && !args.id) return "An exact update operation is required";
    return nullptr;
  }
  if(type == uint8_t(proto_type_t::battery)) {
    for(JsonPairConst pair : d) {
      if(strcmp(pair.key().c_str(), protoField(proto_field_t::chargeLimit).key)) return "Unknown battery field";
      if(!pair.value().is<int>() || pair.value().as<int>() < 0 || pair.value().as<int>() >= chargeLimitCount) return "Unknown charge limit";
      args.value = int8_t(pair.value().as<int>());
    }
    return nullptr;
  }
  return "Not a device argument op";
}
}
