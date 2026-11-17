#pragma once
#include "GitUpdate.h"
// One plain-language line per updater outcome, shared by the LCD and the web UI.
// Kept short enough for the LCD.
inline const char *gitUpdateAdvice(GitUpdateReason reason) {
  switch(reason){
  case GitUpdateReason::none:return "Firmware and Web update together. No automatic checks.";
  case GitUpdateReason::development:return "Development build: stable updates unavailable.";
  case GitUpdateReason::clock:return "Clock is unset. Connect to the network and try again.";
  case GitUpdateReason::certificateTime:return "HTTPS certificate date refused. Check the device clock.";
  case GitUpdateReason::memory:return "Not enough update memory. No automatic retry.";
  case GitUpdateReason::cancelled:return "Update cancelled; controls resume after cleanup.";
  case GitUpdateReason::timeout:return "Update timed out; controls resume after cleanup.";
  case GitUpdateReason::candidateChanged:case GitUpdateReason::invalidCandidate:return "Release changed. Check and review it again.";
  case GitUpdateReason::activationFailed:return "Boot selection uncertain. Controls paused. Recovery required.";
  case GitUpdateReason::writeFailed:return "Firmware write failed. Wait for cleanup before retrying.";
  case GitUpdateReason::admission:return "Controller busy or state changed. No automatic retry.";
  case GitUpdateReason::bootloader:case GitUpdateReason::assets:case GitUpdateReason::layout:return "Installed image is not qualified for GitHub updates.";
  case GitUpdateReason::transport:return "Verified download failed. Check the network and try again.";
  case GitUpdateReason::digest:case GitUpdateReason::invalidImage:return "Release verification failed. Update refused.";
  case GitUpdateReason::noNewerRelease:return "This firmware is current. No newer stable release.";
  default:return "Update unavailable. Review network, clock and compatibility.";
  }
}
