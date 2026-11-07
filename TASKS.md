# Task Memory

## Context

- Objective: implement/install the approved 320×170 phase-2 LCD UI for #15, including PSRAM graphics, screenshots and bedroom brightness.
- Workspace: `/var/mnt/data/projects/personal/ESPSomfy-RTS/issue-15-phase2-ui`, branch `codex/issue-15-phase2-ui`. The phase-1 source remains in sibling `issue-15-display`.
- Authority: Sid authorized board updates after suitable backup and firmware-only A/B OTA. **Sid clarified that he may operate the shutters manually through the UI; the assistant must not send shutter/RF/motor commands.** Sid explicitly requested flashing with physical controls enabled. The canonical `lilygo-t-embed-cc1101-plus-ui` now defines `ESPSOMFY_LOCAL_COMMANDS`; the separate `-ui-preview` remains command-locked. Current work fixes TWO03 half-step decoding/direction, adds GPIO6 Back, and adds saved dim/off timeout choices including zero = No dim/No sleep.

## Results

1. [x] Implemented and installed the command-locked preview.
   - Source commits: `495db0a` safe state/fallback, `67aea60` renderer, `78f5a61` PSRAM runtime, `ea7c18e` screenshots, `1664cf1` current group names. Detailed design and evidence: `docs/lilygo-local-ui.md`.
   - Fresh portable/protocol/settings/counter backup; verified standard layout; firmware-only A/B OTA retained the previous active phase-1 slot and firmware/ELF. New full-flash dump omitted under Sid's explicit A/B exception. Private evidence pointer: `~/.local/state/espsomfy-lilygo/LATEST_UI_INSTALL`.
   - Actual framebuffer PNGs retrieved with local command lock. Nineteen status comparisons over ten minutes, unchanged counters/all five settings sections, no resets; final uptime 662.336 s. Web/HA available, radio initialization/RX ready at 433.42 MHz. No shutter requests or HA services sent.
   - Graphics allocated 108,800 bytes in PSRAM; internal transfer buffer 2,560 bytes. Full-frame paint 6,469 µs, SPI lock 1,489 µs. Idle capture PWM 0. Primary graphics → simple status with PSRAM failure cause → blank if fallback also fails.
   - Saved brightness feature; dim 10 s/off 20 s idle; estimated motion keeps controls awake. Manufacturer documents no ambient-light sensor. Local commands remain locked regardless of encoder input.
   - Checks: native 651 passed plus one existing optional private-backup skip; JS 62, manifest 24, image codec 2, real GFX/runtime/capture and board/SPI harnesses passed; UI/simple/headless/generic firmware builds passed. Temporary armed adapter compiled offline only.

2. [x] Installed the authorized input/settings fixes with manual controls enabled.
   - Source `7061e98`: TWO03 two-transition detents, clockwise down, GPIO6 Back, saved independent dim/sleep settings including No dim/No sleep. Separate `-ui-preview` remains locked.
   - Fresh portable backup and all five settings sections captured. Verified partition layout, then firmware-only inactive-slot OTA; retained previous preview binary/ELF and rollback slot. Private evidence: `~/.local/state/espsomfy-lilygo/LATEST_UI_INPUT_FIX`.
   - Actual framebuffer header confirms `X-LCD-Commands: enabled`. Post-reboot read-only checks found identical entity state/counters and settings, connected Wi-Fi, radio ready at 433.42 MHz, and all three HA covers available/idle. Uptime progressed beyond the 60-second OTA confirmation window without a restart. No assistant movement commands or HA services sent.
   - Checks: 655 native passes plus one optional skip; real GFX/runtime/capture and GPIO/SPI harnesses passed; UI/simple/headless builds passed. Graphics use 108,800 bytes PSRAM; measured paint 6,470 µs and SPI lock 1,491 µs.
   - Next: Sid can manually verify revised wheel direction/detents, Back, controls, and saved display timers. No assistant actuation is authorized. No further installation or approval needed.

## Reproduction and limits

Use `git-agent` / `gh-agent`; GitHub must be `r-sidshaytay`. No upstream publication or issue closure. Host builds use pinned `~/.cache/espsomfy-lilygo/pio-python/bin/pio`, with build/libdeps under `build/`; native C++ checks use the existing `rocm-7.2.3` container. Task-owned USB/status observers were stopped; no background task is required. Power telemetry, induced fallback hardware faults and on-air responsiveness remain unverified. Do not restore stale counters or whole raw images for routine firmware rollback.
