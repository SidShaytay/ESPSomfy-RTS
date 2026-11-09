#if defined(ARDUINO) && defined(BOOT_RECOVERY_HOST_TEST)
#error "Boot recovery host hooks are forbidden in firmware"
#endif
#include "GitUpdateRecovery.h"
#if defined(ESPSOMFY_LILYGO_T_EMBED_CC1101) || defined(BOOT_RECOVERY_HOST_TEST)
#include "SafeRestart.h"
#include "GitUpdate.h"
#include "ModelIdleForAdministration.h"
#include "LocalControlQueue.h"
#include <atomic>
#ifndef BOOT_RECOVERY_HOST_TEST
#include "Somfy.h"
#include "LoopQueue.h"
#include "GitUpdateBootloader.h"
#include <esp_ota_ops.h>
extern SomfyShadeController somfy;
extern LoopQueue loopQueue;
#endif
namespace BootRecoveryDetail {
std::atomic<BootRecoveryNotice> notice{BootRecoveryNotice::none};
uint64_t requested=0,retryAt=0,lastClock=0;
bool active=false,unavailable=false;
bool rollbackPossible(){
  if(!gitUpdateBootloaderMatches())return false;
  const auto *running=esp_ota_get_running_partition();esp_ota_img_states_t state;
  return running && esp_ota_get_state_partition(running,&state)==ESP_OK && state==ESP_OTA_IMG_PENDING_VERIFY && esp_ota_check_rollback_is_possible();
}
bool sourceCurrent(bool sealed){
  if(!active || !rollbackPossible())return false;
  RadioUpdateActivity activity;
  if(!somfy.transceiver.updateActivity(activity) || activity.txSealed!=sealed)return false;
  // Only this source's final TX seal may be ignored. RX queues, partial
  // frames, counter edits, scans and every foreign pause remain blockers.
  activity.txSealed=false;return activity.canPause();
}
}
BootRecoveryNotice gitUpdateRecoveryNotice(){return BootRecoveryDetail::notice.load(std::memory_order_relaxed);}
void gitUpdateRecoveryLoop(bool required,uint64_t now){
  using namespace BootRecoveryDetail;
  active=required;
  if(!required){
    if(requested){const auto status=restartStatus();if(status.generation==requested && status.pending())cancelRestart();}
    requested=retryAt=0;unavailable=false;lastClock=now;notice=BootRecoveryNotice::none;return;
  }
  if(now==UINT64_MAX || now<lastClock)unavailable=true;
  lastClock=now;
  if(requested){
    const auto status=restartStatus();
    if(status.generation==requested && status.pending()){notice=BootRecoveryNotice::scheduled;return;}
    requested=0;retryAt=now>UINT64_MAX-250?UINT64_MAX:now+250;
  }
  if(unavailable){notice=BootRecoveryNotice::attention;return;}
  if(!modelIdleForAdministration(somfy)){notice=BootRecoveryNotice::movement;return;}
  RadioUpdateActivity activity;
  if(loopQueue.pending() || LocalUi::localControlPending() || gitUpdate.busy() || restartPending() ||
      !somfy.transceiver.updateActivity(activity) || !activity.canPause()){notice=BootRecoveryNotice::work;return;}
  if(restartTransferBlocked()){notice=BootRecoveryNotice::storage;return;}
  if(now<retryAt)return;
  retryAt=now>UINT64_MAX-250?UINT64_MAX:now+250;
  if(!rollbackPossible()){unavailable=true;notice=BootRecoveryNotice::attention;return;}
  const auto result=requestCheckedRestart(sourceCurrent,3000);
  if(result.admitted){
    const auto status=restartStatus();
    if(status.generation && status.pending()){requested=status.generation;notice=BootRecoveryNotice::scheduled;return;}
  }
  notice=BootRecoveryNotice::work;
  // A refused/cancelled intent is finished. A later idle observation creates a
  // fresh checked intent; failed model persistence blocks via the same ledger.
}
#else
void gitUpdateRecoveryLoop(bool,uint64_t){}
BootRecoveryNotice gitUpdateRecoveryNotice(){return BootRecoveryNotice::none;}
#endif
