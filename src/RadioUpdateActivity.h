#pragma once
#include <cstdint>
// Read under radio -> scheduler -> receive locks. Unknown/busy snapshots never
// grant pause permission, and no accepted frame/job is cleared for an update.
struct RadioUpdateActivity {
  bool observed=false,ready=false,schedulerIdle=false,suspended=false,txSealed=false,
       counterBusy=false,receivingFrame=false,scanning=false,receiverPaused=false;
  uint8_t receivedFrames=0,repeaterFrames=0;
  bool canPause() const {
    return observed && ready && schedulerIdle && !suspended && !txSealed && !counterBusy &&
        !receivingFrame && !scanning && !receiverPaused && !receivedFrames && !repeaterFrames;
  }
};
struct RadioInstallToken {uint64_t generation=0;};
enum class RadioInstallResume : uint8_t { resumed,unavailable,busy,stale };
