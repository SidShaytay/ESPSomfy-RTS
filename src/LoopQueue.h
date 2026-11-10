#ifndef LOOP_QUEUE_H
#define LOOP_QUEUE_H
#include <stdint.h>
#include <atomic>
#include <new>
#include "ShadeRequests.h"
#include "GitUpdateOwner.h"
#include "ProtoFrame.h"
#include "ProtoSchema.h"
#include "FrequencyScanToken.h"

// The one queue through which other tasks hand work to loop(), which alone
// changes the shade model (docs/adr/0002-loop-task-is-the-only-writer-of-the-shade-model.md).
// The items and the hold rule are kept free of FreeRTOS so they can be unit
// tested on the PC (pio test -e native).

// Big enough for Home Assistant firing a command at every shade at once.
#define LOOP_QUEUE_SIZE 32

// What a restore_shades item restores, as bits of restoreParts.
#define RESTORE_SETTINGS    0x01
#define RESTORE_SHADES      0x02
#define RESTORE_NETWORK     0x04
#define RESTORE_TRANSCEIVER 0x08
#define RESTORE_REPEATERS   0x10
#define RESTORE_MQTT        0x20

enum class loop_item_type_t : uint8_t {
  shade_request,   // run by ShadeRequests
  restore_shades,  // restore from /shades.tmp, then reboot
  model_edit,      // apply an edit with ModelEditor
  model_commit,    // persist current model only; no repeated edit/RF operation
  proto_read,      // answer a protocol 2 read op from the shade model
  proto_device,    // run a protocol 2 settings op or the WiFi scan (DeviceOps)
  local_control,  // loop-owned LCD intent token, freshly revalidated on drain
  local_advanced, // bounded loop-owned learned-link/file-recovery intent
  firmware_install // complete staged file; exact owner, never request/buffer pointers
};
// Any-task atomic driver observation; no model/SPI access.
bool radioInstallPaused();

struct model_edit_t;
struct settings_save_t;
struct loop_item_t {
  // Payloads are mutually exclusive. Explicit activation preserves C++ union
  // lifetime; the FIFO copies a trivially copyable marker, never model data.
  union { shade_request_t request{}; FrequencyScanToken scanToken; };
  loop_item_type_t type = loop_item_type_t::shade_request;
  bool restartAfterCommit = false;
  uint8_t restoreParts = 0;
  uint8_t op = 0,groupId = 0,section = 0;
  bool radioScanAtAdmission = false,radioPauseAtAdmission = false,localRadio = false;
  model_edit_t *edit = nullptr;
  proto_reply_to_t replyTo;
  settings_save_t *save = nullptr;
  uint64_t localToken = 0;
  GitUpdateOwnerToken externalOwner;
  void setScanToken(FrequencyScanToken value){new(&scanToken) FrequencyScanToken(value);}

};
static_assert(sizeof(loop_item_t)<=80,"Keep model contexts out of the 32 queue markers");

// Accepted writes/RF invalidate an older restart even if drained before the next
// observation. Read-only polling still receives its reply and does not change
// that history. Unknown future operations conservatively count as mutations.
inline bool loopItemMutates(const loop_item_t &item) {
  if(item.type==loop_item_type_t::proto_read) return false;
  if(item.type==loop_item_type_t::proto_device)
    return item.op!=uint8_t(proto_type_t::getSettings) && item.op!=uint8_t(proto_type_t::getFrequencyScanResult) && item.op!=uint8_t(proto_type_t::reboot);
  return true;
}
// Whether the item sends anything on air.
inline bool loopItemUsesRadio(const loop_item_t &item) {
  if(item.type == loop_item_type_t::local_control || item.type == loop_item_type_t::local_advanced) return item.localToken && item.localRadio;
  return item.type == loop_item_type_t::shade_request && shadeRequestUsesRadio(item.request.type);
}
// Whether the item at the head of the queue waits for the radio.  An HTTP or
// MQTT client was already told it was accepted, so it stays queued until the
// radio has room rather than have the radio drop it; a protocol 2 client gets
// its reply once it has run.  While TX is suspended nothing gets through
// anyway so nothing is held back then, except an unpair, which waits until TX
// resumes rather than leave the shade paired.  Everything queued
// behind it waits too, protocol 2 replies included, so a frequency scan
// started after an unpair delays them until it ends. Install pauses instead
// reject RF before this hold rule; GPIO remains independent.
inline bool holdLoopItem(const loop_item_t &item, bool txSuspended, bool txHasRoom) {
  if(txSuspended) return item.type == loop_item_type_t::shade_request && item.request.type == shade_request_type_t::unpair;
  return loopItemUsesRadio(item) && !txHasRoom;
}

class LoopQueue {
  public:
    void begin();
    // Hands the item to loop() without waiting, so it is safe from any task,
    // including async_tcp.  False when the queue is full.  first puts it
    // ahead of everything waiting: an item that ends a frequency scan, which
    // an unpair at the head waits for (holdLoopItem).
    bool submit(const loop_item_t &item, bool first = false);
    // Task-safe observed pending count; the currently popped item is excluded.
    uint32_t pending() const;
    // Runs the waiting items in order.  Only from loop().
    void drain();
    // Loop-only final shutdown barrier. Never discards accepted items or waits
    // for a producer. A refused shutdown must explicitly reopen admission.
    bool sealForRestart();
    bool restartSealHeld() const;
    void reopenAfterRefusedRestart();
    // The one direct socket RF publisher shares this producer barrier. Caller
    // must pair an admitted begin with end on every return, outside any lock.
    bool beginSocketMutation(uint32_t clientId);
    void endSocketMutation(bool accepted);
  private:
    void *queue = nullptr;
    std::atomic<bool> restartSealed{false};
    std::atomic<uint32_t> producers{0};
};
#endif
