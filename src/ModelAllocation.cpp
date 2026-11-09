#include "Somfy.h"
#include "ModelEditor.h"

// A deleted shade, group or room leaves a gap, so a new one takes the lowest
// free id, from 1 up to the maximum.
uint8_t SomfyShadeController::getNextShadeId() {
  uint8_t ids[SOMFY_MAX_SHADES];
  for(uint8_t i = 0; i < SOMFY_MAX_SHADES; i++) ids[i] = this->shades[i].getShadeId();
  const uint8_t id = firstFreeId(ids, SOMFY_MAX_SHADES, SOMFY_MAX_SHADES);
  return id == 0 ? 255 : id;
}

int8_t SomfyShadeController::getMaxShadeOrder() {
  int8_t orders[SOMFY_MAX_SHADES];
  size_t count = 0;
  for(uint8_t i = 0; i < SOMFY_MAX_SHADES; i++)
    if(this->shades[i].getShadeId() != 255) orders[count++] = this->shades[i].sortOrder;
  return nextSortOrder(orders, count) - 1;
}

int8_t SomfyShadeController::getMaxGroupOrder() {
  int8_t orders[SOMFY_MAX_GROUPS];
  size_t count = 0;
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPS; i++)
    if(this->groups[i].getGroupId() != 255) orders[count++] = this->groups[i].sortOrder;
  return nextSortOrder(orders, count) - 1;
}

uint8_t SomfyShadeController::getNextGroupId() {
  uint8_t ids[SOMFY_MAX_GROUPS];
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPS; i++) ids[i] = this->groups[i].getGroupId();
  const uint8_t id = firstFreeId(ids, SOMFY_MAX_GROUPS, SOMFY_MAX_GROUPS);
  return id == 0 ? 255 : id;
}

uint32_t SomfyShadeController::getNextRemoteAddress(uint8_t id) {
  uint32_t used[SOMFY_MAX_SHADES + SOMFY_MAX_GROUPS];
  size_t count = 0;
  for(uint8_t i = 0; i < SOMFY_MAX_SHADES; i++)
    if(this->shades[i].getShadeId() != 255) used[count++] = this->shades[i].getRemoteAddress();
  for(uint8_t i = 0; i < SOMFY_MAX_GROUPS; i++)
    if(this->groups[i].getGroupId() != 255) used[count++] = this->groups[i].getRemoteAddress();
  return offeredRemoteAddress(this->startingAddress, id, used, count);
}

uint8_t SomfyShadeController::getNextRoomId() {
  uint8_t ids[SOMFY_MAX_ROOMS];
  for(uint8_t i = 0; i < SOMFY_MAX_ROOMS; i++) ids[i] = this->rooms[i].roomId;
  return firstFreeId(ids, SOMFY_MAX_ROOMS, SOMFY_MAX_ROOMS);
}

int8_t SomfyShadeController::getMaxRoomOrder() {
  int8_t orders[SOMFY_MAX_ROOMS];
  size_t count = 0;
  for(uint8_t i = 0; i < SOMFY_MAX_ROOMS; i++)
    if(this->rooms[i].roomId != 0) orders[count++] = this->rooms[i].sortOrder;
  return nextSortOrder(orders, count) - 1;
}

// Stable IDs are independent of storage slots: config loading packs records.
// Both a free ID and an unused slot are required before mutating any record.
SomfyShade *SomfyShadeController::addShade() {
  if(!modelLoad.usable())return nullptr;
  const uint8_t shadeId = this->getNextShadeId();
  if(shadeId == 255) return nullptr;
  for(auto &shade : this->shades) {
    if(shade.getShadeId() != 255) continue;
    const int8_t order = this->getMaxShadeOrder() + 1;
    // Reset only the selected unused slot. Existing and legacy loaded records
    // keep their GPIO values, including their historical missing-field fallback.
    shade.clear();
    shade.roomId = 0;
    shade.setFlags(0);
    shade.gpioUp = shade.gpioDown = shade.gpioMy = 255;
    shade.gpioFlags = 0;
    shade.gpioDir = 0;
    shade.gpioRelease = 0;
    shade.lastFrame = {};
    shade.setShadeId(shadeId);
    shade.sortOrder = order;
    this->isDirty = true;
    return &shade;
  }
  return nullptr;
}

SomfyRoom *SomfyShadeController::addRoom() {
  if(!modelLoad.usable())return nullptr;
  const uint8_t roomId = this->getNextRoomId();
  if(roomId == 0) return nullptr;
  for(auto &room : this->rooms) {
    if(room.roomId != 0) continue;
    const int8_t order = this->getMaxRoomOrder() + 1;
    room.roomId = roomId;
    room.sortOrder = order;
    this->isDirty = true;
    return &room;
  }
  return nullptr;
}

SomfyGroup *SomfyShadeController::addGroup() {
  if(!modelLoad.usable())return nullptr;
  const uint8_t groupId = this->getNextGroupId();
  if(groupId == 255) return nullptr;
  for(auto &group : this->groups) {
    if(group.getGroupId() != 255) continue;
    const int8_t order = this->getMaxGroupOrder() + 1;
    group.clear();
    group.flags = 0;
    group.flipCommands = false;
    group.lastFrame = {};
    group.lastRollingCode = 0; // Fresh RAM only; next use reads its address's NVS.
    group.direction = 0;
    group.changed = 0;
    group.setGroupId(groupId);
    group.sortOrder = order;
    this->isDirty = true;
    return &group;
  }
  return nullptr;
}
