# Local entity administration

Open **Home → Administration → Rooms** and unlock when required. Select
**Add room**, or select an existing room by its stable ID to edit or delete it.
Duplicate names are allowed; the ID identifies the room. This flow works on the
LCD without a browser. Device and group administration are still being added.

Every activation requires the exact selection and current configuration to have
finished reaching the display. If the display is busy, wait and press again.
Changing the list never turns an old press into a different room or a new Add.
The selected ID stays anchored through sorting or renaming; deletion returns
selection to Add and requires a new press.

Turning changes only the field editor. Apply copies a field into the bounded
local draft. Save submits the draft through the same loop-owned model edit
service as the browser. Cancel, leaving the screen, authorization changes,
configuration changes, timeout or allocation failure discard the local draft.
Delete names the exact room ID and starts with Cancel selected.
Selecting a field that already has a draft offers **Cancel**, **Edit draft** and
**Revert field draft**. Revert restores only that field's original value and keeps
other drafts. It performs no model or file write.

The device/group form component adds a separate default-Cancel Save confirmation,
showing the exact target and the effects of configuration changes. Its screens
are not yet reachable through Administration. In those forms, inactive GPIO fields
remain readable; a protocol change cannot silently discard an existing GPIO draft.
Revert that draft or select a protocol that uses it. Rejected saves show a protected
reason screen: Revise returns to the relevant field, while Cancel form discards all
drafts. Acknowledging the reason never retries Save. A whole-form refusal uses
Back to fields rather than naming an unrelated repair field.

Back from a field cancels that edit. Back from the field list discards all its
drafts and returns to Rooms; Back from Rooms returns to Administration, then
Home. Result screens stay protected until acknowledged and replaced by a full
non-administration frame. Sleep, graphics loss and authorization changes discard
drafts. An applied edit remains applied when its result screen is closed.

The component reads current values rather than copying the model into a JSON
form. A changed-only document and temporary expanded request use the injected
bounded, wiping allocator. Names retain the model's 20-byte limit; sort order
uses its signed 8-bit representation. These bounds are shared with the typed
edit parser. Omitted fields preserve existing values, including legacy values.
Invalid original numbers and malformed UTF-8 names offer a separate replacement
prompt with Cancel selected by default. Replace starts an empty text editor or
a bounded numeric candidate; it changes no stored value. Apply updates only the
draft, and Save persists it. Cancel preserves the original and other drafts.
An invalid offered Add default (such as sort order 128 after an existing maximum
of 127) must be explicitly replaced before Save; it cannot silently wrap.

The runtime checks authorization, configuration history and the existing work
queue immediately before a mutation or model-file retry. Pending work produces
Busy, without a mutation, file write or automatic retry. This preserves accepted
queued work before a synchronous local operation. Remote requests continue to
use the normal queue.

A successfully applied edit can still fail to save its model file. The screen
retains the returned entity ID, discards the applied draft and distinguishes a
failed save from an uncertain replacement verification. Retry saves the current
model file only: it never repeats Add, Delete, an RF operation or a counter write.
A changed configuration or revoked authorization closes that result screen
before a retry. “Model file saved” certifies only the checked model file result,
not separate NVS counters or power-loss recovery.

Linked remotes, counter correction, pairing and RF programming require their
separate checked domain operations and screens. They are not exposed by this
component.

Host verification uses the native suites `test_localentityform`,
`test_localentityruntime`, `test_protoedits` and `test_modelcommitservice`, plus
`bash tools/preview-local-entity.sh` with the pinned cached Adafruit GFX library.
`bash tools/preview-local-ui.sh` also drives the real encoder/Controller/Runtime
through room Add, Edit, default-Cancel and confirmed Delete, file-save failure,
commit-only retry, protected capture and unseen-selection refusal.
No controller access, firmware upload, RF transmission or HA operation is part
of these tests.
