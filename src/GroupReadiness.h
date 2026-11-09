#pragma once
#include "SomfyFrame.h"
#include <stddef.h>
// Loop-owned read-only classification. Cached group direction/last command is
// evidence of possible movement ONLY when member tracking is incomplete; it is
// never a replacement for the present members' actual administration readiness.
struct GroupReadiness {
  bool trackedMembers = false, missingMembers = false, activeMember = false,
       movementEvidence = false;
  bool trackedIdle() const {
    return trackedMembers && !missingMembers && !activeMember;
  }
  bool unknownMovement() const {
    return (!trackedMembers || missingMembers) && movementEvidence;
  }
};
template <typename Model, typename Group>
GroupReadiness readGroupReadiness(Model &model, const Group &group) {
  constexpr size_t capacity =
      sizeof(group.linkedShades) / sizeof(group.linkedShades[0]);
  static_assert(capacity <= 32, "Bounded physical group membership");
  GroupReadiness out;
  out.movementEvidence =
      group.direction != 0 ||
      (group.lastFrame.valid && startsMovement(group.lastFrame.cmd));
  for (const auto id : group.linkedShades) {
    if (!id || id == 255)
      continue;
    auto *shade = model.getShadeById(id);
    if (!shade) {
      out.missingMembers = true;
      continue;
    }
    out.trackedMembers = true;
    if (!shade->idleForAdministration())
      out.activeMember = true;
  }
  return out;
}
