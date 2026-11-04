# Implementation Plan: LILYGO T-Embed CC1101 Plus

For current session state and the next action, read [CONTINUE.md](CONTINUE.md).

## Objective and authority
Port the existing ESPSomfy RTS implementation to the user's USB-connected LILYGO Plus external-antenna device on vega. The user authorizes autonomous research, implementation, builds, backup and flashing of this device, GitHub issue management, and parallel subagents/worktrees. Motor pairing/programming requires the user's physical participation. Do not change motor limits. No cloud service is required for shutter control.

## Issue tracker and execution order
GitHub Issues is the authoritative task/status record; do not create a duplicate local task checklist.

Parent: https://github.com/SidShaytay/ESPSomfy-RTS/issues/1

1. https://github.com/SidShaytay/ESPSomfy-RTS/issues/2 — hardware research, positive device identification and full factory backup.
2. https://github.com/SidShaytay/ESPSomfy-RTS/issues/3 — opt-in board profile, safe fixed pins and diagnostics.
3. https://github.com/SidShaytay/ESPSomfy-RTS/issues/4 — pinned firmware/LittleFS build (tool setup may run alongside research).
4. https://github.com/SidShaytay/ESPSomfy-RTS/issues/5 — identified-device flash and boot/register/web checks.
5. https://github.com/SidShaytay/ESPSomfy-RTS/issues/6 — receive and decode the user's Telis remote.
6. https://github.com/SidShaytay/ESPSomfy-RTS/issues/7 — user-assisted pairing, controlled shutter operation and reboot persistence.
7. https://github.com/SidShaytay/ESPSomfy-RTS/issues/9 — private configured backup and verified recovery.
8. https://github.com/SidShaytay/ESPSomfy-RTS/issues/8 — local HA/HomeKit handover and final documentation.

Checkpoints: require backup and successful build before writing flash; valid physical RTS reception before pairing; observed UP/DOWN/STOP and reboot continuity before declaring automation ready. Distinguish compile checks, hardware/register checks, RF receive, RF transmit and actual shutter movement in evidence.

## Architecture
Keep protocol/timing/rolling-code logic unchanged. Add one compile-time board profile selected by `ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS`; generic builds retain current behavior. Deselect display/SD/nRF24 before powering peripherals, disable nRF24 CE and display backlight, and select the 433 MHz path before CC1101 initialization. Lock integrated radio pins against restored/UI values and reserve board control pins from relay use. Run headlessly.

Pin Arduino core and libraries to the existing release workflow. Configure ESP32-S3 QIO 80 MHz, 16 MB physical flash, 8 MB OPI PSRAM and hardware USB CDC/JTAG; retain the existing 4 MB OTA/filesystem partition layout to preserve updater assumptions. Build fresh LittleFS at 0x290000 with size1441792. Never erase the whole device during routine flash.

## Risks and recovery
SPI identity is insufficient proof of RF operation: issues598 and666 describe different failure modes. Keep factory backup outside Git, with checksum and exact recovery offsets. Physical PCB revision cannot be inferred solely from the MCU; compare manufacturer Plus pin sources and observed radio behavior, recording any remaining uncertainty. Stop for a short physical remote press at the receive step and for user-assisted additional-remote pairing. Network credentials remain local and out of public issues.

## Workspace
Application work: port originated on `feature/lilygo-t-embed-cc1101-plus`; check Git/PR status and `CONTINUE.md` for publication progress.
Build tooling worktree: sibling `../ESPSomfy-RTS-lilygo-build`, branch `chore/lilygo-build`.
User-owned tools/cache: `~/.cache/espsomfy-lilygo/`.
