# LCD interaction and security proposal for issue #25

Status: design for review plus an authorization implementation slice. Only the
implementation record below describes implemented behavior; other flows remain proposals. Sid approved
reuse of the configured web credential for both local controls and administration,
with 120-second inactivity expiry and a device-wide attempt limit. Sid later superseded the initial STOP decision: narrowly scoped STOP remains available
for at most 30 seconds after expiry for already-authorized continuing movement, ending
sooner when that movement ends, as described below. The existing
web throttle schedule uses
volatile local state (including the documented reboot-reset limitation). Other decisions
remain pending as marked below. This document is
based on fetched main `fc5a2947928fdf855185f77ecaae69c00b677156` and the reviewed local
main STOP fix `a493d50`. It does not authorize controller access, flashing, RF, motor,
pairing, calibration, limits or HA operations. All development verification uses fake
request sinks and synthetic model data. Hardware acceptance remains a separate stage.
See the [capability matrix](lcd-web-capability-matrix.md) for operation coverage.

Sid's revised build decision is one full-feature `lilygo-t-embed-cc1101` image for CC1101 and Plus, using their same project-relevant hardware. Keep the supported hardware list exact; the unused nRF24 does not create another firmware variant. Historical two-build evidence remains history and does not define the current product.

## Existing contracts and gaps

- `LocalUi.h`, `LocalUi.cpp` and `LocalUiAdapter.cpp` contain three fixed slots,
  shades 1/2 and group 1, and UP/STOP/DOWN. Existing input behavior includes 40 ms
  debounce, an 800 ms hold, held-at-start suppression, wake-only first input,
  button/turn conflict suppression and keeping the screen awake during movement.
  Display preferences currently save on Back; this is unsuitable for new admin forms.
- `LocalUiRenderer.cpp` uses 320×170 RGB565 with a title at baseline 28, three
  30-pixel menu rows and a footer at baseline 164. FreeSans9pt/FreeSansBold12pt are
  ASCII fonts. Adapter names currently replace bytes outside ASCII with `?` and
  truncate at 20 bytes; duplicate/non-ASCII names need explicit identification.
- `LocalUiRuntime.cpp` executes on the owning loop, paints into PSRAM and transfers
  four-row bands at least 2 ms apart. A command normally requires the selected
  target/action to have been fully presented. Position repaints do not invalidate
  a previously presented STOP. Rendering must remain separate from command dispatch.
- ADRs [0002](adr/0002-loop-task-is-the-only-writer-of-the-shade-model.md),
  [0005](adr/0005-one-websocket-protocol.md) and
  [0006](adr/0006-two-security-modes-eleven-http-routes.md) define one model writer,
  typed protocol operations and web security. `LoopQueue` holds 32 items; an item
  behind a radio-blocked unpair waits. LCD must not block loop while awaiting work.
- `ShadeRequests`, `ProtoOps`, `ProtoEdits`, `ProtoSettings`, `SomfyEditor` and
  `DeviceOps` are the domain/validation seams. Do not call HTTP through loopback,
  use rendering code to send raw frames, or duplicate rolling-code generation.
- `SecuritySettings` has None, four-character PIN and username/password modes.
  Web tokens are IP-bound HMAC values; they are not local authorization objects.
  Web login throttle is owned by async_tcp, so the loop cannot safely share its
  mutable instance. LCD currently performs no runtime authentication.
- `/lcd/screenshot` is web-token protected and permits one response at a time,
  but `LocalUiCapture.cpp` copies any presented framebuffer. There is currently
  no protected-screen suppression or local secret-buffer policy.
- Fetched remote main predates local main's `a493d50` shared-group STOP fix.
  Preserve that fix and its tests when integrating, rather than restoring the old
  per-member-only behavior from the fetched tree. Actual motor stopping has not
  been hardware-qualified by this design.
- `DeviceOps.cpp` currently mutates settings and ignores multiple persistence return
  values before replying success; `runDeviceOp` is socket orchestration, not a reusable
  transactional save API. LCD administration needs a shared typed save result and
  verified transaction/rollback semantics (or an explicit partial-failure outcome)
  before it can claim that settings were saved. Add injected persistence-failure
  tests; do not merely reuse the current success reply.
- Main's firmware/application upload routes begin writing on the first allowed
  upload chunk; `/restore` stages `/shades.tmp` but applies after upload completion.
  There is no LCD stage/review/apply session yet. GitHub updating is absent on this
  baseline (ADR 0007); issue #12 owns the verified updater, not this design.

## Approved scope and decisions required before dependent flows

| Decision | Recommended proposal | Alternative and consequence |
|---|---|---|
| Physical-control authorization — approved | Follow configured web security: None permits local actions; PIN/password requires a local unlock for controls and admin. Navigation/read-only status stays available. | Sid approved this scope; do not create an ungated everyday-control exception. |
| Unlock credential — approved | Reuse configured PIN or username/password. Do not store another local credential. Local-only session and 120 s inactivity expiry. | Explicit Lock, sleep, restart and security changes revoke it. |
| STOP after local lock/timeout — approved override | Keep a narrow movement-scoped STOP lease for the same previously authorized target/RF identity/exact member and capability context, only for movement already in progress at expiry. All other actions require unlock. | Lease ends irrevocably on observed idle or target/config/membership/security change; later movement cannot regain it. See STOP policy below. |
| Credential/text input | Bounded on-device ASCII keyboard for typed fields, masked credentials, explicit Save/Cancel; companion browser for convenient long/non-ASCII entry with the same validators. Preserve existing Unicode bytes on unchanged fields. | Full Unicode editing/font coverage adds font/input memory and requires a language coverage decision. ASCII replacement alone is not faithful name display. |
| Files — approved browser-owned equivalent | Backup, restore and manual firmware upload stay in the existing browser workflow. The shared LILYGO firmware bundles web assets, so separate web-image uploads on these boards are retired and refused before writes. LCD gives truthful instructions/status; no hybrid browser/LCD confirmation or staging session. | No new transfer endpoints, TLS/tunnels or SD support are authorized by this issue. Standalone LCD GitHub update must use reviewed #12 service. |
| Recovery | Existing authenticated web settings can repair local credential/configuration errors. No boot-button or failed-login bypass that grants admin. | A physical recovery mode requires a separate precise authorization/state-preservation design; do not silently equate physical possession with permission to erase or reset counters. |

These decisions are product/security choices, not feature deferrals. A browser-equivalent
workflow must be implemented and tested before claiming that matrix row complete.
Issue #12 admission, trust, rollback and compatibility decisions remain authoritative for
GitHub installation. Do not invent an updater or silently integrate unfinished branches.

## Menu and input model

Home opens Rooms, Devices, Groups, Settings and Diagnostics as one bounded paginated
menu. Rooms filters the configured device/group lists, including an Unassigned entry.
Devices and Groups show model sort order, with ID as a deterministic tie-breaker. Empty
lists display “No configured devices” and a Back action; no placeholder can send commands.
Three items fit a page, with a fixed `1–3/32` indicator. Stop at list ends rather than
wrapping into a command; each detent changes at most one row. Back preserves the selected
stable ID when returning, or selects the nearest surviving row after deletion.

The following are layout mockups, **not production-renderer screenshots**:

```text
Devices                    4–6/32
┌ Kitchen curtain     #09 ──────┐
│ Living room…        #17   42% │
│ Living room…        #24  idle │
└───────────────────────────────┘
Turn: select   Press: open
```

Use three 30-pixel rows starting at y=40, title baseline 28, footer baseline 164 and
296-pixel content width, matching the production renderer's current geometry. A selected
name can open a read-only details page with its full supported text and stable ID. Never
use name as identity. Preserve UTF-8 boundaries during truncation; use a documented
replacement glyph per unsupported codepoint, not per byte, and always show type/ID to
distinguish collisions. Production-renderer previews must cover long names, identical
prefixes, duplicate names, malformed UTF-8, non-ASCII names and maximum capacities.

A device detail page shows type, room, availability, estimated lift/tilt and a three-row
operation menu. The primary page selects STOP by default and keeps it reachable with a
single Back from an editor while moving. “More” opens capability-specific lift target,
tilt target/steps, favourite/MY, toggle, sun/light and other matrix operations. Do not
show an unsupported action as an apparently valid UP/DOWN command for a toggle device.
The menu derives capabilities from the shared domain rules for all 17 shade types,
5 tilt modes and actual protocols; it does not assume every paired device is RTS.

```text
Kitchen curtain #09      moving
Estimated 42% open
┌ STOP ────────────────────────┐
│ Open                         │
│ Close                        │
└───────────────────────────────┘
Press: send   Back: devices
```

Rotation and navigation never send RF, mutate settings, save or advance calibration.
Push release activates the presented item. Hold and Back cancel navigation/editor state;
Back never saves an admin draft. Preserve held-at-boot, bounce, turn-while-pressed,
wake-only and simultaneous-input suppression. Returning from an operation resets focus
to STOP or a neutral menu item, never the next destructive action. No automatic repeats
from a held encoder button; protocol-specific repeated commands use explicit domain ops.

Numbers have a bounded draft and explicit Apply/Cancel; rotation changes only the draft.
Lift/tilt drafts use displayed coordinates and the domain's flip transform exactly once.
Text entry selects one printable character/control per detent; push adds it. Controls
include Backspace, Done and Cancel, with a byte count and a field-specific bound. Validate
before accepting Done and again before applying. Network changes warn that the companion
may disconnect. Full-screen confirmation shows action, stable entity ID and scope, with
Cancel selected by default; sensitive content is never repeated in confirmation text.

## Intent, validation and completion

Proposed local intent carries entity kind and stable model ID, RF address when relevant,
model/config revision, sorted group member IDs plus each RF identity, capability revision,
authorization generation and the typed requested operation. Never derive a request ID
from list offset. A single loop-owned generation advances on relevant model/settings
mutation, including restore; ordinary estimated-position changes do not stale the page.

At execution, re-read the live model and reject visibly if any relevant identity,
membership, protocol, pairing, capability, authorization or admission condition differs.
Delete/recreate with the same numeric ID must fail through identity/generation mismatch.
A changed estimate alone can remain valid for explicit targets, but STOP rechecks current
motion. Re-open/refetch after a stale rejection rather than silently retargeting.

Keep one outstanding local intent and one editable form. Reuse typed validation and
loop-owned domain execution; if work is queued, the identity guard travels with the work
and is checked when it runs, not only at enqueue. Extend common domain entry points where
necessary rather than manually changing the model. Preserve radio-disabled maintenance,
TX backpressure, restore/update admission and busy responses. A radio-unavailable failure
must not become success just because the input was accepted.

Distinguish “Queued” (accepted), “Applied” (loop/domain completed), “Save failed” and
“Result unavailable—refresh”. Radio acceptance is not motor acknowledgement. Cancel
removes only a draft/unsubmitted intent; once a command was accepted or transmitted,
show “Already submitted; cancellation cannot undo movement”. Never say that Back undid RF.
For partial group actions, report partial/refused status and do not retry automatically.

STOP is a distinct intent from favourite/MY despite sharing the on-air command:

1. Single target: send MY only if the current, matching target is estimated moving and
   its device/protocol permits this meaning; otherwise refuse with a visible reason.
2. Group: one group MY only when all valid applicable members are currently moving,
   identities and exact membership match, and the shared protocol/group semantics permit
   it. Preserve `a493d50`'s behavior for the existing pair.
3. Mixed moving/idle group: stop only matching moving members through domain requests;
   never broadcast MY to an idle member. Recheck immediately before each request.
4. All idle: no RF. Favourite is a separately labelled intentional action, with shared
   idle/tilt/type restrictions. `set_favorite` is administration with confirmation.

At authorization expiry, retain a STOP lease for at most 30 seconds, only for matching movement observed both
while the session was authorized and at expiry. Freeze the eligible moving member set;
a group member that starts moving later cannot acquire a lease. Revalidate exact target,
RF identities, membership, capabilities and security configuration before every request.
The deadline is the original session-expiry time plus 30 seconds; a late poll, activity,
or failed unlock cannot extend it. Observed idle irreversibly removes the target/member from the lease; idle then new motion
does not revive it. Any relevant configuration or security change revokes the lease.
A shared group MY requires every current member to retain a matching continuing-motion
lease; otherwise stop only leased moving members individually. No lease permits an idle
MY/favourite, new movement, another target or a fallback after a refused shared request.
Keep this STOP action reachable while other controls show Unlock required.

RTS position/motion is estimated. External remotes and unknown movement can make it
wrong; the UI must not claim a safety stop, emergency stop or motor acknowledgement.

## Local security and sensitive screens

The trust boundaries are physical input → local policy, asynchronous web/HA edits →
loop model, companion payload → validators/staging and framebuffer → capture response.
Protected assets are authorization secrets, Wi-Fi/MQTT credentials, backup contents,
rolling codes, RF identities, calibration, limits and firmware trust state.

Implement the approved loop-owned local authorization state.
A security-settings revision revokes authorization before subsequent operations, including
queued ones. Failed entry gets no domain request. Reuse the `LoginThrottle` lockout
calculation in an independent local instance/policy: fifth failure begins 30 s lockout,
subsequent failures double up to 15 min; forget only after the established quiet period.
Use a 64-bit monotonic clock, display remaining wait, and never clear failures by Back,
sleep or changing screens. Failures 1–4 do not lock; failure 5 begins the first lock.
A successful permitted validation resets failures. An attempted submission during lockout
does not validate credentials, reset the lock or extend its deadline. Do not share async_tcp's mutable per-IP table. Volatile failures permit a reboot-based
bypass. Persisting the failure level on failed submissions and reapplying the full lock
duration at boot is a possible stronger policy with flash-wear and recovery consequences;
Sid accepted volatile state initially, matching the existing web throttle; no new
lockout persistence writes are introduced. Do not claim power-cycle-resistant lockout.

Do not prefill stored passwords into an editable or renderable object. Existing values
appear as “Set”; replace requires an explicit draft, and clear is a distinct confirmed
action. Mask every credential character without a reveal feature by default. Fixed
buffers use schema maxima plus terminator, reject overlength, and are explicitly wiped
on submit, cancel, timeout, lock, navigation away and error. Never include secrets in
notices, logs, scene hashes with exported diagnostics, previews or screenshots.

Mark the complete credential/unlock screen as protected. Capture returns
a refusal without pixel data while protected, even when a request arrived before entry.
When leaving, wipe drafts and repaint every framebuffer pixel before allowing capture;
a partially presented frame is not a safe nonsecret frame. Test entry/exit during an
in-flight capture and during band-transfer failure. Existing immutable screenshot buffers
already handed to a response cannot be recalled; therefore secrets must never enter
capture buffers in the first place. Apply no-store headers to all capture errors too.

## Browser-owned files and standalone updater

Sid explicitly selected the existing browser-owned workflows as the equivalent for
file-based backup/export, restore/import and manual firmware/web uploads. The LCD cannot
pick or save host files. Its corresponding pages explain that those operations are
performed in the browser and do not imply local staging, review, approval or cancellation
controls that the current server does not support. There is no new hybrid transfer
session, file endpoint, TLS/tunnel requirement or SD storage in this scope.

Current backups omit password/PIN/Wi-Fi passphrase values but expose identifiers,
configuration and rolling counters. Existing web authorization still applies. Keep their
contents outside logs, screenshots and fixtures. Restore retains the state-preservation
and counter-recovery requirements of [flashing](flashing.md) and issue #9; this offline
stage does not authorize restoring any production file or changing current counters.

Functions that can be fully standalone on LCD remain local. Firmware checking/install
uses only the reviewed issue #12 service, with explicit shared CC1101/Plus hardware compatibility, authenticated
metadata, feature/partition/web compatibility, staging admission, quiescence and rollback.
The LCD is its client, not another installer. Until that service is reviewed/integrated,
show unavailable honestly; do not present browser manual upload as standalone GitHub update.

## Memory, scheduling and failure budgets

These are proposed bounded design budgets, not measured hardware headroom:

| Allocation/work | Bound and failure behavior |
|---|---|
| RGB565 framebuffer | Existing 108,800 PSRAM bytes; no internal-heap fallback for full graphics. Missing/allocation-failed PSRAM shows safe diagnostic state and admits no invisible commands. |
| Screenshot | Existing one response, about 108,866 PSRAM bytes; allocation failure returns 503 and releases admission. Protected frames return no pixels. |
| SPI transfer | Existing 320×4×2 = 2,560 internal bytes; one short shared-bus band per eligible tick; retries return to loop. |
| Catalog/snapshot | At most 32 devices, 16 groups, 16 rooms; bounded page of three full labels. At most 32 member identities for the focused group. Aim ≤8 KiB total catalog/focused snapshot in PSRAM; enforce `sizeof`/allocation tests before growing it. No full-model stack copy. |
| Editor | One ≤4 KiB validated form/draft allocation in PSRAM and one local intent. Secret buffers bounded by current schema (Wi-Fi up to 64, security/MQTT username/password up to 32); actual ownership is explicit. No recursive menus/JSON expansion. |
| Navigation/text entry | One input sample and bounded transition per loop. No waits, busy loops, network calls or I2C transactions in renderer/input handling. |
| Transfer/TLS | Use issue #12 admission and measured internal free/largest/min heap thresholds; reject start rather than stealing framebuffer or radio resources. Do not invent thresholds from PSRAM size. |

Keep the existing bounded fuel-gauge polling and shared SPI arbitration. Measure real
loop service interval, maximum paint and band hold time; repaint timings alone do not
prove radio/network responsiveness. Combined TLS handshake, full firmware staging,
screenshot, reconnect, long idle/fragmentation and repeated editors/transfers must meet
explicit measured acceptance limits on each board after a separately authorized stage.
Until then record runtime heap/latency acceptance as pending, not passed by a build.

## Offline evidence required before integration

- Native controller tests: zero RF intents on navigation, wake, boot-held buttons,
  bounce, simultaneous input, timeout, cancellation, failed unlock or stale model;
  wraparound clocks, list bounds and STOP reachability during drafts.
- Dynamic adapter fake sink: empty/sparse/max models, every type/protocol capability,
  rename/delete/re-pair/reused ID, group membership/RF replacement, radio disabled,
  backpressure, freeze and stale queued work. Check exact operation/target/repeat count,
  all-moving/mixed/idle groups, partial refusal, favourites, tilt and flip semantics.
- Admin fake persistence: strict bounds, invalid fields, edit/save/cancel, duplicate
  IDs, transaction failure, unchanged counters/identities and explicit result delivery.
- Authorization tests: configured mode, inactivity expiry, credential changes,
  retry/backoff, reboot policy, queued intent revocation and the narrow STOP lease after expiry, including idle-then-new-motion and member movement changes.
- Runtime/capture tests: protected-screen capture on every entry/exit race, disconnected
  capture, allocation failures, partial band presentation and resuming only after full
  sanitized redraw. Use synthetic secret sentinels and assert absent output bytes.
- Browser-owned equivalent pages: truthful file workflow instructions without invented
  staging or local confirmation/cancellation. Preserve existing web transfer tests.
  Standalone #12 fake-service checks cover identity/trust/admission/failure/rollback.
- Production-renderer previews: all new pages at 320×170, long/duplicate/non-ASCII names,
  masked credentials, confirmation defaults, error/queued/partial state and dim/wake.
  Text mockups above alone do not meet the preview acceptance criterion.
- Existing native, JS, board-profile, status-panel, adapter/runtime/capture/renderer and
  tooling suites; the single shared CC1101/Plus full firmware build; generic-board build isolation and
  negative compiler/package contracts. Record ELF/slot size; hardware memory and
  latency evidence stays explicitly pending. Independent review must challenge auth,
  capture races and STOP/MY with fake sinks before feature integration into main.

## Authorization slice implementation record

Final shared CC1101/Plus integration, including the approved 30-second cap: 700 native
tests passed with one optional private-fixture skip; JS69, Python36, release script and
production renderer/runtime/capture/domain/credential-adapter harnesses passed. Shared
and generic ESP32 firmware compiled. The shared image reports 108,924 static RAM bytes
and 1,371,230 linker program bytes. A parallel firmware invocation exposed the existing
shared `data/` minifier race; sequential firmware builds passed without a source change.
These are offline checks, not physical stopping or hardware latency evidence.

The isolated authorization slice implements configured None/PIN/password checks for
local commands and display-setting persistence, a 120-second monotonic inactivity
session independent of estimated movement, device-wide volatile web-schedule throttling,
revocation on any security setting change (including inactive username/password/PIN)
and exact-byte bounded masked entry. A private bounded cache compares all configured
security fields without exposing their bytes or a secret fingerprint to the model. ASCII character input
and Byte 01–FF input support the existing byte-limited credentials without substituting
unsupported Unicode. NUL is excluded because configured fields are C strings. PIN is
at most four bytes; username/password are at most 32 bytes each. User entry drafts wipe
on submit, cancel, timeout and sleep; configured credential copies wipe before replacement.
The boot/non-running model fails closed before configured security is loaded.
Display-setting previews revert and close on authorization expiry, security changes or
sleep; wheel input cannot continue modifying an unauthorized open draft. Existing
authorized display-settings Back/save behavior is retained in this slice.
Web token generation and web login behavior are unchanged.

A single generic 32-member `StopLease` follows the last actual authorized target.
Ordinary menu navigation keeps observing that target; selecting another actual target
changes or revokes scope rather than granting a catalog-wide exception.
Expiry freezes continuing observed member motion for at most 30 seconds. Another moving, never-focused target
gets no lease. Observed idle removes eligibility permanently until a new authorized
session; member/target/RF/security/relevant configuration changes revoke it. All other
commands require the current session generation. Ordinary dispatch and page activation
also compare the relevant configuration fingerprint. The later dynamic-model slice still
needs a mutation generation to reject delete/recreate or re-pair that returns to an
identical final configuration between observations; a fingerprint alone cannot detect it.

The runtime sets capture protection before painting unlock and keeps it set through a
complete sanitized redraw after leaving. Both a new request during entry and a waiter
from before entry are denied without pixel allocation. Protected/error responses have
no-store where a response can be allocated; allocation failure returns 503 without pixels.
Graphical allocation failure admits no invisible commands. Actual sleep revokes once per
transition; an explicit Lock menu item remains for the later administration navigation
slice rather than being claimed available here. Wrong unlock cannot grant or expand an
existing timed-out STOP lease. Any backwards clock observation revokes the session and
lease, and validation remains blocked until the clock catches up without resetting throttling.

Offline evidence: 27 focused native auth/editor/controller tests passed; production
adapter fake-sink, credential-adapter, runtime, capture and real GFX renderer harnesses
passed. Previews include masked PIN/32-byte username/password, byte mode and throttle.
Visual inspection found a 32-star/count overlap, corrected by a bounded masked text area.
Mutation checks killed an expiry-boundary weakening, STOP-lease union instead of
intersection, removal of the protected-capture handler guard, and bypass of dispatch
authorization. The isolated legacy Plus UI configuration compiled with pinned Core 6.2.0: static RAM
108,996/327,680 bytes; reported program flash 1,368,490/3,342,336 bytes; firmware binary
1,413,472 bytes (1,928,864 bytes below its slot bound); ELF 28,988,664 bytes. These are
link/build sizes, not runtime heap measurements.
that is an authorization smoke check, **not the final shared-board build contract**.
Final integration must build the single shared configuration Sid subsequently requested.
No firmware was flashed, and no controller/HA/RF/motor operation was performed.
