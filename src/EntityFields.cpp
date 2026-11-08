#include "EntityFields.h"
#include <string.h>
const entity_field_t *entityField(proto_field_t field) {
  static const entity_field_t fields[] = {
      {proto_field_t::name, 0, 20}, {proto_field_t::sortOrder, -128, 127}};
  for (const auto &f : fields)
    if (f.field == field)
      return &f;
  return nullptr;
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
    if (f.choices[i] == n)
      return true;
  return false;
}
