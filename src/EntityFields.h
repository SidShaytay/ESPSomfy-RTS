#pragma once
#include "ModelEditor.h"
#include "ProtoSchema.h"
#include <ArduinoJson.h>
struct entity_choice_t {
  int16_t value;
  const char *label;
};
// Per-operation constraints describe the destination and supported enums. The
// authoritative allowlist remains editForm; metadata never holds stored values.
struct entity_field_t {
  proto_field_t field;
  int64_t lo, hi;
  const entity_choice_t *choices = nullptr;
  uint8_t choiceCount = 0;
  const proto_field_info_t &schema() const { return protoField(field); }
};
const entity_field_t *entityField(model_edit_type_t type, proto_field_t field);
const char *entityFieldLabel(proto_field_t field);
bool validEntityValue(const entity_field_t &, JsonVariantConst value);

// Applicability follows the shared physical setup/release mask, not a
// category-specific logical command mapping.
bool entityFieldActive(const entity_field_t &, radio_proto protocol);
