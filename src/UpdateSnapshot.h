#pragma once
#include "ConfigRecords.h"
// A normal model snapshot has no backup-only settings/network/radio records.
// Reserve the maximum once so legacy String writers cannot silently drop a
// character when a later concat tries to grow a fragmented internal heap.
constexpr uint32_t UPDATE_SNAPSHOT_CAPACITY = SHADE_HDR_SIZE +
  SOMFY_MAX_ROOMS * ROOM_REC_SIZE + SOMFY_MAX_SHADES * SHADE_REC_SIZE +
  SOMFY_MAX_GROUPS * GROUP_REC_SIZE + REPEATER_REC_SIZE;
inline bool updateSnapshotComplete(const config_header_t &header, uint32_t bytes) {
  return bytes <= UPDATE_SNAPSHOT_CAPACITY && bytes == expectedFileBytes(header);
}
