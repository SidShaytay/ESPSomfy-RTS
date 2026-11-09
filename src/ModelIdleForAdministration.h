#pragma once
#include "GroupReadiness.h"
// Loop-owned read-only model proof. No trust, queue, radio or ownership grant:
// those belong to the actual administration boundary. This also permits a
// quarantined-but-idle model to enter explicit recovery.
// Partial proof for metadata-only operations: untracked group history grants
// no physical readiness, but must not make malformed metadata unrepairable.
// The actual boundary separately proves no accepted queue/radio work and owns
// configuration. RF/update operations still require the strict proof below.
template<typename Model>
bool configuredShadesIdleForAdministration(Model &model) {
  for(auto &shade:model.shades)
    if(shade.getShadeId()!=255 && !shade.idleForAdministration())return false;
  return true;
}
template<typename Model>
bool modelIdleForAdministration(Model &model) {
  if(!configuredShadesIdleForAdministration(model))return false;
  for(auto &group:model.groups) {
    if(group.getGroupId()==255)continue;
    const auto readiness=readGroupReadiness(model,group);
    if(readiness.activeMember || readiness.unknownMovement())return false;
  }
  return true;
}
