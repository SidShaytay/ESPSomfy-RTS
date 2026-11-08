#pragma once
#include "LocalUi.h"
#include "LocalOperationAdmission.h"
#include "LocalControlQueue.h"
namespace LocalUi {
Model snapshotModel(bool running,const View &);
CatalogueSource &runtimeCatalogue();
// Called only after a complete control frame was presented. Context tokens are
// loop-owned and monotonic; a list row cannot manufacture a presented RF target.
void markControlPresented(EntityRef,uint64_t token,bool authorizedSelection,uint32_t sessionGeneration);
// Revoke all local display/focus/STOP proofs without reusing their nonces.
void invalidateControlPresentation();
DisplayPreferences loadPreferences();
bool savePreferences(const DisplayPreferences &);
DispatchResult dispatch(const Command &);
}
