#ifndef CONFIG_VALIDATION_H
#define CONFIG_VALIDATION_H

#include "ConfigHeader.h"
#include "ShadeTypes.h"

// readModel() indexes the controller's fixed arrays using these counts.
// Check them before loading either shades.cfg or a restored backup.
inline bool modelRecordCountsFit(const config_header_t &header) {
  return header.roomRecords <= SOMFY_MAX_ROOMS
    && header.shadeRecords <= SOMFY_MAX_SHADES
    && header.groupRecords <= SOMFY_MAX_GROUPS;
}

#endif
