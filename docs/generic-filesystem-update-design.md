# Generic filesystem update: admission and recovery

Generic builds use the browser-only prepared flow described in
[the user guide](generic-filesystem-update.md). Canonical LILYGO firmware bundles
web assets and refuses separate Application images with HTTP 410. This change
does not introduce a browser/LCD approval workflow.

The existing GitUpdate manual owner and ManualUpdateLifecycle govern metadata
preparation, exclusive receive, cleanup and restart handoff. Preparation binds a
nonce, security revision and exact full-partition byte count. Initial preparation
uses the existing model-commit FIFO at normal priority, behind accepted work;
no request pointer or image is copied into its 80-byte markers. Metadata requests
have no upload-disconnect hook. Only an exclusively adopted file request installs
that hook. A failed adoption cannot retire the request that actually owns receive.

The loop's bounded ApplicationFilesystem resource record performs these steps:

1. Check model trust, security history, idle work and the exact mounted `spiffs`
   partition. Pinned Arduino Update ignores the label for U_SPIFFS/U_LITTLEFS and
   selects the first partition of the subtype. Prove the SDK target's address,
   size, label and subtype match the mounted partition and exact image size.
2. Verify a current-model save. Briefly seal empty FIFO admission while publishing
   the existing radio pause and the mount-I/O seal, then reopen FIFO admission.
   Independent wired controls remain usable. RF requests admitted during this
   install pause retain refusal even if drained after resume; they cannot execute
   later. Legacy frequency-scan accepted-work behavior is preserved separately.
3. Wait for tracked handles to drain. MountedFilesystemGate leases cover checked
   model persistence and read-only static assets. Shared FileImpl aliases retain
   their lease until underlying close; concurrent close waits for active reads,
   and closed aliases cannot issue late I/O. Backup responses are RAM-backed.
4. Call LittleFS.end() and verify esp_littlefs_mounted("spiffs") is false. The SDK
   end() returns void and may fail, so its call alone never admits raw writes.
5. Permit the exact prepared source to receive one file. Recheck authentication,
   source and expiry on every chunk; enforce offsets and byte count, reject a
   second file, and use Update.end(false). The legacy image receiver is
   firmware-only; it cannot accept filesystem images.
6. Complete the authenticated whole HTTP request before requesting recovery or
   restart. File-final alone grants neither a restart nor a recovery retry that
   can reboot. Only exact-source loop cleanup aborts an SDK object it owns.
7. Remount without formatting, verify the mount, restore the exact radio token
   and save CURRENT RAM through the checked writer. Never load uploaded counters
   or model configuration. Preserve dirty RAM while storage is unavailable.
8. After verified recovery, try safe restart once while authority is fresh.
   Expiry, refused restart or a recovery fault removes that automatic permission.
   Explicit recovery retry grants no receive or restart permission.

Cleanup runs before generic manual cleanup and retains the same operation claim
while mount/radio resources remain unavailable. No owner lock spans file, SDK,
radio or serialization I/O. A busy radio restoration defers without sealing the
FIFO; failed mount recovery still releases the radio where possible. A failed
mount/save retains exact-source attention status for explicit retry. Canonical
bundled builds compile inert filesystem-preparation functions.

The host harnesses exercise actual owner/driver/receiver/HTTP code with fake I/O,
the actual FIFO and local-control adapter, and pinned Arduino File/FS aliases.
Native tests cover lifecycle and FIFO rules; browser tests cover readiness,
identity validation, cancellation, status loss, explicit recovery and no replay.
Both firmware environments compile. These are offline checks, not hardware or
power-loss qualification. No power-loss transaction guarantee is established.


The entry-point audit found startup-only directory listing and app-version/model
reads before HTTP handlers start; unused legacy settings-file loaders have no
runtime callers. Restore temporary-file receive, validation and apply retain
the mutually exclusive restore owner, so they cannot overlap application
preparation. Backup output is constructed in RAM. The checked model commit and
generic static-file wrapper are the remaining live mounted-I/O paths, and both
use the shared gate.
