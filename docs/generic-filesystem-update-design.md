# Generic filesystem update: admission and recovery

This is an implementation design, not a completed or qualified upload path.
Canonical LILYGO firmware bundles its web assets and continues to refuse raw
Application images. Generic boards retain their separate filesystem workflow.

The current generic receiver starts Arduino Update and writes the filesystem
partition while LittleFS remains mounted. Its request-owner guard prevents
competing uploads but does not exclude model persistence or open static-file
responses. This must be corrected before claiming update safety.

The pinned Arduino `LittleFSFS::end()` returns void. It logs an unregister
failure and returns without clearing the mountpoint. Calling it is therefore
not proof of unmounting. The underlying SDK exposes `esp_littlefs_mounted()`;
admission must verify the actual unmounted state before any raw write. The
installed SDK header does not establish safe behavior for open response files.
Do not infer that from exclusive upload ownership.

The implementation must use the existing manual owner and lifecycle throughout:

1. Prepare on loop, after all accepted movement/work drains. Preserve current
   model and counters with the existing checked save. Prevent new filesystem
   readers and writers, and wait for existing response/file operations to close.
   Refuse before writing if those conditions cannot be proved.
2. Unmount and verify the SDK reports unmounted. Only then permit this exact
   source to begin receiving/writing its filesystem image. Bind the original
   authenticated security revision and retain bounded transfer expiry.
3. On completion, disconnect, short write or expiry, finish/abort only the exact
   owned SDK operation. Loop remounts without formatting, verifies the mount,
   and saves the CURRENT retained RAM model through the existing checked writer.
   Never reload counters/configuration from the uploaded filesystem image.
4. Permit restart only after verified current-state save and the existing final
   restart checks. Failed mount/save remains visibly refused or recovery-pending;
   never automatically format, erase state, restore stale counters or replay RF.

Reader tracking is now implemented for generic static assets through a read-only
FS view. Its shared FileImpl lease survives request/response aliases and releases
only after underlying close. Concurrent close waits for active reads; closed
aliases cannot issue later filesystem I/O. Actual pinned Arduino File/FS tests
cover aliases, destruction, missing files, write denial and concurrent closure.
The two native gate tests cover sealing, draining and cancelled preparation.
The gate uses ordinary project-style atomic operations in task context; the
pinned ESP32 toolchain does not promise lock-free atomics. No service lock is
held across file I/O.

The upload receiver does not seal this gate yet. The raw mounted-write bug is
still open until the prepare/unmount/recovery flow above is wired and tested.
Backup responses are RAM-backed, not persistent file handles.

The checked model writer now holds the same mount lease across staging,
replacement and final readback. Sealed admission returns filesystemBusy before
serialization or I/O; the existing model service retains dirty RAM state. A retry
after recovery serializes current counters, not a pre-upload snapshot. Protocol
and log messages distinguish this deferral without claiming an old disk file was
retained while its partition may have been replaced.

The existing manual lifecycle now has preparation/ready/adoption transitions
binding a nonce, security revision and exact image byte count. Ready adoption
is exclusive; stale sources and expired clocks cannot claim receiving. A distinct
filesystem-unavailable resource prevents owner release or restart even when no
SDK operation was ever acquired. Expired sources may enter reducing mount
recovery, but cannot regain receive/SDK claims. This metadata is bounded to80
bytes under the same updater lock; it is not a second transfer owner.

Remaining implementation: finish the live-filesystem entry-point audit and
choose the smallest bounded browser admission handshake that preserves the SAME
manual owner and does not create a second upload broker. Tests must inject reader
races, failed unmount/remount, short writes, expiry, old callbacks and failed
current-model recovery. No power-loss transaction guarantee is established.

Pinned updater selection must match the mounted partition. Arduino Update's
U_SPIFFS branch selects the first SPIFFS-subtype partition and ignores its label
argument; U_LITTLEFS similarly selects the first LittleFS subtype. Verify the
chosen partition is exactly the one mounted as spiffs, and reject a mismatch
before raw writes. Do not assume the begin() label selects it.

`ApplicationFilesystem` now implements the loop's bounded physical resource
record beneath that same owner. Initial preparation uses the existing model
commit FIFO at normal priority. It verifies the exact mounted `spiffs` partition
matches the pinned Update SDK's first partition of the selected subtype and
requires a full-partition image. It saves current RAM, briefly seals queue
admission while publishing the existing radio pause and filesystem seal, then
allows independent wired work while tracked file handles drain. Only verified
unmounting publishes ready status. The metadata request has no upload-disconnect
hook; only the later adopted file request may own that hook.

Reducing cleanup precedes generic upload cleanup. It never aborts a foreign SDK
object, remounts without formatting, restores the exact radio token, and saves
current RAM. Busy radio restoration is deferred without holding queue admission.
Mount/save faults retain the source for explicit recovery retry; retry cannot
receive another image or authorize a restart. A failed mount still releases the
radio where possible. Cached status and loop resources contain no request or
image pointers. Canonical bundled builds compile inert versions of this module.

Seventeen host scenarios exercise the actual implementation with the real
GitUpdate owner and fake I/O; the FIFO preparation regression additionally
checks normal ordering and no producer-side commit. Both firmware environments
compile. The browser preparation endpoints, exact-source file receiver and
successful-upload restart handoff are still to be connected and qualified;
these component checks do not make the existing generic upload route safe.


The prepared-source receiver is implemented separately from legacy firmware
upload handling. It binds each chunk to the exclusively adopted source and
current authentication, requires exact file size/offset, uses Update.end(false),
and rejects a second file. A failed adoption cannot retire another receiver.
Only authenticated whole-request completion transfers to reducing loop recovery;
file-final alone grants no reboot. After mounted proof and current-RAM save,
restart is attempted once through the existing safe restart service. Expiry,
refused restart or a recovery fault removes that automatic permission; explicit
cleanup retry cannot revive it. Thirty-three actual owner/driver/receiver host
scenarios cover these boundaries, including overlapping adoption and foreign SDK
preservation. Browser endpoints and UI choreography are still unconnected.
