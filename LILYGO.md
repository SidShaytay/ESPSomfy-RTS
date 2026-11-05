# LILYGO T-Embed CC1101 Plus

This profile runs ESPSomfy RTS headlessly on the Plus external-antenna board, using the existing Somfy decoder, transmit timing, rolling codes, persistence, and web interface.

## Build the firmware

Use Bash for these commands, starting in the repository root. Requires Linux x86_64, `uv`, `curl`, Git, and USB serial access; no host package layering is needed on Fedora Silverblue.

```bash
bash tools/setup-lilygo.sh
bash tools/build-lilygo.sh
```

Setup pins Arduino CLI 1.3.1, ESP32 Arduino core 2.0.17, ArduinoJson 6.21.5, PubSubClient 2.8.0, SmartRC-CC1101-Driver-Lib 2.5.7, WebSockets 2.4.0, esptool 4.7, and mklittlefs 3.1.0/LittleFS 2.5.1. Downloads and Python helpers stay in `~/.cache/espsomfy-lilygo/`; override with `ESPSOMFY_TOOL_CACHE`.

The build selects `ESPSOMFY_LILYGO_T_EMBED_CC1101_PLUS`, ESP32-S3, QIO/80 MHz flash, 16 MB flash, OPI PSRAM, and hardware USB CDC/JTAG. Generic builds retain their existing defaults. Artifacts and `flash-manifest.json` containing offsets, sizes, source hashes, and artifact SHA256 values appear in `build/lilygo/artifacts/`; build output is `build/lilygo/build.log`.

## Board wiring and startup

| Function | GPIO / setting |
|---|---|
| Peripheral power | 15 HIGH |
| CC1101 SCK / MOSI / MISO | 11 / 9 / 10 |
| CC1101 CS / GDO0 TX / GDO2 RX | 12 / 3 / 38 |
| RF SW1 / SW0 | 47 HIGH / 48 HIGH for 433 MHz path |
| Display CS / SD CS / nRF24 CS | 41 HIGH / 13 HIGH / 44 HIGH |
| nRF24 CE / display backlight | 43 LOW / 21 LOW |

The profile deselects all shared-SPI devices before enabling peripheral power, selects the RF filter, then initializes CC1101. Radio pins are fixed against saved/UI overrides; board control pins are reserved against relay assignments. The synthesizer defaults to **433.42 MHz OOK**, independently of the filter's 433/434 MHz designation. The display, SD, and nRF24 drivers are not started. UART0 shares GPIO43/44 with nRF24, so diagnostics use USB CDC.

Startup reports flash/PSRAM size, switches, CC1101 PARTNUM/VERSION, OOK/asynchronous register configuration, frequency readback, and RX MARCSTATE. Valid decoded frames log address, command, rolling code, protocol, and RSSI. Register success alone does not prove RF operation. A physical PCB revision cannot be inferred from MCU identification alone.

## Identify, back up, and flash only this device

Select the stable by-id path from `ls -l /dev/serial/by-id/`; substitute the purchased LILYGO's path below. Confirm esptool reports ESP32-S3, **16 MB**, and the previously recorded MAC before every write. Close serial monitors first.

```bash
LILYGO_REPO="$PWD"
LILYGO_CACHE="${ESPSOMFY_TOOL_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/espsomfy-lilygo}"
LILYGO_PY="$LILYGO_CACHE/build-python/bin/python3"
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
"$LILYGO_PY" -m esptool --chip esp32s3 --port "$LILYGO_PORT" --baud 460800 read_flash 0x0 0x1000000 factory-16mb.bin
sha256sum factory-16mb.bin > SHA256SUMS
sha256sum -c SHA256SUMS
```

Retain the existing 4 MB OTA layout on the 16 MB device: NVS at `0x9000`, two `0x140000` application slots at `0x10000`/`0x150000`, and LittleFS at `0x290000` with size `1441792` (`0x160000`). Remaining flash is unallocated. Keep these offsets compatible with ESPSomfy's updater.

After checking the manifest's SHA256 values against the files, flash these individual segments; this preserves NVS and avoids a whole-device erase:

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

Exit miniterm with Ctrl+]. Firmware writes erase only their destination sectors. Do not run `erase_flash`. Preserve your custom profile when updating; stock upstream binaries lack its board initialization.

## Recover factory firmware

Recheck chip, flash size, and MAC against the same board, then verify `SHA256SUMS`. Restoring the full image replaces controller configuration and returns the backed-up factory state. Never restore an old controller snapshot after pairing: its rolling codes may be stale.

```bash
cd "$LILYGO_BACKUP"
sha256sum -c SHA256SUMS
"$LILYGO_PY" -m esptool --chip esp32s3 --port "$LILYGO_PORT" --baud 460800 write_flash --flash_mode keep --flash_size 16MB 0x0 factory-16mb.bin
```

If USB does not enumerate, hold the encoder BOOT button while resetting, release BOOT, and identify the ROM USB port again before recovery.

## Receive first, then pair with the user present

Follow the [upstream configuration guide](https://github.com/rstrouse/ESPSomfy-RTS/wiki/Configuring-the-Software). Join `ESPSomfy RTS`, open `http://192.168.4.1`, and save Network Wi-Fi settings. Disconnect from its AP, then use its LAN address. Keep credentials private.

1. Open Radio → Logs. On one Telis channel, briefly press MY with the shutter safely visible. Check decoded frames and rolling-code progression across separate presses. Stop for the user's button press; never simulate rolling codes.
2. Under Somfy → Shades, add one shade using observed protocol/bit length and a new controller address. Do not clone the Telis address.
3. With the user present, open Pair Shade. Hold rear PROG until the first motor jog, then release. Click the wizard's PROG once and confirm another jog. Stop if a jog is absent: repeating PROG can undo pairing. Never reset the motor or change limits.
4. Use Link Remote with that Telis channel. Verify UP/DOWN/STOP and existing-remote operation. Reboot; check configuration and rolling-code continuity. Record actual movement separately from transmitted frames.

## Prepare local Home Assistant and Apple Home

After RF acceptance, install [ESPSomfy RTS-HA](https://github.com/rstrouse/ESPSomfy-RTS-HA) through HACS as an integration custom repository. Restart Home Assistant if requested, then configure the discovered controller under Settings → Devices & services. Verify the cover controls locally.

Add [HomeKit Bridge](https://www.home-assistant.io/integrations/homekit/) through the same settings, include only the validated shutter entities, and pair its QR code in Apple Home → Add Accessory. This control path uses the local network; cloud access is unnecessary for shutter operation. Installation/downloads and optional update checks use the Internet. These are preparation instructions, not evidence that either integration has been configured.

## Hardware evidence and issue tracking

Pinned manufacturer reference: [utilities.h](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/c36826e0f91311cb591c4545fccf91e254fc55dc/examples/utilities.h), [power/SPI ordering example](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/c36826e0f91311cb591c4545fccf91e254fc55dc/examples/cc1101_recv_irq/cc1101_recv_irq.ino), and [RF switch schematic](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/c36826e0f91311cb591c4545fccf91e254fc55dc/hardware/CC1101%2020250206%20TO%20XY.pdf). [Plus product information](https://lilygo.cc/products/t-embed-cc1101-plus) identifies the added nRF24.

Upstream [#598](https://github.com/rstrouse/ESPSomfy-RTS/issues/598) describes SPI success without reception; missing antenna-path selection is a reported cause, not a verified complete fix. [#666](https://github.com/rstrouse/ESPSomfy-RTS/issues/666) describes initialization failure on another revision without a resolution. [SmartRC v2.5.7](https://github.com/LSatan/SmartRC-CC1101-Driver-Lib/blob/b8c6af4c7c2214cd77a4e9b2e2cb37b24b393605/ELECHOUSE_CC1101_SRC_DRV.cpp) already configures GDO2 for asynchronous RX; its unbounded MISO waits make power and SPI deselection essential.

Execution order/status lives in [parent issue #1](https://github.com/SidShaytay/ESPSomfy-RTS/issues/1), with ordered sub-issues [hardware #2](https://github.com/SidShaytay/ESPSomfy-RTS/issues/2), [profile #3](https://github.com/SidShaytay/ESPSomfy-RTS/issues/3), [build #4](https://github.com/SidShaytay/ESPSomfy-RTS/issues/4), [flash #5](https://github.com/SidShaytay/ESPSomfy-RTS/issues/5), [receive #6](https://github.com/SidShaytay/ESPSomfy-RTS/issues/6), [shutter #7](https://github.com/SidShaytay/ESPSomfy-RTS/issues/7), and [automation #8](https://github.com/SidShaytay/ESPSomfy-RTS/issues/8).

## Bring-up results

| Check | Result |
|---|---|
| Firmware/LittleFS build | Pending final build evidence |
| Factory backup/recovery image | Pending completed read and checksum |
| Hardware boot/registers/433.42 MHz/RX state | Pending flashed-device evidence |
| Physical Telis receive and decode | Pending user button press |
| RF transmission | Pending user-assisted pairing |
| Actual shutter movement / physical remote | Pending observed UP/DOWN/STOP |
| Wi-Fi/configuration/rolling-code persistence | Pending reboot checks |
| Home Assistant / Apple Home | Prepared instructions; not verified |
