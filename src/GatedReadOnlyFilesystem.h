#pragma once
#include <FS.h>
#include "MountedFilesystemGate.h"
// Used only by generic-board static web assets. The SDK File aliases retain
// one gated FileImpl, so response/request destruction cannot release it early.
class GatedReadOnlyFilesystem final:public fs::FS {
public:
  GatedReadOnlyFilesystem(fs::FS &source,MountedFilesystemGate &gate);
};
MountedFilesystemGate &mountedFilesystemGate();
