#pragma once
#include "ProtoSchema.h"
#include <ArduinoJson.h>
// Bounds describe the actual destination representation; choices describe the
// supported enum values. The edit's authoritative allowlist remains editForm.
struct entity_field_t {
  proto_field_t field;
  int64_t lo, hi;
  const int16_t *choices = nullptr;
  uint8_t choiceCount = 0;
  const proto_field_info_t &schema() const { return protoField(field); }
};
const entity_field_t *entityField(proto_field_t field);
bool validEntityValue(const entity_field_t &, JsonVariantConst value);
