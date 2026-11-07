# Task Memory

## Context and authority

- Objective: issue #15, 320×170 PSRAM LCD UI, physical controls, saved brightness/dim/sleep, screenshots, version details and battery percentage.
- Feature worktree: `/var/mnt/data/projects/personal/ESPSomfy-RTS/issue-15-phase2-ui`, branch `codex/issue-15-phase2-ui`. Use `git-agent` / `gh-agent` as `r-sidshaytay`; no upstream publication or issue closure.
- Sid explicitly returned the device and authorized flashing after the offline changes. Fresh backups and firmware-only A/B OTA are authorized. **Physical shutter controls must be enabled for Sid; the assistant must not send shutter/RF/motor commands or HA services.** No HA changes are required.
- The other agent installed source `c41cfc4` (UI plus unfinished issue-23 group targeting). Preserve that installed functionality in a separate deployment build; do not roll it back or represent issue 23 as physically validated. Main remains older and is not changed by this deployment.

## Completed work

1. [x] Installed phase-2 PSRAM UI preview, source `1664cf1`, then input/settings update `7061e98`. Private evidence pointers: `~/.local/state/espsomfy-lilygo/LATEST_UI_INSTALL` and `LATEST_UI_INPUT_FIX`.
   - Main RGB565 framebuffer 108,800 bytes explicit PSRAM; internal transfer buffer 2,560 bytes. Allocation failure -> simple status with cause -> blank if fallback also fails. Read-only framebuffer endpoint uses immutable PSRAM copy and existing authentication.
   - Wheel one item per detent, clockwise down; GPIO6 Back. Saved brightness and independent dim/sleep delays including No dim / No sleep. First input after dim/off wakes only. Sid confirmed screen, wheel and Back work.
2. [x] Fixed category gate, source `11ef3d4`. Actual paired RTS shutters (category 4) were incorrectly rejected by a roller-only condition. All categories now reach the existing dispatcher. Production-adapter regression reproduced the bug then passed, with identity/STOP checks retained. The other agent already included this fix in its installed firmware.
3. [x] Scrollable Details versions, source `49b715f`. Firmware comes from compiled `FW_VERSION`; Application from installed filesystem `settings.appVersion.name`. Six visible rows, ten total, wheel scroll; removed redundant issue/build label. Renderer tests check changed version values and scrolling change pixels.
4. [x] Battery reader implemented and checked offline; committed as `ebeb209`; USB identity/battery diagnostics added in `ce5f559`.
   - BQ27220 at 0x55 on SDA8/SCL18, I2C controller1 at100kHz, 2ms transaction timeout. Only register-pointer reads; no charger/gauge configuration changes. Five registers sampled across loop passes, ten-second cycles. Gauge SOC/voltage shown on Status/Details; absent/uninitialized/invalid/failed readings explicit.
   - Final checks: 662 native tests passed plus one optional skip; renderer/runtime/capture/adapter, panel/radio/GPIO harnesses passed. UI/simple-status/headless builds passed. SOC-condition mutation caused four tests to fail.

## Active installation

5. [-] Build and install cumulative display update.
   - Deployment worktree `/var/mnt/data/projects/personal/ESPSomfy-RTS/issue-15-details-power-deployment`, branch `codex/issue-15-details-power-deployment`, source `b1ad2e4`, preserves installed issue-23 source plus current UI. Combined build passed; 706 native tests plus one optional skip, 63 JS tests, renderer/runtime/capture/adapter and panel/radio/GPIO harnesses passed.
   - Fresh portable/settings/counter capture passed with idle shades. Full16MiB ROM read is in progress (tool session24081) under private pointer `~/.local/state/espsomfy-lilygo/LATEST_UI_DETAILS_POWER_INSTALL`; board is held in ROM, no flash written yet. Exact previous installed firmware/ELF retained. Next: finish/verify dump, layout/OTA selection, NVS/config/counters; boot old app, refresh state, then install. Firmware-only inactive-slot OTA; no filesystem/NVS/bootloader/table overwrite. Check enabled-controls header, battery reading, Wi-Fi/radio, configuration and uptime after reboot. Do not actuate shutters.

## Limits and tools

Host PIO uses `~/.cache/espsomfy-lilygo/pio-python/bin/pio`, build/libdeps under build/. Native/GFX harnesses use existing `rocm-7.2.3` container. Do not restore old rolling counters. Power readings are not yet hardware-validated; induced display fallback, physical control RF behavior and issue-23 movement reliability remain unverified.
