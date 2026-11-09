# Advanced configuration

The Advanced component is not connected to production navigation yet. This
checkpoint supplies the bounded screen state and backend contract. It does not
implement radio commands, learning, counter changes or Favorite programming.

The approved first screen flow is Administration → More, with Advanced selectors
for configured devices and groups, and a repeater list. Existing entry positions
are preserved. Device/group selection uses stable IDs and cached physical slots,
with the ID shown before the name. Every screen is protected from capture.

The first operations change configuration only:

- Change the paired marker, without claiming the motor paired or unpaired.
- Remove a linked remote association.
- Remove a repeater association, after already accepted repeater work completes.
- Remove dangling group-member references. This cannot unpair a missing device
  from a physical motor. Duplicate references to the selected ID are removed
  together; other members remain.

Every operation opens a confirmation with Cancel selected. Turning the encoder,
entering a screen, waking, pressing Back or timing out never applies an edit.
Removing an association does not transmit a physical unpair command. Present
members in the initial group screen are read only; their physical programming
workflow is a later increment.

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

Native component tests cover Cancel, file-only retry, busy retry, allocation
failure, timeout, all 32 picker entries, stable-ID label validation and context or
authorization changes. Releases inspect every returned byte for zeroization.
The isolated model harness compiles the actual group edit branch and actual
Group list methods; it checks missing-member repair, retained members and checked
file outcomes without providing a radio endpoint.

The planned receive-only learning cache and RF sequencer are separate increments.
Their entry and stage transitions must never automatically transmit. Favorite
programming remains pending a decision about its existing automatic movement and
later My-command sequence.
