# Issue25 workspaces

- Working: `/var/mnt/data/projects/personal/ESPSomfy-RTS/issue-25-work`, branch `work/issue-25`. Two commits above pre-issue25 `a493d50dbd6149e4aa8f3663811a809c19957494`: one consolidated final-code snapshot, then these task handoff notes.
- Backup: `/var/mnt/data/projects/personal/ESPSomfy-RTS/issue-25-engineering-v481`, branch `codex/25-engineering-v481`, unchanged at `23b239caee9e2532deee2ebd05f72dfc3d6bdc25` with its original history, validated package and build output. Treat it as read-only.
- Source tree before the handoff-only commit: `f35435d8276065945e4cd7ee25010f51e9518e47`, exactly equal to the backup.
- Private cleanup archive: `/var/home/sid/.local/state/espsomfy-worktree-cleanup/run-20261110T214929Z`. It contains the verified original Git bundle, every old head/reflog reference, per-worktree staged/unstaged patches, changed and untracked files, compressed useful artifacts, original Git worktree metadata and the full task history.
- Persistent dependency cache: `/var/home/sid/.cache/espsomfy-lilygo/issue25-preserved-libdeps`. The working `.pio/libdeps` points there. Existing framework/toolchain caches were not changed.

Only reproducible compiler objects/static libraries/dependency files and Python caches were omitted from artifact preservation. All old branch refs remain; removed worktrees can be reconstructed from their saved HEAD and patches. Artifacts are indexed by their original path in `worktrees/<old-name>/manifest.json` and stored as SHA256-addressed zlib objects. Recovery instructions and the restore helper live in the archive.

This squash changes Git identity but no firmware code. Keep existing version tags and SHA-bound release assets attached to the original history; do not retag them or claim old binaries came from the squashed commit. Source tests already passed on the identical final tree; this history-only operation does not need another firmware build or device test.
