#pragma once
#include "GroupReadiness.h"
// Loop-owned read-only model proof. No trust, queue, radio or ownership grant:
// those belong to the actual administration boundary. This also permits a
// quarantined-but-idle model to enter explicit recovery.
template<typename Model>
bool modelIdleForAdministration(Model &model) {
  for(auto &shade:model.shades)
    if(shade.getShadeId()!=255 && !shade.idleForAdministration())return false;
  for(auto &group:model.groups) {
    if(group.getGroupId()==255)continue;
    const auto readiness=readGroupReadiness(model,group);
    if(readiness.activeMember || readiness.unknownMovement())return false;
  }
  return true;
}
