# Advanced configuration

Use **Home → Administration → Devices/Groups → Advanced** to select a configured
device or group. Open **Administration → More → Repeaters** for repeater entries.
These screens change configuration only; RF programming, learning, counter
correction and Favorite programming are separate workflows.

Advanced follows Reorder, preserving existing list entry positions. More is the
fourth paginated Administration entry. Select the target explicitly by its stable
ID; the ID appears before its name. All these screens are protected from capture.

You can:

- Change the paired marker, without claiming the motor paired or unpaired.
- Remove a linked remote association.
- Remove a repeater association, after already accepted repeater work completes.
- [Add or remove group controller entries](lcd-group-membership.md), including
  missing references, duplicates and unused placeholders. This leaves physical
  pairing unchanged.

Every operation opens a confirmation with Cancel selected. Turning the encoder,
entering a screen, waking, pressing Back or timing out never applies an edit.
Removing an association does not transmit a physical unpair command. The group controller-list editor has a separate review for present, missing and
unused entries; it does not perform physical programming.

If a change is applied but its model file cannot be verified, the action is
forgotten. The result offers a separate model-file retry and Close. Retrying
cannot apply the mutation again, send radio commands, or replay a counter write.
The backend must adopt the current identity/history for that retry and recheck
permission, context, queued work and restart admission. A busy retry keeps the
result screen and requires another explicit attempt.

A session allocates one object through the injected wiping PSRAM allocator. It
constructs that object with placement new, invokes its destructor before release,
and relies on the allocator to erase the allocation. The handle is at most 32
bytes; the session plus this allocator's aligned header is at most 2304 bytes.
The SDK's own heap metadata is additional. Session storage and transient edit JSON
share the existing 4096-byte allocator budget; there is no internal-heap fallback.
The parent supplies its existing three-row frame rather than retaining another
frame in the component. Allocation failure, expiry, clock reversal, cancellation,
authentication changes and configuration changes close and erase the session.

Device/group context includes the exact physical membership, identity and
configuration/restore/radio history. Repeater context is tagged separately and
contains all seven physical address slots plus model history. Repeater state is
never represented by an invented entity ID. The runtime backend must freshly
validate the chosen association and all applicable idle/owner admission rules
before applying a shared domain operation.

Native and actual encoder/Runtime tests cover Cancel, file-only retry, busy retry, allocation
failure, timeout, all 32 picker entries, stable-ID label validation and context or
authorization changes. Releases inspect every returned byte for zeroization.
The isolated model harness compiles the actual group edit branch and actual
Group list methods; it checks missing-member repair, retained members and checked
file outcomes without providing a radio endpoint.

The planned receive-only learning cache and RF sequencer are separate increments.
Their entry and stage transitions must never automatically transmit. Favorite
programming remains pending a decision about its existing automatic movement and
later My-command sequence.

The metadata backend uses the existing shared configuration owner. It refuses
stale or busy requests before acquiring that owner, then rechecks the exact
lease, permission/history/context, queue/restart state and applicable idle rules
after allocation. Missing members do not prevent repairing their own references;
present members must be idle. Repeater removal waits for the radio's complete
quiescent query without sealing, flushing or cancelling accepted work. Shared
`applyModelEdit` and `applyModelCommit` provide the ordinary checked persistence
and notification outcomes. Only the first changes metadata. Linked-remote lists
retain the current read-only rolling-code display; no counter correction API is
exposed by these screens.

A quarantined counter identity cannot be hidden by changing its paired marker or
removing that linked remote/repeater association. The refusal explains that counter
recovery is required. Unrelated metadata and missing-member repair remain available
under their normal admission rules. Saving or removing unrelated configuration
does not clear counter quarantine, and a saved model file does not certify NVS
counter recovery.

The integrated tests use the actual configuration-owner service with scripted
model/file boundaries. They cover unseen launchers and confirmations during SPI
contention, applied file failure and COMMIT-only retry, all physical list slots,
read-only rolling codes, missing member ID 88, radio-busy refusal, identity
quarantine, and graphics/sleep erasure. The runtime uses its unchanged 4096-byte
allocator cap with target-like JSON pools; this is an allocation-path check rather
than a physical heap benchmark.
