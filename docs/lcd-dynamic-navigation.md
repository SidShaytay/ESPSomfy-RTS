# Dynamic LCD navigation foundation

This foundation replaces the three fixed control targets. It is an offline,
host-tested implementation checkpoint for issue #25; capability menus and queued
control admission are separate follow-up slices on the same feature branch.

The wheel opens Home, then Rooms, Devices or Groups. Catalogues show three rows,
keep the selected stable model ID when entries reorder, and support all 32
devices, 16 groups and 16 configured rooms. Rooms also includes virtual All rooms
and Unassigned filters. Empty catalogues remain browsable. Names are labels;
selection and dispatch use the kind and stable ID, never a display row or name.

A fully transferred catalogue frame must show the selected row before Push can
open it. A target must then have a fully transferred control frame before any
command, including STOP. The same target's previously shown STOP stays available
through estimate and action repaints, including a blocked display transfer. This
permission does not transfer to another target or replacement configuration.

One focused target and three catalogue rows fit within the asserted 512-byte
snapshot budget. Exact member contexts live in bounded loop-owned storage rather
than a full catalogue of control contexts on the loop stack. A context records
stable target/member IDs, RF addresses, entity edit revisions, model edit history,
validated restore epoch and radio configuration history. Explicit position,
favourite and rolling-code corrections, pairing changes, deletion/recreation and
restores invalidate old intents. Normal movement estimates and rolling-code
advancement do not. Changed targets require a fresh displayed selection; merely
observing a new configuration cannot reseed an authorization-expiry STOP lease.

Groups validate all configured members, up to 32, with compatible RF protocol and
bit length. Shared group STOP is used only when every member is moving and, when
locked, every member has a continuing-motion lease. A refused shared STOP has no
individual fallback. Mixed groups send individual STOP only to the moving eligible
members; idle groups emit no MY command. Partial failures are reported explicitly.
The existing authorization inactivity and anchored 30-second STOP cap still apply.

The current font displays ASCII. Each unsupported UTF-8 code point is displayed as
one question mark; malformed bytes and control characters also display as question
marks. Raw names remain unchanged, and visible stable IDs distinguish identical or
fallback-rendered labels. Newline, tab and other controls never reach GFX layout.

Display changes require explicit Save; Back cancels the draft. Authentication
expiry or security replacement discards display drafts. Settings has a protected
navigation seam for the separately reviewed settings-form component. Entering
local-operation freeze revokes authorization so a draft or queued intent cannot
survive a complete freeze/resume interval with the same authorization generation.

Validation uses synthetic catalogues and the production controller, renderer,
runtime and adapter with fake request sinks. No controller, RF, motors or Home
Assistant were accessed. Generated previews are reproducible with
`tools/preview-local-ui.sh` and are not committed.

## Capability menus checkpoint

The next reviewed increment adds primary capability buttons and a three-row More
menu. It covers supported position/tilt targets, tilt direction/STOP, explicit MY,
80-bit Favorite and step commands, Toggle and sun flags through the existing typed
request policy. No light opcode or GPIO relay tilt remapping is introduced.

Numeric movement drafts use the shared bounded ValueEditor. Push enters a review
with Cancel selected; Apply must itself finish reaching the LCD before it can
submit the chosen number. Back, Cancel, timeout, target change and authorization
replacement discard without a request. Unknown estimates do not acquire a silent
zero/default target. Ordinary same-target STOP remains one Back away; tilt STOP
never inherits the authorization-expiry grace exception.

Offline evidence: 781 native cases passed with one optional private-fixture skip;
production renderer/runtime/capture and adapter/auth harnesses passed. Removing the
numeric presented-value/choice guard reproduces an unintended submission with SPI
blocked, and the regression fails; restoring it passes. The shared CC1101 firmware
build uses 110,684 bytes static RAM and 1,386,362 bytes flash. The separate pending
intent/LoopQueue integration still follows this checkpoint before full everyday
control completion is claimed. No hardware or HA acceptance is implied.

## Authorization session source

STOP grace is also bound to the original authorized session generation captured
before inactivity revocation. Failed unlocks may advance the current revision but
preserve that original source and deadline. Successful unlock, explicit lock,
security configuration replacement, clock rollback or the grace deadline clears the source.
An old focus or submitted command cannot borrow a later session's grace, even if
that replacement session both began and expired between adapter observations.
Missing source metadata fails closed. The same source ticket is available to the
subsequent queued-intent admission layer; no additional credential is stored.

The session-source foundation passed 787 native cases with one optional fixture
skip, the production UI/capture/adapter/auth harnesses, and shared CC1101 plus
generic ESP32 firmware builds. Independent mutations removing either the lease's
session comparison or the command's source check fail their corresponding
regressions. All sources were restored before the passing checks.
