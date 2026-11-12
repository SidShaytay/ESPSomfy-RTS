# Issue 36 — save each display value on press

Objective: remove Display Save/Cancel controls. Turn selects a row, press enters editing, turn changes a draft and the next center press saves that value and returns to row selection.

Acceptance: three value rows only; confirmation persists immediately without leaving Display; confirmed values survive Back, expiry and reboot; unconfirmed edits revert on Back/expiry; storage failures are shown; completed-frame and authorization guards remain intact.

Scope: display controller/renderer, regression tests, guide and screenshot; verified same-layout firmware-only update with fresh full-flash recovery set, then integration into main. Sid explicitly authorized flashing and its necessary access/backup/reboot on 2026-11-11. No RF/motor tests or HA changes are authorized.

Workspace: `issue-36-display`, branch `codex/issue-36-save-on-press`, based on main `19b4dcb`. Use only git-agent/gh-agent as r-sidshaytay. Clean up this worktree after successful deployment and merge, retaining private recovery artifacts.

Completed milestone: failing confirmation regression reproduced the missing save; correction passes 39 controller tests, full native suite (1,321 passed, one optional private-backup skip), production preview/runtime/capture/adapter/queue harnesses. Reversing the successful-save baseline condition caused five regression failures. Updated #36 requirements and #33 manual checklist; both remain open.

Next milestone: finish fresh raw backup verification, restore the current confirmed application, and build the reviewed clean-source candidate before a single firmware-only upload.

Review: controller state transitions, persisted-baseline handling, failure/cancellation, unchanged-value write suppression, authorization and completed-frame checks reviewed without blocking findings. Normal and audit-mode UI harnesses, 122 JS tests and release-script checks passed.

State: implementation complete; clean firmware build and deployment pending. Full backup read is in progress; no firmware upload has occurred. Private recovery pointer: `~/.local/state/espsomfy-lilygo/ISSUE36_CONFIRM_ACTIVE`. Historical Save/Cancel behavior is superseded by this request.
