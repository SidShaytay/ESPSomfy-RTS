#pragma once
#include "LocalStopLease.h"
namespace LocalUi {
enum class EntityKind : uint8_t { device, group };
struct EntityRef {
  EntityKind kind=EntityKind::device;
  uint8_t id=0;
  bool valid() const { return id && id!=255 && (kind==EntityKind::device || kind==EntityKind::group); }
  bool operator==(const EntityRef &other) const { return kind==other.kind && id==other.id; }
  bool operator!=(const EntityRef &other) const { return !(*this==other); }
};
struct MemberContext {
  uint8_t id=0;
  uint32_t identity=0;
  uint64_t revision=0;
};
// Owned by the loop adapter, never copied into a catalogue, task stack model or
// every queue item. Motion/estimates/ordinary rolling-code advancement are absent.
struct TargetContext {
  EntityRef ref;
  uint32_t identity=0;
  uint64_t revision=0, modelRevision=0, restoreRevision=0, radioRevision=0;
  uint8_t memberCount=0;
  MemberContext members[32];
  bool valid() const {
    if(!ref.valid() || !revision || !modelRevision || !restoreRevision || !radioRevision || memberCount>32)
      return false;
    for(uint8_t i=0;i<memberCount;i++)
      if(!members[i].id || members[i].id==255 || !members[i].revision ||
         (i && members[i-1].id>=members[i].id)) return false;
    return ref.kind==EntityKind::group ||
      (memberCount==1 && members[0].id==ref.id && members[0].identity==identity && members[0].revision==revision);
  }
  bool same(const TargetContext &other) const {
    if(!valid() || !other.valid() || ref!=other.ref || identity!=other.identity || revision!=other.revision ||
       modelRevision!=other.modelRevision || restoreRevision!=other.restoreRevision ||
       radioRevision!=other.radioRevision || memberCount!=other.memberCount) return false;
    for(uint8_t i=0;i<memberCount;i++)
      if(members[i].id!=other.members[i].id || members[i].identity!=other.members[i].identity ||
         members[i].revision!=other.members[i].revision) return false;
    return true;
  }
  StopContext stopContext(uint64_t token,uint32_t securityGeneration,uint32_t sessionGeneration) const {
    StopContext out;
    out.id=ref.id; out.group=ref.kind==EntityKind::group; out.identity=identity;
    out.configuration=token; out.securityGeneration=securityGeneration; out.sessionGeneration=sessionGeneration; out.members=memberCount;
    for(uint8_t i=0;i<memberCount && i<32;i++) {
      out.memberIds[i]=members[i].id; out.memberIdentities[i]=members[i].identity;
    }
    return out;
  }
};
static_assert(sizeof(TargetContext)<=576,"Focused 32-member guard must stay bounded");
} // namespace LocalUi
