#pragma once
#include <stddef.h>
#include <stdint.h>
#include "LocalTargetContext.h"

namespace LocalUi {
constexpr size_t quickAccessSlots=3, quickAccessBytes=32;
static_assert(quickAccessBytes<=64,"Quick access stays one bounded preference record");
struct QuickAccessEntry {
  EntityRef ref;
  uint32_t identity=0; // Current 24-bit RF identity anchor, never a generated address.
  bool empty()const {return ref.id==0;}
};
enum class QuickAccessEdit:uint8_t {changed,unchanged,invalidSlot,invalidBinding,duplicate};
class QuickAccess {
  QuickAccessEntry entries[quickAccessSlots];
public:
  const QuickAccessEntry *at(size_t slot)const {return slot<quickAccessSlots?&entries[slot]:nullptr;}
  QuickAccessEdit assign(size_t slot,EntityRef ref,uint32_t identity);
  QuickAccessEdit remove(size_t slot);
  // Move one slot, including an empty slot; shift intervening slots in order.
  QuickAccessEdit move(size_t from,size_t to);
};
// Missing storage and an I/O failure are not a valid empty saved preference.
// Every non-valid result clears the output. No I/O or persistence is performed.
enum class QuickAccessLoad:uint8_t {valid,missing,invalid,unreadable};
enum class QuickAccessReadState:uint8_t {present,missing,unreadable};
struct QuickAccessRead {QuickAccessReadState state;const uint8_t *bytes;size_t size;};
QuickAccessLoad loadQuickAccess(const QuickAccessRead &,QuickAccess &out);
QuickAccessLoad decodeQuickAccess(const uint8_t *,size_t,QuickAccess &out);
// Exactly 32 bytes: "LQA", version 1, three 8-byte slots, CRC32/IEEE LE.
// Slot: kind (0 empty/1 device/2 group), id, RF address BE24, three zero bytes.
// Empty slots are all zero. CRC detects corruption; it is not authentication.
// A short/null output is refused without touching it; bytes after 32 are untouched.
bool encodeQuickAccess(const QuickAccess &,uint8_t *out,size_t capacity);
} // namespace LocalUi
