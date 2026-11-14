# Issue 23: guided shutter calibration

Objective: provide guided calibration on the LILYGO LCD and web, accounting for slack and settling where needed, and use each shutter's profile for individual and group positioning.

Acceptance: direct and segmented mechanisms; human endpoint observations separate from estimates; opening/closing/reversal measurements; review, save and discard; compatible persistence; correct retarget/reversal and RF failure handling; bounded runtime diagnostics; repeatable board checks; review, main integration and deployment. Physical accuracy still needs watched calibration.

Authorization: Sid approved board access, flashing and automation, with RF testing on November 13, 2026 from 08:05 to 20:00 PST (November 14 04:00 UTC). He then asked us to finish development and deployment while away. Never invent observations. Pairing and motor-limit programming are outside scope.

## Delivered

Firmware 4.2.0 from clean main commit 50e1f23 is installed and confirmed healthy. Later main commits add tests and documentation only. Code and documentation are pushed. GitHub issue 23 was rewritten around the final design, evidence and remaining acceptance. HA remains unchanged at b396171; no matching integration change was needed.

Both interfaces use the same owned guide and actual RF timestamps. Segmented profiles include take-up, halfway, settling and reversal measurements. Direct travel skips mechanical phases. Saved profiles control individual/group moves. Compatible group members share starts and retain individual Stop deadlines.

Stops during uncertain mechanical phases and incomplete RF create a saved reference requirement. Percentage requests then require an explicit physical endpoint confirmation; entering a number does not count. Public endpoint reporting waits for completed movement. There is no motor position feedback.

Release firmware includes motion/RF/calibration category filters, optional USB output, bounded web retrieval and automatic expiry. Trace is off at boot. Raw LCD input injection remains instrumented-only. The release-board routine uses normal APIs and screenshots; its RF mode takes exclusive ownership, checks active travel before Stop, records actual transmissions and checks the stopped model stays stable.

## Verification

- 1,457 native tests passed; one existing skip. JavaScript: 131 passed. Tooling: 113 passed. Version scripts, board profiles, production LCD renderer/runtime/capture, owned-repeat and GPIO harnesses passed.
- Forty-two group/guide adapter scenarios passed. Independent review and regressions cover received remote Stops, reversal knowledge, zero legacy travel timers, exact Stop outcomes and trace clearing/expiry.
- Direct and segmented/reversal/save browser flows passed with synthetic RF and no console errors. Example screenshots are in the calibration guide; they are not physical test evidence.
- Fresh verified full-flash recovery sets preceded both firmware writes. Exact first-boot ELF and SDK health were verified, and settings, profiles, identities and counters matched each fresh pre-write capture. Only the inactive app was written; the previous active app and data were preserved.
- Final live no-movement run: 18 passed, RF-only case skipped. Both individual RF routines then passed: directional and Stop transmissions each took 277 ms; reported position stayed at 98% without drift. Physical position was not observed.
- One final group Close took 277 ms. After the full travel interval the model reported both at 100%, idle. Effective counter deltas were exactly two per primary and one for the group. Settings and profiles remained unchanged. LCD capture succeeded. Final heap: 120,696 free, 110,308 minimum, 77,812 largest block; uptime 269,001 ms.

Live testing found two corrected defects: false trace overflow after clearing capture, and raw guide commands bypassing normal movement/Stop model handling. Both fixes are reviewed and covered by production adapter regressions. Final tracing is disabled. RF tests are stopped; the last RF command was group Close.

## Remaining user verification

Calibrate each physical shutter while watching it, save actual observations, then check 25%, 50% and 75% individually and as a group in both directions. No synthetic profile was saved. The original settings were preserved, so both shutters need calibration before judging the group slider. Issue 23 stays open for that physical acceptance; development and deployment are complete.

Private recovery/evidence pointer: ~/.local/state/espsomfy-lilygo/ISSUE23_CALIBRATION_ACTIVE. Prior pointer: ISSUE23_RELIABLE_ACTIVE. Historical details remain in issue-23.log. Do not reuse an old recovery set for a new write, repeat an ambiguous upload, or reset during first-boot health verification.
