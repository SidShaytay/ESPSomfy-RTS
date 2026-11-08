#include "ModelRecords.h"
shade_record_t normalizedModelShadeRecord(const shade_record_t &source) {
  shade_record_t out = source;
  if (out.tiltType == tilt_types::none || out.shadeType != shade_types::blind) {
    out.tiltType = tilt_types::none;
    out.myTiltPos = -1;
    out.currentTiltPos = 0;
  }
  return out;
}
bool writeModelRecords(ConfigWriter &writer, const config_header_t &header,
                       ModelRecordSource &source) {
  if (header.roomRecords > SOMFY_MAX_ROOMS ||
      header.shadeRecords > SOMFY_MAX_SHADES ||
      header.groupRecords > SOMFY_MAX_GROUPS)
    return false;
  if (!writeConfigHeader(writer, header))
    return false;
  uint8_t rooms = 0, shades = 0, groups = 0;
  for (uint8_t slot = 0; slot < SOMFY_MAX_ROOMS; slot++) {
    room_record_t record;
    if (!source.room(slot, record))
      continue;
    if (!writeRoomRecord(writer, record))
      return false;
    ++rooms;
  }
  for (uint8_t slot = 0; slot < SOMFY_MAX_SHADES; slot++) {
    shade_record_t record;
    if (!source.shade(slot, record))
      continue;
    const auto normalized = normalizedModelShadeRecord(record);
    if (!writeShadeRecord(writer, normalized))
      return false;
    ++shades;
  }
  for (uint8_t slot = 0; slot < SOMFY_MAX_GROUPS; slot++) {
    group_record_t record;
    if (!source.group(slot, record))
      continue;
    if (!writeGroupRecord(writer, record))
      return false;
    ++groups;
  }
  uint32_t repeaters[SOMFY_MAX_REPEATERS];
  source.repeaters(repeaters);
  return writeRepeaterRecord(writer, repeaters) &&
         rooms == header.roomRecords && shades == header.shadeRecords &&
         groups == header.groupRecords;
}
