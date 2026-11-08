#pragma once
#include <stdint.h>
// I/O exclusion only. This does not grant updater ownership or authorization.
// One runtime mutex must protect this gate and the restart coordinator together.
enum class StorageUse : uint8_t { modelWrite, restart, exclusiveUpdate };
struct StorageLease {
  uint64_t generation=0;
  StorageUse use=StorageUse::modelWrite;
};
class StorageLeaseGate {
  uint64_t generation=0;
  bool active=false;
  StorageUse use=StorageUse::modelWrite;
public:
  StorageLeaseGate()=default;
  StorageLeaseGate(const StorageLeaseGate &)=delete;
  StorageLeaseGate &operator=(const StorageLeaseGate &)=delete;
  bool busy() const { return active; }
  bool owns(const StorageLease &token) const {
    return active && token.generation && token.generation==generation && token.use==use;
  }
  bool acquire(StorageUse requested,StorageLease &token) {
    if(active || generation==UINT64_MAX || static_cast<uint8_t>(requested)>static_cast<uint8_t>(StorageUse::exclusiveUpdate)) return false;
    active=true;use=requested;token={++generation,use};return true;
  }
  bool release(const StorageLease &token) {
    if(!owns(token)) return false;
    active=false;return true;
  }
  bool permitsModelWrite(const StorageLease &token) const {
    return owns(token) && (token.use==StorageUse::modelWrite || token.use==StorageUse::restart);
  }
};
