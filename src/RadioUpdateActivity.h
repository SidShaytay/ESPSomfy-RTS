#pragma once
#include <cstdint>
// Read under radio -> scheduler -> receive locks. Unknown/busy snapshots never
// grant pause permission, and no accepted frame/job is cleared for an update.
struct RadioUpdateActivity {
  bool observed=false,ready=false,schedulerIdle=false,suspended=false,txSealed=false,
       counterBusy=false,receivingFrame=false,scanning=false,receiverPaused=false;
  uint8_t receivedFrames=0,repeaterFrames=0;
  // Configuration repair needs idle resources, even with disabled or
  // unavailable radio hardware. Installer pause still requires ready below.
  bool quiescent() const {
    return observed && schedulerIdle && !suspended && !txSealed && !counterBusy &&
        !receivingFrame && !scanning && !receiverPaused && !receivedFrames && !repeaterFrames;
  }
  bool canPause() const { return ready && quiescent(); }
};
struct RadioInstallToken {uint64_t generation=0;};
enum class RadioInstallResume : uint8_t { resumed,unavailable,busy,stale };
