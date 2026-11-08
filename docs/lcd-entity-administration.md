# Local entity administration

The room administration component implements Add, Edit and Delete without a
browser. Parent navigation and presentation gating are not wired yet, so the
component is not reachable from the device menu. Integration must protect its
screens and require a complete matching display frame before acting. Shade and
group forms are not implemented by this room slice.

Turning changes only the field editor. Apply copies a field into the bounded
local draft. Save submits the draft through the same loop-owned model edit
service as the browser. Cancel, leaving the screen, authorization changes,
configuration changes, timeout or allocation failure discard the local draft.
Delete names the exact room ID and starts with Cancel selected.

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
No controller access, firmware upload, RF transmission or HA operation is part
of these tests.
