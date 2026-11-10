#pragma once
#include <stdint.h>
// Exact retained storage, including absence. A missing key and stored zero are
// different review states. No namespace/key is created by a review read.
struct StoredCounter {
  uint16_t code = 0;
  bool present = false;
  bool same(const StoredCounter &other) const {
    return present == other.present && code == other.code;
  }
};
// Physical model slots, not a hash or high-water approximation: a change to a
// lower-valued alias must also invalidate an operator's deliberate correction.
struct PrimaryCounterReview {
  static constexpr unsigned aliasSlots = 272; // 32 * (primary + 7 linked) + 16 groups
  uint32_t address = 0;
  uint16_t primary = 0;
  uint8_t shadeId = 0;
  StoredCounter stored;
  uint16_t codes[aliasSlots]{};
  uint8_t matching[(aliasSlots + 7) / 8]{};
  bool includes(unsigned slot) const {
    return slot < aliasSlots && (matching[slot / 8] & (1u << (slot % 8)));
  }
  unsigned aliasCount() const {
    unsigned count = 0;
    for (unsigned i = 0; i < aliasSlots; ++i) count += includes(i);
    return count;
  }
  bool same(const PrimaryCounterReview &other) const {
    if(address!=other.address || shadeId!=other.shadeId || primary!=other.primary || !stored.same(other.stored))return false;
    for(unsigned i=0;i<aliasSlots;++i)
      if(includes(i)!=other.includes(i) || (includes(i) && codes[i]!=other.codes[i]))return false;
    return true;
  }
  bool reconciled(uint16_t code) const {
    if (!address || !stored.present || stored.code != code || primary != code)
      return false;
    unsigned count = 0;
    for (unsigned i = 0; i < aliasSlots; ++i) if (includes(i)) {
      ++count;
      if (codes[i] != code) return false;
    }
    return count != 0;
  }
};
static_assert(sizeof(PrimaryCounterReview) <= 600, "One bounded exact primary-counter review");
