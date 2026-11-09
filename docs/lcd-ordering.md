# Local list ordering

The ordering component supports complete room, device and group lists. Parent
navigation is a separate integration: this component alone does not add a
reachable Reorder menu.

Select an entry by its stable ID, press to pick it, turn to move it, then press
to apply that move to the local draft. Back while moving restores the draft
before that pick, preserving earlier applied draft moves. Nothing has been
saved yet. Choose Save order and explicitly confirm; Cancel is selected first.
Back from the list discards its entire draft. Empty and already unchanged,
compact lists close without a save.

Save renumbers the displayed complete list from zero. When legacy numbering is
noncompact, Save remains available even without moving an entry. This repairs
an Add refusal caused by an unrepresentable next sort number without wrapping
or changing existing records just by viewing them. The LCD accepts only a full
current stable-ID permutation. Existing browser subset semantics are unchanged.

Saving uses the same loop-owned model-edit completion and order notification as
the browser. Failed model-file persistence does not undo an applied order.
The result distinguishes a failed save from uncertain replacement verification;
Retry model file runs COMMIT only. It never reapplies the permutation. A Busy
file retry retains that result; it cannot become a new empty order draft.
Authorization/configuration changes or timeout close the result before retry.
A successful file result certifies the model file, not separate NVS counters.

The backend checks current authority, configuration history, queued operations
and the active restart flag. It validates the complete permutation before and
after constructing its bounded ID-only request with the injected wiping
allocator, then checks authority again immediately before shared application.
It does not use the entity editor's prepared target cache or control-availability
filters. Sorting changes list order only; it does not require idle motors or
issue RF/GPIO/counter commands. Labels are read directly from captured physical
slots with an expected-ID check; rows do not repeatedly scan the full catalogue.

The component retains three 32-byte arrays: draft IDs, move rollback IDs and
aligned physical slots. It borrows the parent's FormView for each render rather
than retaining another frame. Native size is 160 bytes, guarded by a static
bound. The existing native Administration size is 1,216 bytes, leaving room
within its 1,536-byte bound for parent integration. Target size/build verification
and parent presentation/admission tests remain required before integration.
All output frames are protected; the parent must retain exact fully-presented
frame admission, wake/debounce filtering and discard on sleep/graphics loss.

Native suites `test_localorder` and `test_localentityruntime` cover draft-only
moves, partial-move cancellation, explicit default-Cancel Save, normalization,
all 32 slots, cached labels, empty/invalid snapshots, clock/auth/history loss,
queue/restart and allocation refusals, post-allocation permutation changes,
applied file failure and COMMIT-only retry. Removing the second permutation
check or selecting Save by default makes the relevant regression fail.
`tools/preview-local-order.sh` uses the actual GFX renderer for representative
list/move/confirmation/retry views and checks the fixed footer pixel widths.
These tests and previews are offline; they do not access a controller or send
motor/RF/HA commands.
