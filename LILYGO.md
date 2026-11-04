# LILYGO T-Embed CC1101 Plus

This profile runs ESPSomfy RTS headlessly on the Plus external-antenna board, using the existing Somfy decoder, transmit timing, rolling codes, persistence, and web interface.

## Build the firmware

Use Bash for these commands, starting in the repository root. Requires Linux x86_64, `uv`, `curl`, Git, and USB serial access; no host package layering is needed on Fedora Silverblue.

```bash
bash tools/setup-lilygo.sh
bash tools/build-lilygo.sh
```

Setup pins Arduino CLI 1.3.1, ESP32 Arduino core 2.0.17, ArduinoJson 6.21.5, PubSubClient 2.8.0, SmartRC-CC1101-Driver-Lib 2.5.7, WebSockets 2.4.0, esptool 4.7, and mklittlefs 3.1.0/LittleFS 2.5.1. A separate esptool 4.12 environment handles backups to avoid [slow native USB/JTAG reads](https://github.com/espressif/esptool/issues/936). Downloads and Python helpers stay in `~/.cache/espsomfy-lilygo/`; override with `ESPSOMFY_TOOL_CACHE`.

The build selects `ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS`, ESP32-S3, QIO/80 MHz flash, 16 MB flash, OPI PSRAM, and hardware USB CDC/JTAG. Generic builds retain their existing defaults. Artifacts and `flash-manifest.json` containing offsets, sizes, source hashes, and artifact SHA256 values appear in `build/lilygo/artifacts/`; build output is `build/lilygo/build.log`.

For host GPIO checks, run `CXX=g++ bash tools/test-board-profile.sh` in an environment with a C++ compiler. Checks cover power/CS modes and ordering, invalid bands, stuck-MISO recovery, generic behavior, and non-S3 rejection.

## Board wiring and startup

| Function | GPIO / setting |
|---|---|
| Peripheral power | 15 HIGH |
| CC1101 SCK / MOSI / MISO | 11 / 9 / 10 |
| CC1101 CS / GDO0 TX / GDO2 RX | 12 / 3 / 38 |
| RF SW1 / SW0 | 47 HIGH / 48 HIGH for 433 MHz path |
| Display CS / SD CS / nRF24 CS | 41 HIGH / 13 HIGH / 44 HIGH |
| nRF24 CE / display backlight | 43 LOW / 21 LOW |

The profile deselects all shared-SPI devices before enabling peripheral power, selects the RF filter, then initializes CC1101. Radio pins are fixed against saved/UI overrides; board control pins, USB19/20 and flash/OPI PSRAM26–37 are reserved against relay assignments. The synthesizer defaults to **433.42 MHz OOK**, independently of the filter's 433/434 MHz designation. The display, SD, and nRF24 drivers are not started. UART0 shares GPIO43/44 with nRF24, so diagnostics use USB CDC.

Startup reports flash/PSRAM size, switches, CC1101 PARTNUM/VERSION, OOK/asynchronous register configuration, frequency readback, and RX MARCSTATE. Valid decoded frames log address, command, rolling code, protocol, and RSSI. Register success alone does not prove RF operation. A physical PCB revision cannot be inferred from MCU identification alone.

## Identify, back up, and flash only this device

Select the stable by-id path from `ls -l /dev/serial/by-id/`; substitute the purchased LILYGO's path below. Confirm esptool reports ESP32-S3, **16 MB**, and the previously recorded MAC before every write. Close serial monitors first.

```bash
LILYGO_REPO="$PWD"
LILYGO_CACHE="${ESPSOMFY_TOOL_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/espsomfy-lilygo}"
LILYGO_PY="$LILYGO_CACHE/build-python/bin/python3"
LILYGO_BACKUP_PY="$LILYGO_CACHE/backup-python/bin/python3"
LILYGO_PORT='/dev/serial/by-id/<identified-LILYGO-device>'
"$LILYGO_PY" -m esptool --chip esp32s3 --port "$LILYGO_PORT" chip_id
"$LILYGO_PY" -m esptool --chip esp32s3 --port "$LILYGO_PORT" flash_id
```

Preserve the full factory image outside Git before flashing. This bring-up uses `~/.local/state/espsomfy-lilygo/factory-2026-11-03/factory-16mb.bin` with `SHA256SUMS`; a backup is valid only after the complete read and checksum succeed. Do not overwrite an existing backup.

```bash
LILYGO_BACKUP="$HOME/.local/state/espsomfy-lilygo/factory-2026-11-03"
mkdir -p "$LILYGO_BACKUP"
chmod 700 "$LILYGO_BACKUP"
cd "$LILYGO_BACKUP"
test ! -e factory-16mb.bin || { echo "Keep the existing backup; do not overwrite it."; exit 1; }
"$LILYGO_BACKUP_PY" -m esptool --chip esp32s3 --port "$LILYGO_PORT" --baud 921600 read_flash 0x0 0x1000000 factory-16mb.bin
sha256sum factory-16mb.bin > SHA256SUMS
sha256sum -c SHA256SUMS
```

Retain the existing 4 MB OTA layout on the 16 MB device: NVS at `0x9000`, two `0x140000` application slots at `0x10000`/`0x150000`, and LittleFS at `0x290000` with size `1441792` (`0x160000`). Remaining flash is unallocated. Keep these offsets compatible with ESPSomfy's updater.

For initial provisioning only, check the manifest's SHA256 values against the files, then flash these individual segments. This preserves NVS and avoids a whole-device erase. A fresh filesystem replaces `shades.cfg`; do not repeat this five-segment command after pairing.

```bash
cd "$LILYGO_REPO/build/lilygo/artifacts"
"$LILYGO_PY" -m esptool --chip esp32s3 --port "$LILYGO_PORT" --baud 460800 write_flash --flash_mode keep --flash_size 16MB \
  0x0 SomfyController.ino.bootloader.bin \
  0x8000 SomfyController.ino.partitions.bin \
  0xe000 boot_app0.bin \
  0x10000 SomfyController.ino.bin \
  0x290000 SomfyController.littlefs.bin
"$LILYGO_PY" -m serial.tools.miniterm "$LILYGO_PORT" 115200
```

Exit miniterm with Ctrl+]. Firmware writes erase only their destination sectors. Do not run `erase_flash`. For subsequent updates, upload the custom `SomfyController.ino.bin` through the controller's firmware-upload UI and preserve its filesystem. Export controller settings before any filesystem replacement. Stock online firmware updates lack this board profile; use custom builds exclusively.

## Recover factory firmware

Recheck chip, flash size, and MAC against the same board, then verify `SHA256SUMS`. Restoring the full image replaces controller configuration and returns the backed-up factory state. Never restore an old controller snapshot after pairing: its rolling codes may be stale.

```bash
cd "$LILYGO_BACKUP"
sha256sum -c SHA256SUMS
"$LILYGO_PY" -m esptool --chip esp32s3 --port "$LILYGO_PORT" --baud 460800 write_flash --flash_mode keep --flash_size 16MB 0x0 factory-16mb.bin
```

If USB does not enumerate, hold the encoder BOOT button while resetting, release BOOT, and identify the ROM USB port again before recovery.

## Receive first, then pair with the user present

Follow the [upstream configuration guide](https://github.com/rstrouse/ESPSomfy-RTS/wiki/Configuring-the-Software). Join the default hotspot `ESPSomfyRTS` (or configured hostname), open `http://192.168.4.1`, and save Network Wi-Fi settings. Disconnect from its AP, then use its LAN address. Keep credentials private.

A **jog** is the motor’s brief up/down twitch acknowledging programming. Someone must watch the shutter during pairing; release rear PROG immediately at the first twitch.

1. Open Radio → Logs. On one Telis channel, briefly press MY with the shutter safely visible. Check decoded frames and rolling-code progression across separate presses. Stop for the user's button press; never simulate rolling codes.
2. Under Somfy → Shades, add one shade using observed protocol/bit length and a new controller address. Do not clone the Telis address.
3. With the user present, open Pair Shade. Hold rear PROG until the first motor jog, then release. Click the wizard's PROG once and confirm another jog. Stop if a jog is absent: repeating PROG can undo pairing. Never reset the motor or change limits.
4. Use Link Remote with that Telis channel. Verify UP/DOWN/STOP and existing-remote operation. Reboot; check configuration and rolling-code continuity. Record actual movement separately from transmitted frames.

New shades start with 10-second software travel estimates. Measure and set the actual up/down travel times before percentage-position automations; these estimates do not change motor end limits.

## Back up the configured controller

Keep backups outside Git in a private directory. The original factory image does not contain later pairing, Wi-Fi or calibration settings.

| Backup | Includes | Excludes |
|---|---|---|
| Existing `/backup` export, format 24 | Shade/group identities, membership, pairing flags, rolling counters, travel times, linked remote addresses, radio configuration, general settings and non-secret network/MQTT settings | Wi-Fi SSID/password, MQTT/security credentials, firmware, raw NVS and linked remotes' received counters |
| Raw NVS, offset `0x9000`, length `0x5000` | Persistent credentials, radio settings, identity/counter preferences and other NVS state | LittleFS shade/group configuration and firmware |
| Full 16 MB flash image | NVS, LittleFS, bootloader, partitions and both application slots | Motor-side memory and subsequent counter changes |

Coverage follows `ShadeConfigFile::backup`, `writeNetRecord`, `writeTransRecord` and the NVS readers. An export alone cannot recover Wi-Fi credentials on replacement hardware.

First capture settings using the controller's actual LAN address:

```bash
umask 077
LILYGO_URL='http://<controller-address>'
LILYGO_CONFIGURED="$HOME/.local/state/espsomfy-lilygo/configured-$(date -u +%Y%m%dT%H%M%SZ)"
mkdir -p "$LILYGO_CONFIGURED"
curl --fail --show-error "$LILYGO_URL/backup" --output "$LILYGO_CONFIGURED/controller.backup"
curl --fail --show-error "$LILYGO_URL/shades" --output "$LILYGO_CONFIGURED/shades.json"
curl --fail --show-error "$LILYGO_URL/groups" --output "$LILYGO_CONFIGURED/groups.json"
curl --fail --show-error "$LILYGO_URL/getRadio" --output "$LILYGO_CONFIGURED/radio.json"
```

For complete recovery, reconnect the identified board's data USB cable, close monitors and verify its chip/flash identity as above. With shutters stationary and no automations issuing commands, read the full flash. This restarts the controller but does not erase flash or send motor commands. Use `LILYGO_BACKUP_PY` and `LILYGO_PORT` from the identification section.

```bash
test ! -e "$LILYGO_CONFIGURED/configured-16mb.bin" || exit 1
"$LILYGO_BACKUP_PY" -m esptool --chip esp32s3 --port "$LILYGO_PORT" --baud 921600 \
  read_flash 0x0 0x1000000 "$LILYGO_CONFIGURED/configured-16mb.bin"
python3 - "$LILYGO_CONFIGURED" <<'PY'
import pathlib, sys
folder = pathlib.Path(sys.argv[1])
image = (folder / 'configured-16mb.bin').read_bytes()
assert len(image) == 0x1000000
(folder / 'nvs.bin').write_bytes(image[0x9000:0xe000])
(folder / 'littlefs.bin').write_bytes(image[0x290000:0x3f0000])
PY
cd "$LILYGO_CONFIGURED"
sha256sum controller.backup shades.json groups.json radio.json configured-16mb.bin nvs.bin littlefs.bin > SHA256SUMS
sha256sum -c SHA256SUMS
```

Retain compatible custom firmware, partition binary and flash manifest alongside the backup. Inspect export record sizes and compare identities, counters, calibration and radio values with the JSON snapshots. Keep credentials and identifying addresses private. A size/checksum check proves file integrity, not a successful restore.

### Restore without losing identities

On the original controller with intact NVS, take a fresh export, then use the existing configuration-restore UI and select the required shade/group, radio and settings sections. The loader takes the higher of exported and existing NVS rolling counters for both shades and groups. This protection depends on retaining current NVS.

On a compatible replacement Plus board, install the same custom profile/partitions, reconnect Wi-Fi and restore the export without generating new shade/group identities. Alternatively, use the full-flash recovery command above with the configured image to restore credentials too, provided its counters are still current. Raw NVS alone is not a complete restore. Do not operate original and replacement simultaneously with duplicated identities.

Commands sent after a snapshot make its counters stale. A full-flash restore can roll counters backward, and a replacement has no newer NVS to protect it. Do not guess counter increments or reset motor limits. Recover latest counters from the original if available; otherwise use documented, observed user-assisted recovery/pairing. Motor memory exists independently of the UI paired flag.

Refresh the export after configuration/calibration changes and before firmware updates; refresh full-flash/NVS before destructive recovery and record subsequent use. Firmware-only updates preserve NVS/LittleFS; filesystem replacement/full provisioning does not. After restoration, verify radio, network, both shutters, old remote and reboot persistence. A destructive restore has not been tested on the sole working board.

## Prepare local Home Assistant and Apple Home

After RF acceptance, install [ESPSomfy RTS-HA](https://github.com/rstrouse/ESPSomfy-RTS-HA) through HACS as an integration custom repository. Restart Home Assistant if requested, then configure the discovered controller under Settings → Devices & services. Verify the cover controls locally.

Add [HomeKit Bridge](https://www.home-assistant.io/integrations/homekit/) through the same settings, include only the validated shutter entities, and pair its QR code in Apple Home → Add Accessory. This control path uses the local network; cloud access is unnecessary for shutter operation. Installation/downloads and optional update checks use the Internet. These are preparation instructions, not evidence that either integration has been configured.

The configured controller has two individual shades, **Master Bedroom** and **Master Bathroom**, and a native RF group, **Both rooms**. Prefer Both rooms for everyday combined UP/DOWN/STOP; its single RF command reaches both motors. The integration supports individual and group covers. After installation, identify all three actual `cover.*` entity IDs in HA rather than assuming generated IDs, then select those entities in HomeKit Bridge's inclusion filter. Apple Home pairing requires the user's iPhone and the bridge's QR code.

Home Assistant setup was subsequently completed on Luna through authorized SSH access, using the existing admin session and HACS APIs. ESPSomfy RTS v2.4.7 is installed and its config entry is loaded. The lilygo device is assigned to Master Bedroom; individual cover areas are Master Bedroom and Master Bathroom & Closet.

Verified entities (state reads only):

- `cover.espsomfyrts_master_bedroom`
- `cover.espsomfyrts_master_bathroom`
- `cover.espsomfyrts_both_rooms`

All three reported closed. The existing HomeKit Bridge was updated through its options flow to include these three entities while retaining the existing climate-domain inclusion and paired clients. Both integrations remained loaded; no Home Assistant restart, new motor commands or control tests were performed. Existing shutter/group rolling counters were unchanged throughout setup. Appearance/control in Apple Home has not been observed on an iPhone.

The stock firmware-update entity was disabled in HA because installing stock firmware would lose this custom board profile. Use the documented custom firmware path. A DHCP reservation for the controller is recommended for stable addressing but was deferred: gateway SSH authentication was unavailable. No gateway configuration changed. Do not run motor tests or create motion automations until the user explicitly resumes physical verification.

## Observed shutter setup, 2026-11-03

Both individual identities and Both rooms were paired with user-confirmed first/second jogs. User-observed UP/DOWN/mid-travel STOP passed individually and together. The old remote still controls both together and each independently. Configuration/radio/rolling counters survived a restart unchanged, followed by observed group movement/STOP.

| Shutter | Full closing travel | Full opening travel |
|---|---:|---:|
| Master Bathroom | approximately 19.30 s | approximately 21.15 s |
| Master Bedroom | approximately 22.10 s | approximately 23.75 s |

These manually timed values were saved and read back. Bedroom times were calculated from its additional 2.8 s closing and 2.6 s opening relative to Bathroom. They measure first arrival at the endpoint and exclude subsequent movement. Percentage accuracy and reboot persistence after saving the new calibration have not been tested. RTS supplies no measured position feedback; software estimates can disagree with physical position. In particular, the existing MY handler can suppress transmission if it thinks the shutter is idle and no favorite is configured.

Bedroom has a pre-existing intermittent upper-end fault: reaches the top, lowers slightly and rises repeatedly. The user confirms it is unrelated to this port. A [matching Somfy support report](https://forum.somfy.fr/questions/960717-lt50-instable-position-haute-pourtant-fin-course-bien-regle) suggests a motor brake issue; this is not a diagnosis without identifying the motor. Check model, gravity drift versus powered reversal and warranty before parts. No limits or motor resets were changed.

A private format-24 export was captured after calibration and the bedtime group DOWN command, with record lengths, identities, travel values and SHA256 verified. Raw NVS/full-flash capture is pending data USB reconnection; no serial device was present on Vega. The export omits Wi-Fi credentials. Keep issue #7 open at the user's request and log subsequent calibration there. Raw backup follow-up remains tracked in #9; HA/HomeKit setup evidence is recorded in #8.

## Hardware evidence and issue tracking

Pinned manufacturer reference: [utilities.h](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/c36826e0f91311cb591c4545fccf91e254fc55dc/examples/utilities.h), [power/SPI ordering example](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/c36826e0f91311cb591c4545fccf91e254fc55dc/examples/cc1101_recv_irq/cc1101_recv_irq.ino), and [RF switch schematic](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/c36826e0f91311cb591c4545fccf91e254fc55dc/hardware/CC1101%2020250206%20TO%20XY.pdf). [Plus product information](https://lilygo.cc/products/t-embed-cc1101-plus) identifies the added nRF24.

Upstream [#598](https://github.com/rstrouse/ESPSomfy-RTS/issues/598) describes SPI success without reception; missing antenna-path selection is a reported cause, not a verified complete fix. [#666](https://github.com/rstrouse/ESPSomfy-RTS/issues/666) describes initialization failure on another revision without a resolution. [SmartRC v2.5.7](https://github.com/LSatan/SmartRC-CC1101-Driver-Lib/blob/b8c6af4c7c2214cd77a4e9b2e2cb37b24b393605/ELECHOUSE_CC1101_SRC_DRV.cpp) already configures GDO2 for asynchronous RX; its unbounded MISO waits make power and SPI deselection essential.

Execution order/status lives in [parent issue #1](https://github.com/SidShaytay/ESPSomfy-RTS/issues/1), with ordered sub-issues [hardware #2](https://github.com/SidShaytay/ESPSomfy-RTS/issues/2), [profile #3](https://github.com/SidShaytay/ESPSomfy-RTS/issues/3), [build #4](https://github.com/SidShaytay/ESPSomfy-RTS/issues/4), [flash #5](https://github.com/SidShaytay/ESPSomfy-RTS/issues/5), [receive #6](https://github.com/SidShaytay/ESPSomfy-RTS/issues/6), [shutter #7](https://github.com/SidShaytay/ESPSomfy-RTS/issues/7), and [automation #8](https://github.com/SidShaytay/ESPSomfy-RTS/issues/8).

## Bring-up results

| Check | Result |
|---|---|
| Firmware/LittleFS build | PASS: application binary 1,181,760 bytes; filesystem 1,441,792 bytes; manifest hashes verified. Generic ESP32/C3/S2/S3 compile. |
| Factory backup/recovery image | PASS: full 16,777,216-byte read and SHA256 verification; stored privately outside Git. Recovery command documented, not executed. |
| Hardware boot/registers/433.42 MHz/RX state | PASS: ESP32-S3, 16 MB flash, approximately 8 MB usable PSRAM; filesystem mounts; CC1101 PARTNUM=0x00 VERSION=0x14, IOCFG0/2=0x0D, PKTCTRL0=0x32, frequency=433.420 MHz, MARCSTATE=0x0D, switches=1/1. |
| Physical Telis receive and decode | PASS: five valid 56-bit RTS frames (UP/UP/MY/DOWN/MY), consecutive rolling codes, RSSI -82 to -75 dBm. User confirms channel 1 moved/stopped using physical remote. |
| RF transmission | PASS: independent shade and group pairing/control, confirmed by actual motor response. |
| Actual shutter movement / physical remote | PASS: independent and combined UP/DOWN/STOP; old remote retained. |
| Wi-Fi/configuration/rolling-code persistence | PASS: pre/post restart API snapshots matched, followed by observed group DOWN/STOP; new calibration not separately reboot-tested. |
| Configured backup | Partial: native export/checksum and configuration snapshots verified; raw NVS/full flash require USB reconnection. No destructive restore performed. |
| Home Assistant / Apple Home | HA integration and existing paired HomeKit Bridge configured; 3 closed covers verified through state reads. iPhone appearance/control not observed. No motor commands sent. |
