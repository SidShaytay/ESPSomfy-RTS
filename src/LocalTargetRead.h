#pragma once
#include "LocalTargetContext.h"
#include <stddef.h>
namespace LocalUi {
struct RawMemberContext {
  uint8_t id = 0;
  bool exists = false;
  uint32_t identity = 0;
  uint64_t revision = 0;
};
// Administration captures the physical membership representation, even when
// control policy rejects it. Motion, estimates and ordinary counter advancement
// are intentionally absent. A missing target is a valid deletion tombstone.
struct RawTargetContext {
  EntityRef ref;
  bool targetExists = false;
  uint32_t identity = 0;
  uint64_t revision = 0, modelRevision = 0, restoreRevision = 0,
           radioRevision = 0;
  uint8_t slotCount = 0;
  RawMemberContext members[32];
  bool valid() const {
    return ref.valid() && modelRevision && restoreRevision && radioRevision &&
           slotCount <= 32;
  }
  bool same(const RawTargetContext &other) const {
    if (!valid() || !other.valid())
      return false;
    if (ref != other.ref || targetExists != other.targetExists ||
        identity != other.identity || revision != other.revision ||
        modelRevision != other.modelRevision ||
        restoreRevision != other.restoreRevision ||
        radioRevision != other.radioRevision || slotCount != other.slotCount ||
        slotCount > 32)
      return false;
    for (uint8_t i = 0; i < slotCount; i++)
      if (members[i].id != other.members[i].id ||
          members[i].exists != other.members[i].exists ||
          members[i].identity != other.members[i].identity ||
          members[i].revision != other.members[i].revision)
        return false;
    return true;
  }
};
static_assert(sizeof(RawTargetContext) <= 576,
              "One bounded physical target context");
// Loop only: model remains stable for this read. Explicit model injection lets
// native tests instantiate the same production capture without a hardware stub.
// No commands, NVS access, allocation or mutable module state.
template <typename Model>
bool readRawTargetContext(Model &model, EntityRef ref, RawTargetContext &out) {
  out = {};
  if (!ref.valid() || !model.configurationRevision || !model.restoreRevision ||
      !model.transceiver.configurationRevision)
    return false;
  out.ref = ref;
  out.modelRevision = model.configurationRevision;
  out.restoreRevision = model.restoreRevision;
  out.radioRevision = model.transceiver.configurationRevision;
  if (ref.kind == EntityKind::device) {
    auto *shade = model.getShadeById(ref.id);
    if (!shade)
      return true;
    out.targetExists = true;
    out.identity = shade->getRemoteAddress();
    out.revision = shade->configurationRevision;
    out.slotCount = 1;
    out.members[0] = {ref.id, true, out.identity, out.revision};
  } else {
    auto *group = model.getGroupById(ref.id);
    if (!group)
      return true;
    out.targetExists = true;
    out.identity = group->getRemoteAddress();
    out.revision = group->configurationRevision;
    constexpr size_t slots =
        sizeof(group->linkedShades) / sizeof(group->linkedShades[0]);
    static_assert(slots <= 32, "Physical group slots must fit exact capture");
    out.slotCount = slots;
    for (uint8_t i = 0; i < slots; i++) {
      auto &member = out.members[i];
      member.id = group->linkedShades[i];
      if (!member.id || member.id == 255)
        continue;
      auto *shade = model.getShadeById(member.id);
      if (!shade)
        continue;
      member.exists = true;
      member.identity = shade->getRemoteAddress();
      member.revision = shade->configurationRevision;
    }
  }
  return true;
}
// Preserve strict control validation separately from administration. Sorting
// defines the same stable member order used by control moving/eligibility bits.
inline bool makeControlTargetContext(const RawTargetContext &raw,
                                     TargetContext &out) {
  out = {};
  if (!raw.valid() || !raw.targetExists)
    return false;
  // Failures erase any partial member prefix; do not copy a second 560-byte
  // context onto the loop stack just to sanitize it.
  const auto refuse = [&out] {
    out = {};
    return false;
  };
  out.ref = raw.ref;
  out.identity = raw.identity;
  out.revision = raw.revision;
  out.modelRevision = raw.modelRevision;
  out.restoreRevision = raw.restoreRevision;
  out.radioRevision = raw.radioRevision;
  if (raw.ref.kind == EntityKind::device) {
    if (raw.slotCount != 1 || !raw.members[0].exists ||
        raw.members[0].id != raw.ref.id ||
        raw.members[0].identity != raw.identity ||
        raw.members[0].revision != raw.revision)
      return refuse();
    out.memberCount = 1;
    out.members[0] = {raw.ref.id, raw.identity, raw.revision};
  } else
    for (uint8_t i = 0; i < raw.slotCount; i++) {
      const auto &member = raw.members[i];
      if (!member.id || member.id == 255)
        continue;
      if (!member.exists)
        return refuse();
      uint8_t at = out.memberCount;
      while (at && out.members[at - 1].id > member.id) {
        out.members[at] = out.members[at - 1];
        --at;
      }
      if ((at && out.members[at - 1].id == member.id) ||
          (at < out.memberCount && out.members[at].id == member.id))
        return refuse();
      out.members[at] = {member.id, member.identity, member.revision};
      ++out.memberCount;
    }
  if (!out.valid())
    return refuse();
  return true;
}
} // namespace LocalUi
