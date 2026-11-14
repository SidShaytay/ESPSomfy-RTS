#pragma once
#include <stddef.h>
#include <stdint.h>
struct MotionDiagnosticRecord {
  uint64_t sequence = 0;
  uint32_t at = 0, ticket = 0;
  int32_t value = 0;
  uint8_t category = 0, kind = 0, shadeId = 0;
};
struct MotionDiagnosticsRead { size_t count = 0; uint64_t next = 0; bool overflow = false; };
// Loop-owned, bounded numeric evidence. No remote addresses or credentials.
class MotionDiagnostics {
public:
  static constexpr size_t capacity = 32;
private:
  MotionDiagnosticRecord records[capacity]{};
  size_t head = 0, count = 0;
  uint64_t sequence = 0;
  uint32_t armedAt = 0, ttl = 0;
  uint8_t mask = 0;
  mutable bool expired = false;
public:
  bool configure(uint8_t value, uint32_t duration, uint32_t now) {
    if(value > 7 || (value && (!duration || duration > 900000))) return false;
    mask = value; ttl = duration; armedAt = now; expired = false;
    if(!value) head = count = 0;
    return true;
  }
  bool enabled(uint32_t now) const {
    if(!mask || expired) return false;
    if(now - armedAt >= ttl) { expired = true; return false; }
    return true;
  }
  bool append(uint8_t category, uint8_t kind, uint8_t shadeId, uint32_t ticket,
              int32_t value, uint32_t now) {
    if(!enabled(now) || (category != 1 && category != 2 && category != 4) || !(category & mask) || sequence == UINT64_MAX) return false;
    records[head] = {++sequence,now,ticket,value,category,kind,shadeId};
    head = (head + 1) % capacity;
    if(count < capacity) ++count;
    return true;
  }
  MotionDiagnosticsRead read(uint64_t after, MotionDiagnosticRecord *out, size_t max) const {
    MotionDiagnosticsRead result{0,after,false};
    if(!out || !max || !count) return result;
    const size_t oldest = (head + capacity - count) % capacity;
    result.overflow = after < records[oldest].sequence - 1;
    for(size_t i = 0; i < count && result.count < max; ++i) {
      const auto &record = records[(oldest + i) % capacity];
      if(record.sequence <= after) continue;
      out[result.count++] = record;
      result.next = record.sequence;
    }
    return result;
  }
};
