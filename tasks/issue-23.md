# Issue 23: guided calibration and reliable percentages

Objective: implement guided calibration on the LILYGO LCD and web, including mechanical take-up/settling where relevant, and use each shutter’s profile for individual and group positioning.

Acceptance: roller/direct-travel and segmented/slack hardware profiles; human-confirmed endpoints separate from estimates; per-direction calibration with validation/review/save/cancel; persistent backup/restore compatibility; shared motion calculations for single/group moves; reversal and same-direction retarget handling; guarded diagnostics and release-board automation; regression, browser/LCD, firmware and live checks; integrated main and cohesive GitHub status.

Authorization: Sid approved attached-board flashing, automation/debugging and RF testing November 13, 2026 from 08:05 to 20:00 PST (through November 14 04:00 UTC). This supersedes last night’s stop/offline-only scope. Physical endpoint confirmations must never be invented by automation. Pairing and motor-limit reprogramming remain outside calibration scope.

Baseline: main33f44ef; installed firmware47bb37e (UI4.1.4), HA b396171. Last night shared starts worked, but a50% closed request from fully open stopped bathroom about60% and bedroom85%, with no intervention or reversal. Sid reports mechanical play before visible motion. Root cause is not proven; do not infer new timing values from visual guesses.

Workspace: codex/issue-23-calibration / issue-23-calibration. Private prior evidence pointers: ~/.local/state/espsomfy-lilygo/ISSUE23_RELIABLE_ACTIVE and ~/.local/state/espsomfy-ha-homekit/ISSUE23_RELIABLE_ACTIVE. Prior counters/backups are historical; refresh before writes. Full recovery before flash; preserve inactive rollback and current model/counters.

Milestones:
- [x] Rewrite issue #23 as one cohesive product/design and acceptance record (November 13).
- [-] Define shared calibration/session contract, persistence and release diagnostics boundaries.
- [ ] Implement and verify calibrated motion/persistence with backwards compatibility.
- [ ] Implement shared guided session, protocol and LCD/web workflows.
- [ ] Implement bounded runtime trace/automation support and verify release builds.
- [ ] Review, integrate, fresh backup/deploy and exercise board/UI/RF within window; distinguish physical observations from estimates.

Implemented on branch: pure math2d731d5; profile persistence/protocolaa8fc69; independent production behavior and group testsf214df3/4c231b6; guide tests095b6b1. Uncommitted motion core now uses immutable calibrated runs for single/group moves and passes15cal+71legacy behavior cases,37productiongroup cases and ownedrepeat harness. Pure guide class passes19tests (two-direction drafts/markers/TXoutcomes/sourceflag/save); not yet connected to eitherUI. Ordinary rawgroup-arrow reversal is the remaining reviewed core clock defect; resolve before commit/deploy.

Next milestone: complete raw group-arrow clock/owned-repeat review and commit motion core. Then implement shared loop-owned guided service, source/session/target/exclusivity checks, LCD/web guides, reversal observation, position confidence, release diagnostics/automation, review/integrate/deploy. Do not end at a status/issue update. No new firmware flashed. Core native session7254/log /tmp/calibration-core-native.log and canonical45161/log /tmp/calibration-core-fw.log; earlier core1379native passed. Independent guide contract/test agent group_tests works tests only in ../issue-23-calibration-tests; review agent ha_regression is read-only. Historical detail is retained in issue-23.log.

November13 17:30 PST: core committed/reviewed c2cbecf, shared guide/session and web/LCD implemented and firmware compiled, offline mobile browser workflow checked. Reversal measurement six tests and interrupted mechanical-reference seven tests now pass; integrating their persistence/UI. Release diagnostics added to scope per Sid: bounded runtime category-filtered USB/web trace, expiry/default-off, repeatable actual-board routine separating software/RF evidence from observed movement. Next completed milestone: reference persistence + guided reversal integration, then diagnostics and final integration/deployment. No physical calibration markers will be invented while Sid is away. No new firmware flashed yet.
