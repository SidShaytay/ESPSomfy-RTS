#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <LittleFS.h>
#include "esp_log.h"
#include "LoopQueue.h"
#include "ConfigFile.h"
#include "Somfy.h"
#include "SomfyEditor.h"
#include "Utils.h"
#include "outputs/ProtoSocket.h"
#include "DeviceOps.h"
#ifdef ESPSOMFY_LOCAL_UI
#include "LocalControlQueue.h"
#endif
#include "ModelCommitService.h"
#include "LegacyRestartSource.h"
#include <esp_timer.h>

static const char *TAG = "LoopQueue";

extern SomfyShadeController somfy;
extern ShadeRequests shadeRequests;
extern rebootDelay_t rebootDelay;

static restore_options_t restoreOptions(uint8_t parts) {
  restore_options_t opts;
  opts.settings = parts & RESTORE_SETTINGS;
  opts.shades = parts & RESTORE_SHADES;
  opts.network = parts & RESTORE_NETWORK;
  opts.transceiver = parts & RESTORE_TRANSCEIVER;
  opts.repeaters = parts & RESTORE_REPEATERS;
  opts.mqtt = parts & RESTORE_MQTT;
  return opts;
}

void LoopQueue::begin() {
  if(!this->queue) this->queue = xQueueCreate(LOOP_QUEUE_SIZE, sizeof(loop_item_t));
}
bool LoopQueue::submit(const loop_item_t &item, bool first) {
  if(!this->queue || restartSealed.load()) return false;
  producers.fetch_add(1);
  // A producer observed before sealing keeps it from completing; a producer
  // arriving after sealing cannot enqueue. No mutex spans an RTOS queue call.
  if(restartSealed.load()) {producers.fetch_sub(1);return false;}
  // async_tcp also services the sockets, so it must never wait on loop() or
  // the radio.
  QueueHandle_t q = static_cast<QueueHandle_t>(this->queue);
  const bool accepted=(first ? xQueueSendToFront(q, &item, 0) : xQueueSend(q, &item, 0)) == pdTRUE;
  producers.fetch_sub(1);
  if(!accepted) {
    ESP_LOGE(TAG, "Loop queue full, dropping item");
    return false;
  }
  return true;
}
bool LoopQueue::sealForRestart() {
  bool expected=false;
  if(!restartSealed.compare_exchange_strong(expected,true)) return false;
  if(producers.load() || pending()) {restartSealed.store(false);return false;}
  return true;
}
void LoopQueue::reopenAfterRefusedRestart() {restartSealed.store(false);}
uint32_t LoopQueue::pending() const {
  return this->queue ? uxQueueMessagesWaiting(static_cast<QueueHandle_t>(this->queue)) : 0;
}
void LoopQueue::drain() {
  if(!this->queue) return;
  QueueHandle_t q = static_cast<QueueHandle_t>(this->queue);
#ifdef ESPSOMFY_LOCAL_UI
  // Observe the single slot even behind a held head: idle/new motion must not
  // reacquire STOP eligibility while other clients wait for the radio.
  LocalUi::refreshLocalControl();
#endif
  loop_item_t item;
  // Items run in order, so only the one at the head can be held back.
  while(xQueuePeek(q, &item, 0) == pdTRUE) {
#ifdef ESPSOMFY_LOCAL_UI
    if(item.type == loop_item_type_t::local_control) {
      if(LocalUi::localControlTokenActive(item.localToken)) LocalUi::refreshLocalControl();
      if(!LocalUi::localControlTokenActive(item.localToken)) {
        xQueueReceive(q, &item, 0); // cancelled/expired markers never hold FIFO
        continue;
      }
    }
#endif
    if(holdLoopItem(item, somfy.transceiver.txSuspended(), somfy.transceiver.txHasRoom())) break;
    // loop() is the only consumer so this is the item we peeked.
    xQueueReceive(q, &item, 0);
    switch(item.type) {
      case loop_item_type_t::local_control:
#ifdef ESPSOMFY_LOCAL_UI
        LocalUi::runLocalControl(item.localToken);
#endif
        break;
      case loop_item_type_t::shade_request: {
        const shade_request_result_t result = shadeRequests.run(item.request);
        if(result == shade_request_result_t::radio_refused)
          ESP_LOGW(TAG, "Radio didn't take shade request %u", static_cast<uint8_t>(item.request.type));
        protoSocket.replyRequest(item.replyTo, item.request, result);
        break;
      }
      case loop_item_type_t::restore_shades: {
        const auto &owner=item.externalOwner;
        if(!gitUpdate.claimTransfer(owner,uint64_t(esp_timer_get_time())/1000,true)) {
          ESP_LOGW(TAG,"Restore source expired or is no longer owner; nothing applied or removed");break;
        }
        restore_options_t opts=restoreOptions(item.restoreParts);
        const bool restored=ShadeConfigFile::restore(&somfy,"/shades.tmp",opts,owner);
        // Only this claimed source may remove its temporary file. Stale
        // markers never delete a newer request's backup at the same path.
        LittleFS.remove("/shades.tmp");gitUpdate.transferResource(owner,false);
        const auto time=uint64_t(esp_timer_get_time())/1000;
        if(restored && gitUpdate.transferContinuing(owner,time) && gitUpdate.holdTransferRestart(owner)) {
          const auto published=publishExternalLegacyRestart(rebootDelay,millis()+1000,owner.generation,uint8_t(owner.kind));
          if(published.admitted) {gitUpdate.finishTransfer(owner,time,true);break;}
          ESP_LOGW(TAG,"Restore applied; restart not admitted");
        } else ESP_LOGW(TAG,"Restore refused, failed or expired; no restart scheduled");
        if(gitUpdate.closeTransfer(owner)) gitUpdate.releaseExternal(owner);
        break;
      }
      case loop_item_type_t::model_commit:
        runModelCommit(item);
        break;
      case loop_item_type_t::model_edit:
        runModelEdit(item.edit);
        break;
      case loop_item_type_t::proto_read:
        protoSocket.replyRead(item.replyTo, item.op, item.groupId);
        break;
      case loop_item_type_t::proto_device:
        runDeviceOp(item);
        break;
    }
  }
}
