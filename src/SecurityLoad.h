#pragma once
#include <stdint.h>
#include <stddef.h>
enum class SecurityLoadState : uint8_t { unknown, knownEmpty, loaded, failed };
inline bool securityLoadTrusted(SecurityLoadState state) {
  return state==SecurityLoadState::knownEmpty || state==SecurityLoadState::loaded;
}
// Private loading draft, never render or log this record. Every exit wipes it.
struct SecurityRecord {
  uint8_t type=0;
  char username[33]={},password[33]={},pin[5]={};
  void clear() {
    volatile uint8_t *bytes=reinterpret_cast<volatile uint8_t *>(this);
    for(size_t i=0;i<sizeof(*this);++i)bytes[i]=0;
  }
  ~SecurityRecord(){clear();}
};
static_assert(sizeof(SecurityRecord)==72,"Bound credential loading drafts");
// Only exact NVS NOT_FOUND is absence. No storage write or reset is performed.
// Output stays empty on failure; callers publish only a trusted complete draft.
SecurityLoadState readSecurityRecord(SecurityRecord &);
