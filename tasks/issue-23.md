# Issue 23: guided calibration and reliable percentages

Objective: guided calibration on the LILYGO LCD and web, including mechanical take-up/settling/reversal where relevant, using each shutter's profile for individual and group positioning.

Acceptance: direct and segmented hardware; human endpoints distinguished from estimates; measured per-direction guide/review/save/discard; persistent compatible records; shared single/group timing; retarget/reversal handling; bounded runtime diagnostics and repeatable release-board checks; review/tests/main integration/deployment; watched positioning verification.

Authorization: Sid approved attached-board flashing, automation/debugging and RF November13 2026 08:05–20:00 PST (Nov14 04:00 UTC). Away: continue development and deployment to completion. Never invent physical observations. Pairing and motor-limit programming excluded. HA b396171 stays installed; no matching HA change needed.

Baseline: main33f44ef; installed47bb37e (UI4.1.4). Watched group starts synchronized;50% request physically about60% bathroom/85% bedroom from fully open; both stopped themselves. Mechanical play reported. Do not derive timing values from rough ratios.

Milestones completed:
- Pure calibrated motion/persistence and independent regressions; corec2cbecf reviewed.
- Shared source/target/lease-owned session with actual TX clocks, human markers, complete-press barriers, cancellation, expiry and durable-save outcome handling.
- LCD/web guides including segmented reversal measurement, explicit no-play option, review/save/discard, direct travel skipping mechanical phases.
- Saved reference barrier after uncertain mechanical Stops/partial RF; explicit human confirmation required before percentage moves. Public endpoints withheld until completion; numeric correction cannot impersonate observation.
- Runtime category-filtered release motion/RF/calibration trace, bounded32records, web/optionalUSB, defaultoff and automatic expiry; documented repeatable board routine and no-movement live checks. Raw LCD injection remains instrumented-only.
- Independent review found received remote/group Stop barrier gap; fixed with4additional regression cases. Reversal-known guard mutation caught. Diagnostics review no remaining blockers; expiry now latches.
- Native1456pass1existing skip; JS131pass; tools113pass; version tests; productiongroup39scenarios; productionLCD/runtime/adapter/capture and boardprofile checks pass. Canonicalfirmware compiled before final clean package. Full browser segmented/reversal/save walkthrough passed with synthetic RF and noconsoleerrors; mobilelight/dark screenshots and LCDproductionrenderer previews privately retained.
- Fresh full16MiB backup verified: layout/tableMD5/bothimagechecksums andSHA/NVSCRCs/model/rooms/effectivecounters. Prior confirmed firmware booted again; passiveUSBlogger restored. No new firmware flashed yet.

Next completed milestone: clean main integration and canonical4.2.0package, exactcandidateadmission, firmware-onlyA/Bdeployment, passiveboothealth and liveAPI/RF evidence. No physical calibration has been saved; watched25/50/75% individual/group accuracy remains unverified while Sid is away. GitHub#23 stays open for physical acceptance.

Recovery pointer: ~/.local/state/espsomfy-lilygo/ISSUE23_CALIBRATION_ACTIVE (private). Historicalrecord: issue-23.log. Previousbaselinepointer: ISSUE23_RELIABLE_ACTIVE. Backupwritesflag absent until exactsingleupload; do not repeat an ambiguous upload or reset during OTA health proof. Fresh state must still match immediately before firmware write.
