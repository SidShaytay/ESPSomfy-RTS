# Local device and group backend

This backend supports room, device and group scalar fields from the shared edit
metadata. Device/group navigation and field interaction remain the separate
Form integration; this backend alone does not add reachable screens. Pairing,
linked remotes, group membership changes and explicit counter edits have separate
checked operations and are not replayed by this backend.

The backend retains one bounded raw context for the prepared Form. It captures
all 32 physical group slots, including duplicates, missing members and unused
sentinels. Empty or unpaired groups and disabled radio remain administrable.
Motion and ordinary rolling-code advancement are excluded from identity history.
A deletion that was applied but failed model-file persistence adopts the missing
target as a tombstone; an explicit retry saves the current model file only.
Recreation, identity changes or membership changes invalidate that retry.

Existing values are read unchanged, including inherited frame size 0 and
malformed UTF-8 names. A missing name terminator causes a bounded read failure;
it is distinct from terminated malformed text, which the Form can explicitly
replace. New drafts offer the current radio's frame size, a free primary remote
identity and unassigned GPIOs. Save materializes the displayed applicable defaults
into the existing expanded request using its injected wiping allocator. If the
hidden device/group sort order would exceed its signed byte, Add refuses with a
Reorder message before any mutation; it never wraps or normalizes existing
orders. Existing
entities keep omitted fields unchanged. Inactive GPIO drafts are refused, rather
than discarded or silently ignored.

Before mutation, read-only validation checks shared scalar bounds, configured
room references, primary remote-identity collisions, projected tilt compatibility
and the production physical GPIO admission. A non-blind device with nonzero tilt
must explicitly change tilt to None; a name-only edit cannot silently normalize
it. Correctable failures return an owned canonical reason and field without
calling the mutating domain service or advancing model history.

A device must be idle with no pending GPIO release before configuration changes
or deletion. Group configuration checks every present physical member against
the same rule; missing entries remain recorded and repairable. No automatic retry
or stop command is issued. After validation/allocation the backend rereads local
authorization, history, exact context, pending queue and pending restart before
applying. Busy retains the draft for an explicit later attempt. File-only retry
does not require idle because it does not change live configuration.

The native `test_localentityruntime` suite includes production backend code with
an injected model and storage/service boundaries. It covers every scalar read,
new defaults, sparse room choices, legacy-value preservation, physical membership
changes, admission refusal, final authorization expiry, idle requirements,
allocator failure and allocator-preserving transfer, plus applied-delete failure
and tombstone retry. These are offline tests: no device, GPIO, RF or HA commands.
