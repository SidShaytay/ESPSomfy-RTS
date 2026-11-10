# Local list ordering

**Navigation version:** Paths below describe source `015ac39`, intended for the next release and not yet deployed. Installed 4.7.3 uses the older menus; see the [installed-version path map](lilygo-local-ui.md#navigation-versions).

Open **This controller → Configure → Rooms, devices & groups**, choose **Rooms**, **Devices** or **Groups**,
then select **Reorder** after the configured entries. Add and existing entry
positions keep their previous meaning; Reorder is appended and remains available
when Add is full. The ordering screen covers the complete selected-kind list.

Select an entry by its stable ID, press to pick it, turn to move it, then press
to apply that move to the local draft. Back while moving restores the draft
before that pick, preserving earlier applied draft moves. Nothing has been
saved yet. Choose Save order and explicitly confirm; Cancel is selected first.
Back from the list discards its entire draft. After a completed save/result,
the parent list returns to the last explicitly picked stable ID if it still
exists, rather than an old row number. Scrolling past other entries on the way
to Save does not change that anchor. Empty and already unchanged, compact lists
close without a save.

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
bound. Native Administration is 1,376 bytes including Ordering and its existing
borrowed frame, within its 1,536-byte bound. Target size/build
verification remains a separate acceptance check. All output frames are
protected; the parent retains exact fully-presented frame admission and
wake/debounce filtering. Leaving Administration, sleep and graphics loss discard
the order draft. A list change while Reorder is highlighted keeps the launcher
selected and requires a fresh press after presentation.

Native suites `test_localorder` and `test_localentityruntime` cover draft-only
moves, partial-move cancellation, explicit default-Cancel Save, normalization,
all 32 slots, cached labels, empty/invalid snapshots, clock/auth/history loss,
queue/restart and allocation refusals, post-allocation permutation changes,
applied file failure and COMMIT-only retry. Removing the second permutation
check or selecting Save by default makes the relevant regression fail.
`tools/preview-local-order.sh` uses the actual GFX renderer for representative
list/move/confirmation/retry views and checks the fixed footer pixel widths.
`tools/preview-local-ui.sh` also drives actual encoder journeys for all three
kinds, blocked/unseen launcher/Save/confirmation, draft move undo, file failure
and COMMIT-only retry, all 32 device rows, numbering repair that unblocks a new
Add draft, and sleep/graphics-loss cleanup. Its fake filesystem tracks the order
projection separately: a failed write retains the old projection, while a
successful COMMIT captures the applied live order without replaying an edit.
These tests and previews are offline; they do not access a controller or send
motor/RF/HA commands.
