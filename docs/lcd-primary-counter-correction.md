# Primary rolling-code correction

Open **Administration → Devices → Advanced**, select the configured device by
stable ID, then choose **Primary rolling code** on the next menu page. The
protected review shows the device ID, full remote address, live primary code,
retained stored code (or Missing), and number of matching counter aliases.
The count includes the selected primary, other configured primaries, linked
remotes and groups sharing that address.

Opening or rotating never changes a counter. Cancel is selected first. Choose
**Edit primary code**, then rotate within 0–65535. Press for draft actions:
Cancel edit, Apply to draft, or switch between steps of 1 and 1000. Rotation
stops at the bounds. Apply changes only the draft. A separate review shows the
current and requested values and warns that correction writes **all matching
aliases**, retained counter storage and the current model file. That final
review also defaults to Cancel and must reach the display completely before
confirmation is accepted. Back cancels the numeric edit or pending operation.

Correction sets the last-used rolling code. A subsequent ordinary send advances
it using the existing 16-bit rules. Lowering is deliberate. This is an operator correction, unlike received-remote
learning, which never silently lowers a retained or live code. Correction sends
no RF, GPIO or motor programming command. Ordinary receiver/repeater processing
continues; this flow does not make the controller radio-silent.

A value equal to the primary is not automatically unchanged. It can deliberately
reconcile different aliases, a different stored value or a missing stored key.
Only a fresh, fully reconciled review—stored value present at that exact code,
all matching aliases equal, and no pending counter recovery—closes as No changes
without writing. Missing storage and stored zero remain distinct.

Confirmation submits one ordinary FIFO marker, expiring after five seconds.
Existing accepted work stays ahead; the UI never bypasses it or retries itself.
The credential session, exact device/raw context, global configuration history,
queue/restart/update admission and live aliases are rechecked. The shared domain
then reserves the identity and checks the exact stored presence/value and every
matching physical alias slot. A read-only final time/auth/context witness runs
after those reads and before any writable NVS open. A counter or authority change
refuses without writing and requires fresh review. All configured shades and
all affected tracked group members must be idle; accepted work for affected identities/groups and configured repeaters
must complete before correction can proceed.

Verified success reports Counter saved. If the counter applied but the model
file failed or its verification is uncertain, the result says so and offers
**Retry model file save**, with Close selected first. This serializes the current
model through the shared counter-file recovery service; it never repeats the
counter write or correction. If an attempted counter write may have persisted
without verification, the LCD reports Counter uncertain with Close only. The
identity and restart remain quarantined. There is no automatic replay, rollback,
reset, or unrelated-save shortcut for unknown counter storage.

Back, sleep, graphics loss, auth/config changes and expiry cancel the exact
pending marker before storage is destroyed. Numeric input has a 30-second
inactivity timeout; other Advanced interaction retains the shared 120-second
inactivity rule. Real input refreshes activity; rendering, receive traffic and
queue polling do not. The private review/draft and retained presentation copies
are erased when discarded, and another activation requires a fresh full frame.

The flow shares Advanced's existing wiping PSRAM pool, FIFO registry and borrowed
presentation frame. A typed union constructs either Learn cache/selection or the
primary numeric draft, with an explicit destructor before switching. It retains
no second full frame, full model JSON copy, queue, owner or storage writer. The
session plus allocator header is 1664 bytes in native measurement and remains
bounded to 2304 bytes; the pool remains
4096, outer handle/registry 32 bytes, Administration 1536 and queue marker 80.
These bounds exclude SDK allocation metadata and are checked by target builds.

Offline tests exercise the actual checked counter/NVS orchestration with fault
injection and production UI/backend/encoder/queue/renderer code with synthetic
storage boundaries. They cover exact low-valued alias changes, missing-versus-zero
storage, read failures, deadline/auth changes during the last read, deliberate
lowering, complete protected review, unseen confirmation, bounded draft steps,
unchanged reconciliation, cancellation/expiry, callback lifetime, uncertain
storage and file-only recovery without replay. No controller or physical command
is used for this verification.
