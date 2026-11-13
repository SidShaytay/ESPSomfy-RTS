# Issue 23: native group percentages

Objective: reliable native group percentage movement through current firmware and Home Assistant/Apple Home. Software integration and authorized deployment/testing are complete. Observed motor acceptance remains open in GitHub issue #23.

Firmware source: `47bb37e68219615a2bf1488d129aca3d4374319a`, based on completed main `a944911`. The scheduler, timing, protocol and production integration commits are merged and pushed to main. The installed UI version remains `4.1.4`; the exact source/ELF identifies this build. HA main: `b396171`; all 21 installed integration file hashes already match, so no replacement or restart was needed.

Authorization: Sid approved attached-board access, backup, flashing/reboot and RF commands for two hours, through November 12, 2026 at 20:40 PST. He separately approved HA deployment/restart if needed. No pairing, calibration or motor-limit changes occurred. Do not extend the RF window without a new instruction.

Completed acceptance:
- `groupTarget` 57 preserves operations 55/56, validates explicit targets and prevents replay. HA detects capability by name, keeps legacy fallback and does not retry ambiguous commands.
- Compatible shared starts, individual Stop timing, mixed directions, pending coalescing, retargeting, native reversal, refusal preservation and atomic cancellation. Idle motors do not receive My merely because they have queued targets.
- Favourite programming outcomes remain tracked; new group movement refuses unresolved programming. Ordinary integrated tilt, short 80-bit steps, favourites, GPIO, counter and receipt behavior are preserved.
- Independent review and mutation checks; 1,347 native passes/one optional skip, 123 JS passes, 142 HA/protocol passes, 29 production movement scenarios, 13 inherited group/restart cases and GPIO/counter/model/board/release checks. Canonical and generic ESP32 builds pass. Interrupted concurrent PlatformIO runs were retried in isolated build directories.

Deployment: fresh portable/settings capture and full 16 MiB recovery dump verified layout, both image hashes/checksums, VALID app1 source `56e51ed`, NVS CRCs, membership and effective counters. One firmware-only upload installed `47bb37e` into inactive app0, retaining app1. Exact boot ELF and SDK health confirmed with one expected reset; settings/entities/rooms/counters matched before RF. The read-only observer initially confused development identity `dev-47bb37e68219` with UI version `4.1.4`; its expectation was corrected and the whole passive interval independently validated. No second upload/reset occurred.

Live checks: direct Open/50%, duplicate replay, quick Stop, reversal, repeated Close, native web Open, HA Close arrow, HA 50% slider and HA endpoint Close all accepted with expected TX-driven estimates. Counters establish one group start and one Stop per member for the HA percentage; a repeated moving endpoint adds no start. Intermediate estimates are within one point (HA can show 51% for requested 50%). The initial matrix ended closed and idle. At 20:38–20:39 PST, after Sid said he was watching, Open → 50% was repeated. Both software estimates finished at 50%, stopped; physical observations are pending. All non-counter entity fields match pre-flash, with no additional reset, panic or radio RX failure.

Remaining: human-observed shutter outcomes and Apple Home percentage confirmation. Sid confirmed he was ready to watch; his report of actual movement has been requested. Keep #23 open; estimates and TX/counter evidence do not prove RF reception or motor positions. Next: Sid confirms both shutters start, reach intermediate/end positions and stop, including Apple Home 50%; investigate any reported failure within current authorization.

Private evidence: `~/.local/state/espsomfy-lilygo/ISSUE23_RELIABLE_ACTIVE` and `~/.local/state/espsomfy-ha-homekit/ISSUE23_RELIABLE_ACTIVE`. The pre-RF backup is now historical after counters advanced; take fresh counter captures before any further write/recovery. Merged task worktrees are cleaned up; continue from main.
