# Local entity administration

**Navigation version:** These paths apply to **4.9.0 and later**. For 4.8.0 or 4.7.3, see the [versioned path map](lilygo-local-ui.md#navigation-versions).

Open **System → Configure → Rooms, devices & groups** and unlock when required, then choose **Rooms**,
**Devices** or **Groups**. Select Add, or select an existing entry by its stable ID
to edit or delete it. Duplicate names are allowed; the kind and ID identify the
target. These flows work on the LCD without a browser. Full lists page through
every configured entry. When all slots are used, Add is marked full and pressing
it opens no draft. **Reorder** follows the configured entries and remains
available on a full list; see [local list ordering](lcd-ordering.md).

Every activation requires the exact selection and current configuration to have
finished reaching the display. If the display is busy, wait and press again.
Changing the list never turns an old press into a different room or a new Add.
The selected ID stays anchored through sorting or renaming; deletion returns
selection to Add and requires a new press.

Turning changes only the field editor. Apply copies a field into the bounded
local draft. Save submits the draft through the same loop-owned model edit
service as the browser. Cancel, leaving the screen, authorization changes,
configuration changes, timeout or allocation failure discard the local draft.
Delete names the exact kind and ID and starts with Cancel selected. Removing a
room makes its devices/groups Unassigned; deleting a device or group configuration
does not establish that a physical motor is unpaired.
Selecting a field that already has a draft offers **Cancel**, **Edit draft** and
**Revert field draft**. Revert restores only that field's original value and keeps
other drafts. It performs no model or file write.

Device/group Save has a separate default-Cancel confirmation showing the exact
target and the effects of configuration changes. In those forms, inactive GPIO fields
remain readable; a protocol change cannot silently discard an existing GPIO draft.
Revert that draft or select a protocol that uses it. Rejected saves show a protected
reason screen: Revise returns to the relevant field, while Cancel form discards all
drafts. Acknowledging the reason never retries Save. A whole-form refusal uses
Back to fields rather than naming an unrelated repair field.

Back from a field cancels that edit. Back from the field list discards all its
drafts and returns to the selected Rooms, Devices or Groups list. Back from that
list returns to Administration, then Home. Result screens stay protected until acknowledged and replaced by a full
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
before a retry. Device configuration changes wait until that device is idle and
its GPIO pulse has ended; group changes wait for every present member to be idle.
With missing group references, scalar settings remain editable and the group can
be deleted. Membership repair remains a separate flow. An unavailable radio or unpaired device does not prevent
configuration editing. “Model file saved” certifies only the checked model file result,
not separate NVS counters or power-loss recovery.

Linked remotes, group membership, counter correction, pairing and RF programming require their
separate checked domain operations and screens. They are not exposed by this
component.

Host verification uses the native suites `test_localentityform`,
`test_localentityruntime`, `test_protoedits` and `test_modelcommitservice`, plus
`bash tools/preview-local-entity.sh` with the pinned cached Adafruit GFX library.
`bash tools/preview-local-ui.sh` also drives the real encoder/Controller/Runtime
through room/device/group Add, Edit, default-Cancel and confirmed Save/Delete,
file-save failure, commit-only retry, protected capture and unseen-selection
refusal. Device tests preserve the counter and unpaired state; group tests retain
a missing physical member through a scalar save. These exercise the real UI and
backend with fake model/file effects.
No controller access, firmware upload, RF transmission or HA operation is part
of these tests.
