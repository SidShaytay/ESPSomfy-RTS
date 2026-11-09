#pragma once
#include <cstdint>
enum class BootRecoveryNotice:uint8_t {none,movement,work,storage,scheduled,attention};
inline const char *bootRecoveryText(BootRecoveryNotice value){
  switch(value){
  case BootRecoveryNotice::none:return "";
  case BootRecoveryNotice::movement:return "Rollback waiting for shutters";
  case BootRecoveryNotice::work:return "Rollback waiting for work";
  case BootRecoveryNotice::storage:return "Rollback waiting for saved state";
  case BootRecoveryNotice::scheduled:return "Rollback scheduled";
  case BootRecoveryNotice::attention:return "Rollback needs attention";
  }
  return "Rollback needs attention";
}
// Loop-owned automatic policy; all persistence, reservation, final seals and
// reboot remain in SafeRestart. Notice is a cached, read-only atomic projection.
void gitUpdateRecoveryLoop(bool required,uint64_t now);
BootRecoveryNotice gitUpdateRecoveryNotice();
