# Repository guidelines

## Code and builds

This branch uses pinned cjkas v4.0.2 as its baseline. Firmware lives in `src/`, the vendored CC1101 driver in `lib/`, web sources in `data-src/`, and PlatformIO environments in `platformio.ini`. Preserve the newer driver, scheduler and radio locking when changing the opt-in Plus profile. See `LILYGO.md` and `docs/adr/0008-lilygo-maintained-baseline.md` for the migration decision.

Firmware builds need pioarduino Core 6.2.0; stock PlatformIO refuses the pinned platform. `bash tools/setup-lilygo.sh` installs user-owned tools and `bash tools/build-lilygo.sh` builds the exact Plus artifacts. Generic environments are built with `pio run -e <environment>`. Do not commit generated binaries or private backup/configuration data.

## Tests

Run `pio test -e native` for native C++ tests, `node --test "test/js/*.test.js"` for web protocol tests, and `bash test/scripts/test_set_version.sh` for release scripts. Run `bash tools/test-board-profile.sh` with a host C++ compiler for board GPIO/SPI checks. Build tooling tests and manifest verification are documented in `LILYGO.md`. Add regression tests for changed behavior.

For native coverage use `pio test -e coverage` followed by `gcovr`; `gcovr.cfg` controls the included sources. For JavaScript coverage add `--experimental-test-coverage --test-coverage-include="data-src/js/**"` to Node’s test command. Files outside the native source filter need firmware builds or separate host harnesses.

## Style and review

Follow nearby code: C++ generally uses two-space indentation and opening braces on the same line; JavaScript and CSS generally use four spaces. Use PascalCase classes/module filenames, camelCase methods and uppercase macro constants. Preserve local exceptions and avoid unrelated formatting. No general formatter or linter is configured.

Keep commits focused and explain the problem and resulting behavior in PRs, linking the issue and reporting builds and tests. Include screenshots for visible UI changes. Hardware evidence must distinguish compile checks, radio register checks, RF reception/transmission and observed movement; record hardware, versions, reproduction steps and relevant sanitized logs when such tests are authorized.

## Issues and worktrees

GitHub Issues in SidShaytay/ESPSomfy-RTS is the authoritative status record. Use parent/sub-issues, dependencies, acceptance and verification evidence; record blockers before closing issues. Use separate branches/worktrees for concurrent edits. `tasks/plan.md` and `tasks/CONTINUE.md` retain the historical bring-up context; the current issue’s limits take precedence.

## What needs Sid’s permission

- Do not access or operate the live controller, flash/erase/reboot it, send RF or motor commands, pair, calibrate or change motor limits without explicit authorization for that stage. Issue #13 authorizes offline work only.
- Keep credentials, device identifiers and raw backups outside Git and shared storage. Recovery still requires the staged validation in issue #9 and fresh counters before any approved write. Do not run duplicate radio identities concurrently.
