#pragma once
#include "ManualUpdateLifecycle.h"
// The loop's bounded physical resource record beneath the existing transfer
// owner. Browser callbacks never mount, save, or hold a radio token.
enum class ApplicationFilesystemPhase : uint8_t {idle,queued,draining,ready,recovering,refused,attention};
enum class ApplicationFilesystemFault : uint8_t {none,source,busy,layout,save,unmount,mount,radio};
struct ApplicationFilesystemSnapshot {
  GitUpdateOwnerToken owner;
  ManualApplicationSource source;
  ApplicationFilesystemPhase phase=ApplicationFilesystemPhase::idle;
  ApplicationFilesystemFault fault=ApplicationFilesystemFault::none;
  int command=0;
};
static_assert(sizeof(ApplicationFilesystemSnapshot)<=64,"Bounded file preparation status");
// Any-task metadata publication before the existing FIFO marker is submitted.
bool noteApplicationPreparation(const GitUpdateOwnerToken &,const ManualApplicationSource &);
// Loop only: consume that exact FIFO marker and service drain/reducing cleanup.
void runApplicationPreparation(const GitUpdateOwnerToken &);
void serviceApplicationFilesystem();
ApplicationFilesystemSnapshot applicationFilesystemStatus();
// Authenticated route may request reducing cleanup of this exact failed source.
// Executes only on loop; grants no file write or restart.
bool requestApplicationFilesystemRecovery(const GitUpdateOwnerToken &,const ManualApplicationSource &);
// Read-only observation, never a receive claim. Caller must separately adopt
// the prepared source and authenticate the actual file request.
bool applicationFilesystemReady(const GitUpdateOwnerToken &,const ManualApplicationSource &,int &command);
