#pragma once
#include <stdint.h>
// Read trust is independent of write durability. Invalid typed originals may
// be inspected/replaced; incomplete or unreadable records are unavailable.
enum class SettingsLoadState : uint8_t { unknown, knownEmpty, loaded, invalid, failed };
struct SettingsLoadStatus {
  SettingsLoadState state=SettingsLoadState::unknown;
  bool readable()const{return state==SettingsLoadState::knownEmpty || state==SettingsLoadState::loaded || state==SettingsLoadState::invalid;}
  bool usable()const{return state==SettingsLoadState::knownEmpty || state==SettingsLoadState::loaded;}
};
