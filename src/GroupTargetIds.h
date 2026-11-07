#ifndef GROUP_TARGET_IDS_H
#define GROUP_TARGET_IDS_H
#include <stdint.h>
#include "ProtoSchema.h"

// Per-connection replay protection for groupTarget. The socket owns the lock.
// IDs increase on a connection; a cache eviction must never revive an old move.
struct group_target_reply_t {
  proto_error_t code = proto_error_t::none;
  const char *desc = nullptr; // static protocol error description
};
enum class target_id_result_t { fresh, pending, replay, conflict, stale, full };
class GroupTargetIds {
  public:
    static constexpr uint8_t CAPACITY = 8;
    void reset() { *this = GroupTargetIds(); }
    target_id_result_t begin(uint32_t id, uint8_t group, int8_t target, group_target_reply_t &reply) {
      for(const Entry &entry : entries) {
        if(entry.id != id || !id) continue;
        if(entry.group != group || entry.target != target) return target_id_result_t::conflict;
        reply = entry.reply;
        return entry.done ? target_id_result_t::replay : target_id_result_t::pending;
      }
      if(id <= highest) return target_id_result_t::stale;
      Entry *slot = nullptr;
      for(Entry &entry : entries) {
        if(!entry.id) { slot = &entry; break; }
        if(entry.done && (!slot || entry.id < slot->id)) slot = &entry;
      }
      if(!slot) return target_id_result_t::full;
      *slot = Entry();
      slot->id = id;
      slot->group = group;
      slot->target = target;
      highest = id;
      return target_id_result_t::fresh;
    }
    void complete(uint32_t id, group_target_reply_t reply) {
      for(Entry &entry : entries) {
        if(entry.id != id) continue;
        entry.reply = reply;
        entry.done = true;
        return;
      }
    }
  private:
    struct Entry {
      uint32_t id = 0;
      uint8_t group = 255;
      int8_t target = -1;
      bool done = false;
      group_target_reply_t reply;
    };
    Entry entries[CAPACITY];
    uint32_t highest = 0;
};
#endif
