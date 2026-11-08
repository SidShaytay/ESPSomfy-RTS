#pragma once
#include "ConfigRecords.h"
// One record at a time from the stable, loop-owned model; false means an empty
// slot. Implementations only copy metadata and never access rolling-code NVS.
class ModelRecordSource {
public:
  virtual ~ModelRecordSource() = default;
  virtual bool room(uint8_t slot, room_record_t &out) = 0;
  virtual bool shade(uint8_t slot, shade_record_t &out) = 0;
  virtual bool group(uint8_t slot, group_record_t &out) = 0;
  virtual void repeaters(uint32_t (&out)[SOMFY_MAX_REPEATERS]) = 0;
};
// Normalize only this serialized copy. A retry must not clear live tilt state.
shade_record_t normalizedModelShadeRecord(const shade_record_t &source);
// The header is prepared by the caller, including any backup record sizes.
// Record counts are checked against what was actually emitted.
bool writeModelRecords(ConfigWriter &writer, const config_header_t &header,
                       ModelRecordSource &source);
