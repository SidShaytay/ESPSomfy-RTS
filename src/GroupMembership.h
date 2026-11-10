#pragma once
#include <stdint.h>
#include <stddef.h>
// Physical slots, including holes and legacy 255 placeholders. Only zero is a
// free insertion slot in the existing model list helper. No RF/protocol policy.
struct GroupMembershipInfo {
  uint8_t occurrences=0,freeSlots=0;
};
template<size_t N>
GroupMembershipInfo inspectGroupMembership(const uint8_t (&slots)[N],uint8_t id) {
  static_assert(N<=32,"Bounded physical group membership");
  GroupMembershipInfo info;
  for(auto stored:slots) {
    if(!stored)++info.freeSlots;
    if(id && stored==id)++info.occurrences;
  }
  return info;
}
