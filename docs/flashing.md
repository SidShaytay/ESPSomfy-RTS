# Update firmware without losing controller state

Use this procedure to back up an existing controller and update its firmware while preserving configuration, rolling codes and the previous application. A/B means two application slots: the updater writes the inactive one and selects it for the next boot.

The normal path is a firmware-only A/B update on the **same verified partition layout**. Initial deployment of the verified updater also requires its exact web-asset manifest; an older filesystem without it needs a separate state-preserving bootstrap plan, not a firmware-only upload. It does not update the web filesystem. First installation, a layout change, a board without a second app slot, or recovery from an unreachable application needs a separate board-specific USB plan.

Run the blocks in order in the same Bash session from the repository root. Stop on any failure; do not treat a partial capture as a recovery set. Tool setup requires `uv` and network access; it installs only into your user-owned cache.

## 1. Identify the board and preserve its features

Work within Sid's authorized service window. Backup/flashing and their required resets do not authorize shutter movement, RF tests, pairing, calibration or motor-limit changes. Coordinate any HA deployment/restart separately.

Record the physical board, source revision and firmware hash, build environment, partition table, running app slot, security state and installed features. Read the current protocol-2 model/settings and OTA capability; `hasOtaSlot` alone does not establish layout compatibility. Firmware version text can be shared by different source revisions.

Use the latest `main` as the completed integration baseline. New features start there and join `main` after review and tests, so subsequent firmware includes previous features. Do not assemble a deployment from unfinished parallel branches. Check that the selected image preserves the installed features:

| Existing LILYGO setup | PlatformIO environment |
| --- | --- |
| Interactive LCD and manual controls | `lilygo-t-embed-cc1101-plus-ui` |
| LCD UI with commands deliberately locked | `lilygo-t-embed-cc1101-plus-ui-preview` |
| Simple read-only status screen | `lilygo-t-embed-cc1101-plus-display` |
| Headless controller | `lilygo-t-embed-cc1101-plus` |

For other supported boards, use their exact environment from `platformio.ini`. Do not flash a headless build over an LCD installation accidentally.

Identify HA/HomeKit/MQTT automation writers and arrange a quiet period so state and counters cannot advance unnoticed during capture. Record any automation changes for restoration. Do not run a second controller with the same radio identities.

## 2. Capture a fresh recovery set

Store recovery files privately outside Git and shared storage. This Bash example creates a new directory without reusing an older backup:

```bash
set -euo pipefail
umask 077
recovery_parent="${XDG_STATE_HOME:-$HOME/.local/state}/espsomfy-lilygo"
mkdir -p "$recovery_parent"
recovery_dir="$(mktemp -d "$recovery_parent/recovery-XXXXXXXX")"
printf '%s\n' "$recovery_dir"
```

Capture a fresh portable `/backup` export, protocol model, all five settings sections, shade/group identities and membership, travel/tilt settings, primary and linked-remote counters, current app/partition information, and any LCD settings. Keep credentials and raw responses out of terminal transcripts and public reports. Save the current firmware binary and matching ELF when available.

**Take a full-flash backup by default.** For the verified 16 MB Plus, save exactly 16,777,216 bytes; other boards require their detected physical capacity. Use the stable `/dev/serial/by-id/` path after matching it to the intended board, and ensure no other process owns the port.

The established backup tool is esptool **4.12.0**, pinned in `tools/lilygo-backup-requirements.txt`. Prepare its separate environment if needed; these setup commands do not access a board:

```bash
set -euo pipefail
tool_cache="${ESPSOMFY_TOOL_CACHE:-$HOME/.cache/espsomfy-lilygo}"
if [[ ! -x "$tool_cache/backup-python/bin/python3" ]]; then
  uv venv --python 3.12.12 "$tool_cache/backup-python"
fi
uv pip sync --python "$tool_cache/backup-python/bin/python3" tools/lilygo-backup-requirements.txt
```

Its underscore-style options differ from newer esptool versions. With the approved board connected, identify it and leave it in ROM rather than booting the application between reads:

```bash
set -euo pipefail
backup_python="${ESPSOMFY_TOOL_CACHE:-$HOME/.cache/espsomfy-lilygo}/backup-python/bin/python3"
board_port='/dev/serial/by-id/<the-verified-board>'
"$backup_python" -m esptool version
"$backup_python" -m esptool --chip esp32s3 --port "$board_port" \
  --before default_reset --after no_reset chip_id > "$recovery_dir/rom-chip.log" 2>&1
"$backup_python" -m esptool --chip esp32s3 --port "$board_port" \
  --before no_reset --after no_reset flash_id > "$recovery_dir/rom-flash.log" 2>&1
"$backup_python" -m esptool --chip esp32s3 --port "$board_port" \
  --before no_reset --after no_reset get_security_info > "$recovery_dir/rom-security.log" 2>&1
```

Each command must succeed. Inspect the saved logs privately. Use `esp32s3` only for a verified S3 board. Compare ROM identity with the private device record, verify capacity, and check secure boot, flash encryption and download restrictions. If these differ from the known recovery setup, stop and revise the recovery method; do not change eFuses or use `--force`.

For the verified 16 MB Plus, capture the complete flash while it remains in ROM:

```bash
set -euo pipefail
"$backup_python" -m esptool --chip esp32s3 --port "$board_port" \
  --baud 921600 --before no_reset --after no_reset \
  read_flash 0x0 0x1000000 "$recovery_dir/configured-16mb.bin" > "$recovery_dir/read-flash.log" 2>&1
test "$(stat -c %s "$recovery_dir/configured-16mb.bin")" -eq 16777216
(cd "$recovery_dir" && sha256sum configured-16mb.bin > SHA256SUMS)
(cd "$recovery_dir" && sha256sum -c SHA256SUMS)
```

Use a baud rate already proven reliable for that board/cable. Check the partition-table MD5 and ranges, independently extract authoritative `shades.cfg` and NVS using compatible recovery tools, and compare identity/calibration/counters with the portable export. A checksum confirms file integrity; it does not prove the backup has current counters or that recovery will work. Preserve the original capture unchanged. See [issue #9](https://github.com/SidShaytay/ESPSomfy-RTS/issues/9) for the tested extraction and same-board recovery evidence.

An exception is allowed **only when Sid explicitly authorizes firmware-only A/B updating without a new full dump**. Even then, retain fresh portable/settings/counter captures, a verified unchanged layout, the current firmware/ELF or an independently verified active-slot image, and a usable recovery route. Confirm that the old active slot will remain intact. An earlier exception is not blanket permission for every future update.

## 3. Build and check the exact candidate

Use the pinned pioarduino Core 6.2.0 toolchain installed by `bash tools/setup-lilygo.sh`. From the chosen clean source worktree, this example builds the interactive LCD variant without accessing a device:

```bash
set -euo pipefail
pio_tool="${ESPSOMFY_TOOL_CACHE:-$HOME/.cache/espsomfy-lilygo}/pio-python/bin/pio"
build_environment=lilygo-t-embed-cc1101-plus-ui
"$pio_tool" run -e "$build_environment" -j 4
artifact_dir="$PWD/.pio/build/$build_environment"
sha256sum "$artifact_dir/firmware.bin" "$artifact_dir/firmware.elf" "$artifact_dir/partitions.bin"
```

If `PLATFORMIO_BUILD_DIR` is set, use that actual output directory instead. Record the clean Git revision, environment, relevant compiler defines, tool/dependency versions, firmware size and hashes. Keep the ELF for decoding a later crash. Run the relevant native, protocol, board/SPI and display/adapter checks before installation; [AGENTS.md](../AGENTS.md) and the [UI guide](lilygo-local-ui.md) list them. The headless `tools/build-lilygo.sh` packager must not label an LCD UI image.

Compare the candidate's decoded partition table with the board's verified table. Account for erased sector padding when comparing generated `partitions.bin` with a flash sector. Confirm the inactive app differs from the running app and the image fits it. An A/B upload cannot migrate partitions or create a second slot.

For a normal firmware update, the only upload candidate is the exact checked **`firmware.bin`**. Exclude `littlefs.bin`, `firmware.factory.bin`, merged/provisioning images, bootloader, partitions, OTA initialization data, raw backup and configuration exports. Do not use `upload`, `uploadfs` or `erase` targets as an alternative to the verified update path.

## 4. Upload firmware only

After ROM backup, boot the known current application and recheck live state/counters. If anything advanced, refresh the private capture rather than assuming the earlier snapshot is still current.

Use the authenticated local **firmware update** screen, which posts one file to `/updateFirmware`. The web picker expects a filename containing `.ino.` and ending in `.bin`; make a clearly named copy of the verified firmware when needed, and verify its SHA256 still matches. Use a trusted local connection and existing authentication; do not put credentials in a URL or follow a redirect with them.

Upload exactly that one file once. Do not choose **application/filesystem update** (`/updateApplication`) or **restore** (`/restore`). A successful firmware-only update changes the inactive application and normal OTA-selection metadata; NVS, LittleFS, bootloader, partition table and the old active application remain untouched.

The upload stops radio/MQTT operation and schedules a reboot. Keep stable power and retain the response and private diagnostics. A dropped HTTP connection is ambiguous: inspect the running build/slot before retrying. HTTP success alone does not establish that the intended firmware booted.

## 5. Verify the reboot without moving anything

Identify the new boot by its exact firmware image/ELF identity, app slot and restarted uptime, not just the version label. Allow the updater's delayed reboot; exclude an early response from the old application.

For verified GitHub-update profiles, a pending image must pass 60 continuous seconds of the [stronger boot-health checks](github-updates.md#preserve-controller-state-and-recovery), including matching web assets and bootloader, expected radio availability, responsive loop servicing and internal-memory headroom. Legacy profiles confirm once uptime is at least 60,000 ms with the network connected. Neither policy proves physical RF or motor behavior. Verify the confirmation result and stable uptime, then observe longer for unexpected restarts, watchdogs or memory loss.

Compare saved and current configuration, identities, membership, calibration and counters. Investigate differences; do not overwrite newer counters to force equality. Verify protocol synchronization and the expected schema capability, radio initialization/register/RX status, Wi-Fi/listener availability, and read-only HA entity availability. For LCD builds also check command-lock status and framebuffer/fallback health. These checks establish software state, not physical motor movement or RF reliability.

Firmware and HA must both support an added operation before HA can use it. If an update requires a matching integration change, use the separately authorized HA update/restart procedure, retain its rollback files, and verify the operation in the advertised schema. Do not issue HA services or use Apple Home as a connectivity probe.

Restore any paused writers only after verification, recording their prior state. Refresh recovery state after subsequent real commands. Record candidate/previous hashes, slots, comparisons, confirmation and untested items in the task/deployment record; keep raw evidence private.

## If installation or verification fails

Stop retries and preserve the failure evidence. An interrupted upload may have rebooted the old application; first identify what actually runs. Do not assume every later failure automatically rolls back: rollback depends on image state and bootloader, and a confirmed image is already accepted.

If the application remains reachable, returning to the retained known-good firmware through the same verified firmware-only A/B path is preferable to restoring configuration. Capture the latest counters first. Re-uploading old firmware may replace the other app slot, so record the new slot plan before writing.

If neither application is reachable, identify the board in ROM and inspect current flash before selecting a USB recovery plan. Prefer an explicitly reviewed firmware/boot-selection repair that preserves current state. A raw full-image restore also restores stored counters and layout; it is unsafe unchanged after counters advance. Reconcile fresh primary and linked-remote counters, preserve authoritative configuration, and specify every write range, preserved range and preboot readback comparison before using the [tested recovery procedure](https://github.com/SidShaytay/ESPSomfy-RTS/issues/9).

Do not erase the whole chip, flash an arbitrary factory image, rewrite eFuses, lower counters, regenerate identities, re-pair, recalibrate or change motor limits to hide an update failure. A different layout, inaccessible current counters or different security settings requires a revised recovery plan rather than guessed addresses or increments.
