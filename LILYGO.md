# LILYGO T-Embed CC1101 and CC1101 Plus

For backup, firmware-only A/B installation, verification and recovery, follow [the standard flashing procedure](docs/flashing.md). Use the shared `lilygo-t-embed-cc1101` firmware for either CC1101 or CC1101 Plus; it includes display, interactive UI and local commands.

This branch uses cjkas v4.0.2, pinned at `07bffdec6b540cc99f31d02d7e6de6d3d1e7f8f3`. The standard-layout build at revision `6842f0a22a334c849ab1c5ed11e6c474c6666aed` was installed on the original controller on November 6, 2026. Both runtime boots passed read-only checks, with credentials, identities, calibration and counters preserved. The cjkas versus xkain decision remains open; see the [installed-layout verification](docs/lilygo-standard-layout.md#installed-layout-verification). See the [baseline decision](docs/adr/0008-lilygo-maintained-baseline.md) and [historical bring-up and recovery evidence](docs/lilygo-v247.md).

## Build without connecting a device

Requires Linux, Git, `uv`, and network access for pinned tool downloads. From the repository root, use Bash:

```bash
bash tools/setup-lilygo.sh
bash tools/build-lilygo.sh
```

The setup/build scripts never open a serial port or flash a board. Python build tools live in `${ESPSOMFY_TOOL_CACHE:-$HOME/.cache/espsomfy-lilygo}`; pioarduino keeps its downloaded platform/compiler packages in `~/.platformio`. The pinned tag still contains application version `4.0.1`, which its build script uses for `FW_VERSION`; the manifest records both the actual compiler metadata and the pinned source revision. Assigning a distinct fork release identity remains part of issue #12. The pinned pioarduino Core 6.2.0 uses platform 55.03.312 (Arduino 3.3.12), with the exact library versions in `platformio.ini`. The vendored CC1101 driver comes from the pinned baseline. This replaces the old Arduino CLI build; historical tool versions and generated v2.4.7 binaries are not inputs to this build.

The shared release environment `lilygo-t-embed-cc1101` selects `ESPSOMFY_LILYGO_T_EMBED_CC1101`, ESP32-S3 N16R8, QIO flash at 80 MHz, 16 MB physical flash, 8 MB OPI PSRAM and hardware USB CDC. The ROM image header uses DIO while the SDK enables configured QIO after boot; the manifest distinguishes these. Generic environments keep upstream settings. Artifacts and a validated board-specific manifest are produced locally under `build/lilygo-t-embed-cc1101/` by default; see the build script for exact output names. Hashes identify the artifacts and source inputs, but are not signatures or authorization to install them.

## Partitions and migration limits

See the [standard-layout experiment](docs/lilygo-standard-layout.md) for the comparison with xkain, alternatives and offline build evidence.

| Partition | Offset | Size |
|---|---|---|
| NVS | `0x9000` | `0x5000` |
| OTA selection | `0xe000` | `0x2000` |
| App 0 | `0x10000` | `0x330000` |
| App 1 | `0x340000` | `0x330000` |
| LittleFS | `0x670000` | `0x180000` |
| Core dump | `0x7f0000` | `0x10000` |

The build selects the pinned Arduino SDK’s standard `default_8MB.csv`, matching cjkas’s generic S3 release layout. Each OTA app slot is 3.1875 MiB and LittleFS is 1.5 MiB. Physical flash from `0x800000` through `0xffffff` remains unallocated. This layout choice does not settle the cjkas versus xkain firmware decision. The manifest checks the actual binary partition table and image sizes against this table. Firmware-only artifacts target an inactive application slot on an already provisioned matching layout. Provisioning artifacts include bootloader/partition/filesystem content and have different effects. The November 6 installation used a separately reviewed, state-preserving three-segment plan; it is not permission to install pristine filesystem artifacts or reuse stale counters.

The historical v2.4.7 table used `0x140000` app slots and LittleFS at `0x290000`. The standard app slots overlap that old LittleFS location. They also differ from the earlier offline Plus candidate’s custom table (`app1` at `0x310000`, LittleFS at `0x610000`); those packages are not interchangeable. Preserving NVS offsets alone does not preserve authoritative `shades.cfg`, nor make a provisioning write safe. A merged image can also overwrite NVS with padding. Export plus raw backup, verified independent extraction, latest counters, and an exact reviewed write/restore plan are required before an approved migration. Do not use old guide flash commands with these artifacts; follow [the current flashing procedure](docs/flashing.md).

## Board wiring and radio behavior

| Function | GPIO / setting |
|---|---|
| Peripheral power | 15 HIGH |
| CC1101 SCK / MOSI / MISO | 11 / 9 / 10 |
| CC1101 CS / GDO0 TX / GDO2 RX | 12 / 3 / 38 |
| RF SW1 / SW0 | 47 HIGH / 48 HIGH for 433 MHz path |
| Display CS / SD CS / nRF24 CS | 41 HIGH / 13 HIGH / 44 HIGH |
| nRF24 CE / display backlight | 43 LOW / 21 LOW |

Deselect shared-SPI peripherals before enabling power, select the 433 MHz filter and allow the rail to settle. Before CC1101 initialization, release SPI ownership, perform the bounded MISO-ready check and initialize SPI on the mapped pins. Retain the vendored driver’s Arduino 3 SPI handling, and cjkas’s radio lock and scheduling. Invalid frequency or failed initialization leaves the integrated radio unavailable to software: RX interrupts stay detached and new TX work is refused. A failed reconfiguration can leave the physical chip in its previous RX state; this does not prove hardware idle. The synthesizer default is 433.42 MHz; the selected filter accepts 387–464 MHz.

Radio pin settings are fixed against saved, imported and UI values. Board control pins, USB 19/20 and flash/OPI PSRAM 26–37 are reserved against relay assignments even when the radio is disabled. The shared build starts the display and local interface. SD and nRF24 drivers are not started. UART0 shares pins with nRF24, so diagnostics use USB CDC.

## State and Home Assistant compatibility

Synthetic format24 tests exercise the native record parser for shade/group identities, calibration, protocol and group membership without opening private backups. A restore on the same controller keeps the higher per-address NVS/file rolling code. That avoids lowering counters when importing a stale export. An empty replacement NVS can only recover primary counters present in the export. Linked-remote counters are absent from format24 and remain NVS-only. Older raw snapshots are not automatically safe after more commands have been sent. The inherited 16-bit next-code wrap at 65535 is unchanged; this port does not invent new codes or motor resynchronization.

The baseline HA integration is [cjkas/ESPSomfy-RTS-HA v3.0.4](https://github.com/cjkas/ESPSomfy-RTS-HA/tree/0d3bc1349ac1047c843e87ea21381b069d786fb2), revision `0d3bc1349ac1047c843e87ea21381b069d786fb2`. Features that add protocol operations need the matching firmware and HA integration changes; use the current [protocol guide](docs/PROTOCOL.md) and advertised schema. Firmware 4.x uses protocol 2 on `ws://host/ws` port 80, with login according to the configured security mode. Old ports 8080/8081 and HTTP model APIs are removed. A stock v2.4.7 integration is incompatible. HA deployment is separate from firmware installation. HA’s update display does not implement device-side GitHub pull installation.

## Offline checks and deployment prerequisites

Run native tests with pioarduino and a host C++ compiler; no board is needed:

```bash
~/.cache/espsomfy-lilygo/pio-python/bin/pio test -e native
node --test 'test/js/*.test.js'
bash test/scripts/test_set_version.sh
CXX=g++ bash tools/test-board-profile.sh
```

Use the cache override consistently if configured. Build-tool tests and exact artifact verification are recorded in [the port validation report](docs/lilygo-port-validation.md).

Compile, GPIO harness and fake-device protocol results do not prove RF transmission, actual calibration accuracy, real-device import or rollback. Before considering deployment, finish [recovery validation #9](https://github.com/SidShaytay/ESPSomfy-RTS/issues/9), [safe releases/pull updates #12](https://github.com/SidShaytay/ESPSomfy-RTS/issues/12), and the relevant [security review #14](https://github.com/SidShaytay/ESPSomfy-RTS/issues/14). Obtain explicit approval for a concrete migration plan. Do not flash the sole working board, transmit with duplicate identities, pair, calibrate or issue motor commands as a substitute for offline evidence.

## One firmware for CC1101 and Plus

Use `lilygo-t-embed-cc1101` on both standard CC1101 and CC1101 Plus. The Plus adds nRF24, which this project leaves unused; their reviewed radio/display/input/power/memory wiring permits one shared firmware. It requires `ESPSOMFY_STATUS_DISPLAY`, `ESPSOMFY_LOCAL_UI` and `ESPSOMFY_LOCAL_COMMANDS`, with pinned GFX 1.12.1 and BusIO 1.17.4. Generic targets do not inherit these features.

There is one environment, image and package identity. Package `compatible_boards` names exactly standard CC1101 and Plus under the same reviewed flash/PSRAM/layout/feature requirements; unknown and generic S3 hardware cannot match. The Plus-only environment/macro and all `-display`, `-ui` and `-ui-preview` release names are rejected; they are not aliases.

This build consolidation preserves the existing LCD controls; complete web/LCD coverage remains tracked in the [capability matrix](docs/lcd-web-capability-matrix.md). See the [local UI guide](docs/lilygo-local-ui.md), [status fallback](docs/lilygo-status-display.md), and [pinned manufacturer/revision audit](docs/lcd-build-audit.md). A shared image does not replace physical qualification of each actual board/revision.

For existing installations, identify actual hardware, running features and layout using the separately authorized procedure in [flashing](docs/flashing.md). Historical Plus/suffix profile IDs need an explicit transition; compatibility is not inferred from the new image name. This baseline does not import the unfinished #12 updater; its shared supported-hardware metadata, trust/admission/rollback and migration checks must be reviewed with that integration before GitHub installation is exposed. No second installer or profile alias is provided here.
