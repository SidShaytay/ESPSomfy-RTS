# Issue 36 — deployed save-on-press correction

Objective: remove Display Save/Cancel controls. Turn selects a row, press enters editing, turn changes a draft and the next center press saves that value and returns to row selection.

Acceptance: three value rows only; confirmation persists immediately without leaving Display; confirmed values survive Back, expiry and reboot; unconfirmed edits revert on Back/expiry; storage failures are shown; completed-frame and authorization guards remain intact.

Scope: display controller/renderer, regression tests, guide and screenshot; verified same-layout firmware-only update with fresh full-flash recovery set, then integration into main. Sid explicitly authorized flashing and its necessary access/backup/reboot on 2026-11-11. No RF/motor tests or HA changes are authorized.

Source: `56e51ed`, branch `codex/issue-36-save-on-press`, based on main `19b4dcb`. Reviewed correction merged into main; task worktree cleaned up after merge. Exact candidate binaries/ELF, fresh raw recovery dump, operator scripts and verification logs remain private. Use only git-agent/gh-agent as r-sidshaytay.

Completed milestone: failing confirmation regression reproduced the missing save; correction passes 39 controller tests, full native suite (1,321 passed, one optional private-backup skip), production preview/runtime/capture/adapter/queue harnesses. Reversing the successful-save baseline condition caused five regression failures. Updated #36 requirements and #33 manual checklist; both remain open.

Completed deployment preparation: fresh 16 MiB backup verified against the physical board, exact partition table, both app hashes, VALID OTA app0 sequence31, NVS CRCs, configuration and effective counters. Restored and rechecked the prior confirmed app. Clean source `56e51ed` passed canonical package validation, with exact firmware/ELF/source marker and matching bootloader/table.

Deployment complete: one firmware-only upload installed source `56e51ed` in app1 (planned OTA sequence32), retaining confirmed app0. Exact first-boot ELF identity and SDK health confirmation observed with one expected software reset. Five settings sections, entities, rooms and effective rolling codes matched fresh pre-update state, including a second check over 60 seconds after confirmation. Graphical framebuffer capture succeeded with local commands enabled. No physical wheel interaction, RF/motor tests or HA changes were performed.

Review: controller state transitions, persisted-baseline handling, failure/cancellation, unchanged-value write suppression, authorization and completed-frame checks reviewed without blocking findings. Normal and audit-mode UI harnesses, 122 JS tests and release-script checks passed.

Remaining: Sid’s physical confirmation of wheel selection, confirmation persistence/reboot and unconfirmed-edit Back behavior is tracked in open issues #36 and #33. The authorized implementation, firmware installation and main integration are complete. Private recovery pointer: `~/.local/state/espsomfy-lilygo/ISSUE36_CONFIRM_ACTIVE`. Historical Save/Cancel behavior is superseded by this request.
