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

Remaining implementation investigation: track asynchronous static/backup readers
and the model-save entry points; establish pinned driver behavior for open files;
choose the smallest bounded browser admission handshake that preserves the SAME
manual owner and does not create a second upload broker. Tests must inject reader
races, failed unmount/remount, short writes, expiry, old callbacks and failed
current-model recovery. No power-loss transaction guarantee is established.
