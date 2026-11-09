#include "LocalLearnCandidates.h"
#include "BitLength.h"
#include "SomfyFrame.h"
#include <stddef.h>
#include <type_traits>
namespace LocalUi {
namespace {
void erase(void *value, size_t size) {
  auto *byte = static_cast<volatile unsigned char *>(value);
  while (size--)
    *byte++ = 0;
}
bool sameKey(const LearnCandidate &entry, const ReceiveObservation &value) {
  return entry.address == value.address && entry.protocol == value.protocol &&
         entry.bits == value.bits;
}
bool supportedProtocol(uint8_t value) {
  return value == static_cast<uint8_t>(radio_proto::RTS) ||
         value == static_cast<uint8_t>(radio_proto::RTW) ||
         value == static_cast<uint8_t>(radio_proto::RTV);
}
} // namespace
void LearnCandidates::finish(LearnCheck reason) {
  if (subscription.generation)
    hub.unsubscribe(subscription, *this);
  subscription = {};
  static_assert(std::is_trivially_copyable<LearnCandidate>::value &&
                    std::is_trivially_copyable<LearnSelection>::value,
                "Only plain candidate data is byte-erased");
  erase(records, sizeof(records));
  erase(&selected, sizeof(selected));
  startedAt = observedAt = lastSequence = 0;
  total = 0;
  chosen = 255;
  watching = overflow = false;
  lifecycle = reason;
}
bool LearnCandidates::begin(uint64_t now) {
  end();
  if (!hub.subscribe(ReceiveChannel::learning, *this, subscription)) {
    lifecycle = LearnCheck::busy;
    return false;
  }
  startedAt = observedAt = now;
  lifecycle = LearnCheck::ready;
  return true;
}
LearnCheck LearnCandidates::tick(uint64_t now) {
  if (lifecycle != LearnCheck::ready)
    return lifecycle;
  if (!hub.observing(subscription, *this)) {
    finish(LearnCheck::inactive);
    return lifecycle;
  }
  if (now < observedAt) {
    finish(LearnCheck::clock);
    return lifecycle;
  }
  if (now - startedAt >= windowMs) {
    finish(LearnCheck::expired);
    return lifecycle;
  }
  observedAt = now;
  return lifecycle;
}
void LearnCandidates::received(const ReceiveObservation &value, ReceivePulses) {
  if (tick(value.observedAt) != LearnCheck::ready)
    return;
  if (!value.sequence || value.sequence <= lastSequence) {
    finish(LearnCheck::sequence);
    return;
  }
  lastSequence = value.sequence;
  // Sensor(code0), unknown commands accepted by the decoder, and all three
  // actual received protocols remain eligible. Never invent a button filter.
  if (!value.valid() || value.scanning() || !supportedBitLength(value.bits) ||
      !supportedProtocol(value.protocol) || !value.address ||
      value.address > 0xffffff)
    return;
  if (watching && value.address != selected.address)
    return;
  bool foundAddress = false, previousRegression = false;
  uint16_t previousHighwater = 0;
  uint8_t exact = 255;
  for (uint8_t i = 0; i < total; i++) {
    if (records[i].address != value.address)
      continue;
    foundAddress = true;
    previousRegression |= (records[i].flags & regressedFlag) != 0;
    if (records[i].highwater > previousHighwater)
      previousHighwater = records[i].highwater;
    if (sameKey(records[i], value))
      exact = i;
  }
  const bool lower = foundAddress && value.rollingCode < previousHighwater;
  const uint16_t highwater = value.rollingCode > previousHighwater
                                 ? value.rollingCode
                                 : previousHighwater;
  for (uint8_t i = 0; i < total; i++)
    if (records[i].address == value.address) {
      records[i].highwater = highwater;
      if (lower)
        records[i].flags |= regressedFlag;
    }
  // NVS is keyed by ADDRESS, not protocol/length. Continue watching that
  // address after selection, even when a changed format cannot get a slot.
  if (watching &&
      (value.protocol != selected.protocol || value.bits != selected.bits))
    records[chosen].flags |= formatFlag;
  if (exact == 255) {
    if (watching)
      return;
    if (total == 8) {
      overflow = true;
      return;
    }
    auto &entry = records[total++];
    entry.address = value.address;
    entry.protocol = value.protocol;
    entry.bits = value.bits;
    entry.rollingCode = value.rollingCode;
    entry.lastCode = value.rollingCode;
    entry.highwater = highwater;
    entry.revision = value.sequence;
    entry.observedAt = value.observedAt;
    entry.command = value.command;
    entry.rssi = value.rssi;
    if (lower || previousRegression)
      entry.flags |= regressedFlag;
    return;
  }
  auto &entry = records[exact];
  entry.lastCode = value.rollingCode;
  if (value.rollingCode >= entry.rollingCode) {
    entry.observedAt = value.observedAt;
    entry.rssi = value.rssi;
    entry.command = value.command;
    if (value.rollingCode > entry.rollingCode) {
      entry.rollingCode = value.rollingCode;
      entry.revision = value.sequence;
    }
  }
}
bool LearnCandidates::candidate(uint8_t index, LearnCandidate &out) const {
  out = {};
  if (lifecycle != LearnCheck::ready || index >= total)
    return false;
  out = records[index];
  return true;
}
LearnCheck LearnCandidates::eligible(const LearnCandidate &entry) const {
  if (entry.flags & regressedFlag)
    return LearnCheck::regressed;
  if ((entry.flags & formatFlag) || entry.rollingCode < entry.highwater)
    return LearnCheck::changed;
  return LearnCheck::ready;
}
LearnCheck LearnCandidates::select(uint8_t index, uint64_t now,
                                   LearnSelection &out) {
  out = {};
  watching = false;
  chosen = 255;
  erase(&selected, sizeof(selected));
  const auto current = tick(now);
  if (current != LearnCheck::ready)
    return current;
  if (index >= total)
    return LearnCheck::invalid;
  const auto check = eligible(records[index]);
  if (check != LearnCheck::ready)
    return check;
  const auto &entry = records[index];
  selected = {subscription.generation, entry.revision, entry.address,
              entry.rollingCode,       entry.protocol, entry.bits};
  chosen = index;
  watching = true;
  out = selected;
  return LearnCheck::ready;
}
LearnCheck LearnCandidates::validate(const LearnSelection &value,
                                     uint64_t now) {
  const auto current = tick(now);
  if (current != LearnCheck::ready)
    return current;
  if (!watching || chosen >= total || !value.session ||
      value.session != subscription.generation ||
      value.session != selected.session ||
      value.revision != selected.revision ||
      value.address != selected.address ||
      value.rollingCode != selected.rollingCode ||
      value.protocol != selected.protocol || value.bits != selected.bits)
    return LearnCheck::changed;
  const auto &entry = records[chosen];
  const auto check = eligible(entry);
  if (check != LearnCheck::ready)
    return check;
  return entry.revision == value.revision &&
                 entry.rollingCode == value.rollingCode &&
                 entry.address == value.address &&
                 entry.protocol == value.protocol && entry.bits == value.bits
             ? LearnCheck::ready
             : LearnCheck::changed;
}
} // namespace LocalUi
