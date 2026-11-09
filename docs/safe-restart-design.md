# Safe restart runtime

`SafeRestart` now owns every production restart request and both loop shutdown checks. It replaces the legacy 32-bit timer and source record. Settings completion, explicit protocol Restart, model recovery, restore completion and manual firmware completion all use this service. This wiring does not activate the GitHub worker, an installer or a new product workflow.

## Requests and completion

`requestRestart()` is loop-only: it takes a fresh observation, reserves the existing `GitUpdateClass` restart owner, saves the current model if dirty, and reports actual admission. Its owned counterpart requires the exact completed manual/restore source; an expired or mismatched source never becomes a generic restart. Automatic owned completion refuses outstanding model recovery rather than silently retrying an earlier failed save.

`submitRestart()` and `submitOwnedRestart()` are cached-only asynchronous mailboxes. A queued result is not admission. The maximum 60-second mailbox age includes the age of the last loop-published observation, so an old cache may require an explicit fresh retry sooner. Callbacks do not read model/settings state, wait for the loop or perform restart persistence. Transfer callbacks finish their exact I/O claim before publishing the source. Cancellation before handoff releases only an exact ready source with no claim or resource; stale/expired sources stay under the existing cleanup lifecycle.

A single synchronized record protects source, generation, cached status, coordinator and subordinate storage lease. No model query, owner call, serialization, SDK call, cleanup or shutdown runs under that lock. A new request cannot replace a cancelled intent while its old I/O still owns its lease. `restartPending()` includes the mailbox and admitted delay, and local settings/entity/control admission uses that cached result.

## Histories, clock and queued work

Each intent separately binds settings uncertainty history, model failure history, model configuration revision, accepted mutation history and a non-reused generation. The actual `SomfyShadeController::commit()` records every outcome before returning, including automatic retries. Failure/uncertainty permanently cancels older intent; successful recovery does not rewind history or schedule another restart. Only a fresh explicit request can try again.

Time comes from the 64-bit monotonic boot clock, not `millis()`. Delays are 1–60,000 milliseconds and begin after admission. Backward time, exhausted histories, cancellation, changed revisions or failed persistence terminate the intent. The final `take()` is the shutdown linearization point; cancellation after that terminal decision cannot undo shutdown.

Only successfully accepted mutating/RF queue work advances operation history: shade requests, local controls, model edits/commits, restore, settings writes and explicit Wi-Fi/RF scans. Wi-Fi scan can disconnect the station, so it counts as an operation. Protocol reads, getSettings and duplicate Restart do not advance history. Unknown future operations count conservatively. Queue markers remain at most 80 bytes.

Any pending queue blocks initial admission. Once admitted, ordinary reads and duplicate Restart can drain without cancelling the intent. If due while those replies remain queued, normal loop draining runs first. A maximum five-second overdue wait prevents a permanent restart owner under continuous polling; exceeding it cancels with `queuedOperations`. Accepted mutations cancel even if already drained before the next observation. Duplicate Restart reports pending before taking a new observation.

## Final shutdown and storage

The loop-only `requestCheckedRestart()` adds a source-specific, read-only check
at admission and final persistence. Its final check knows whether this intent
holds both final seals. Automatic boot recovery uses it to recheck pending OTA
state, a valid previous image and RX/partial-frame readiness after saving. The
callback runs outside both runtime and updater locks. The same coordinator and
owner still perform the restart; a rejected source never bypasses them.

The same non-expiring `GitUpdateClass` owner remains held across the delay, final model save and shutdown. `StorageLeaseGate` supplies subordinate I/O exclusion; it is not another updater owner, authorization system or installer. Each coordinator I/O phase reserves a generation-bound lease and keeps it until its own completion/cleanup.

Before shutdown, the service checks settings/counter uncertainty, queue/radio work, unresolved shade behaviour, GPIO pulses and untracked group movement. It seals the actual queue against in-flight producers and the radio against accepted TX/outcomes, writes the current model through checked persistence, and rechecks readiness and all histories. It never clears motion, sends STOP, flushes accepted work or replays edits/counters/RF. Refusal reopens acquired seals and releases the exact owner/lease. No later successful automatic save revives that intent. Successful final admission retains both seals and owner through `net.end()` and `ESP.restart()`.

Manual upload's cached precheck is conservative and does not certify full image/profile/digest admission. Restore still needs checked complete persistence/staging outcomes. Canonical shared firmware bundles web assets and retires its raw web-image upload; generic filesystem replacement and its old mounted-filesystem recovery assumptions remain separately unqualified. There is no claim that this service solves generic raw-filesystem replacement, arbitrary concurrent storage writers, power-loss transactions or physical recovery.

## Offline verification

### Installer handoff draft

`requestInstallRestart()` accepts a completed firmware transfer plus its exact
retained radio pause and already-sealed producer queue. The operation supplies
loop-only freshness, activation and failure callbacks. Missing callbacks, a
stale operation, an unfinished I/O claim or a missing seal refuse before taking
the intent. Admission and final persistence recheck the transfer, both seals
and freshness. The existing coordinator still owns the delay, history checks,
checked model commit and final noncancellable decision.

The original transfer owner stays held until that final decision. Only then
does it become a non-expiring restart owner and invoke the activation callback,
outside locks and with both seals retained. An unverified activation result
reports `activationFailed`, retains owner/seals, and cannot automatically retry,
reopen controls or reboot. Refusal before activation leaves cleanup to the
original installer; SafeRestart never reopens a seal it received from it.

This is an inactive internal API. No production caller supplies a slot-selection
callback yet; startup, controller preparation, LCD integration and boot health
still need completion. Host tests use a recorded callback rather than an OTA
write or real reboot.

- `bash tools/test-safe-restart.sh`: actual service/coordinator and shared owner with fake clock/model/shutdown; cancellation and stale source, 64-bit wrap/rollback, separate histories, failed file then recovery, read draining/flood timeout, final queue/radio barriers and no I/O under the runtime lock.
- `bash tools/test-application-upload.sh`: actual transfer and queue consumers with the actual restart service; claim retirement, expiry, late callbacks, restore FIFO and source handoff.
- `bash tools/test-group-restart.sh`: actual group dispatcher/member behaviour, including completed group travel and unknown members.
- Native policy/coordinator, mutation classification, actual commit hook, settings completion and backend tests; actual UI/queue/authorization/capture host harnesses; shared and generic SDK builds.

Tests use local/fake boundaries and compilation only. No controller, HA, RF, motor or physical restart test is implied.
