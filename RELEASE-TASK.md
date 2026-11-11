# Public LILYGO release

Objective: prepare a v4.1.0 non-engineering firmware build from latest stable main, with accurate user documentation and state-preserving first-upgrade instructions.

Baseline: origin/main ef1bd12, fetched November 10, 2026. Workspace: release-4.1; branch codex/release-4.1.

Acceptance: README covers LCD/status and shutter screenshots, dark mode, security hardening, cjkas fixes, bundled firmware/web image, verified PSRAM-backed GitHub updates, and initial migration preserving settings/counters. Build an identifiable release package; run relevant build/release checks and review; integrate completed work into main. Sid subsequently authorized flashing the attached controller, including the required fresh backup and reboot verification. No RF commands or movement tests are authorized.

Scope: release metadata, documentation, reproducible screenshots from the real renderer, build/package verification. Preserve existing feature behavior and private recovery data.

Version: 4.1.0 after Sid clarified that the only engineering controller is attached and under his control. It requires a one-time manual upload from 4.9.1. Preserve Git history; engineering release listing cleanup is separate.

Completed milestone: user docs, migration guide, release notes, seven real-renderer screenshots and v4.1.0 metadata prepared. Checks: 1318 native passed (one optional skip), 122 JS and 96 Python passed; release scripts and renderer passed. Next: clean tagged SDK build/package, board/UI checks; fresh controller backup, exact firmware-only upload and postboot verification. History review remains separate from main replacement.

History scope: Sid requested a review of squashing long development sequences while preserving a unique commit per feature. Audit: 627 post-import commits, 141 merges; 906ee81 reintroduced the already-consolidated LCD branch. Detailed review at build/history-review.md. No history rewrite/deletion authorized or performed yet. Preserve feature boundaries; retain baseline and original evidence.
