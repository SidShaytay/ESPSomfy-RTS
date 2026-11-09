# Repository guidelines

## Code and builds

This branch uses pinned cjkas v4.0.2 as its baseline. Firmware lives in `src/`, the vendored CC1101 driver in `lib/`, web sources in `data-src/`, and PlatformIO environments in `platformio.ini`. Preserve the newer driver, scheduler and radio locking when changing the shared LILYGO board profile. See `LILYGO.md` and `docs/adr/0008-lilygo-maintained-baseline.md` for the migration decision.

Firmware builds need pioarduino Core 6.2.0; stock PlatformIO refuses the pinned platform. `bash tools/setup-lilygo.sh` installs user-owned tools. `bash tools/build-lilygo.sh` packages `lilygo-t-embed-cc1101`, the single full-feature firmware for both T-Embed CC1101 and CC1101 Plus. It requires display, interactive UI, local commands and bundled web assets; the unused nRF24 module does not need a separate binary. The Plus-only environment/macro and suffix profiles are retired, not aliases. The canonical package embeds the web UI in firmware and must not contain `littlefs.bin`; its `/updateApplication` route refuses uploads before any update work. Generic environments are built with `pio run -e <environment>`. Do not commit generated binaries or private backup/configuration data.

## Backups and flashing

Follow [the standard flashing procedure](docs/flashing.md). Identify the actual board, running build, features, active app slot and partition layout before choosing an image. Version labels alone do not identify a source revision. Keep changing deployment evidence in the task record and private recovery folder, not in this file.

Capture fresh state and counters before every write. Take a verified full-flash backup by default; use the documented firmware-only A/B exception only when Sid explicitly authorizes it. Prefer `/updateFirmware` into the verified inactive slot on an unchanged layout. Preserve NVS, LittleFS, bootloader, partitions and the previous active app. A layout change or USB recovery needs its own exact write/readback plan; never substitute a factory image or erase command. Coordinate matching HA changes separately and compare state after reboot before any movement tests.

## Tests

Run `pio test -e native` for native C++ tests, `node --test "test/js/*.test.js"` for web protocol tests, and `bash test/scripts/test_set_version.sh` for release scripts. Run `bash tools/test-board-profile.sh` with a host C++ compiler for board GPIO/SPI checks. Build tooling tests and manifest verification are documented in `LILYGO.md`. Add regression tests for changed behavior.

For native coverage use `pio test -e coverage` followed by `gcovr`; `gcovr.cfg` controls the included sources. For JavaScript coverage add `--experimental-test-coverage --test-coverage-include="data-src/js/**"` to Node’s test command. Files outside the native source filter need firmware builds or separate host harnesses.

## Style and review

Follow nearby code: C++ generally uses two-space indentation and opening braces on the same line; JavaScript and CSS generally use four spaces. Use PascalCase classes/module filenames, camelCase methods and uppercase macro constants. Preserve local exceptions and avoid unrelated formatting. No general formatter or linter is configured.

Keep commits focused and explain the problem and resulting behavior in PRs, linking the issue and reporting builds and tests. Include screenshots for visible UI changes. Hardware evidence must distinguish compile checks, radio register checks, RF reception/transmission and observed movement; record hardware, versions, reproduction steps and relevant sanitized logs when such tests are authorized.

## Issues and worktrees

`main` is the authoritative completed integration baseline. Start each new feature branch/worktree from the latest `main`. After the required review and tests, integrate each finished feature into `main` before the next feature builds on it, so features accumulate in order. Presence in `main` defines completion; do not substitute an agent's completion claim or silently combine unfinished parallel branches. Feature integration needs no additional approval step. Keep physical flashing authorization separate.

GitHub Issues in SidShaytay/ESPSomfy-RTS is the authoritative status record. Use parent/sub-issues, dependencies, acceptance and verification evidence; record blockers before closing issues. Use separate branches/worktrees for concurrent edits and preserve other worktrees' changes. Use `git-agent` for Git and `gh-agent` as `r-sidshaytay` for GitHub; stop if another account is active. `tasks/plan.md` and `tasks/CONTINUE.md` retain the historical bring-up context; the current issue’s limits take precedence.

## What needs Sid’s permission

- Obtain explicit authorization for the requested live stage: controller access/backup, flashing and its reboot, HA deployment/restart, RF or motor tests, pairing, calibration, or motor-limit changes. Continue stages already authorized in the current session without asking again; flashing permission does not authorize movement tests.
- Keep credentials, device identifiers and raw backups outside Git and shared storage. Recovery must preserve current counters and follow the evidence in issue #9; dated snapshots are not automatically safe to restore. Never run duplicate radio identities concurrently.
