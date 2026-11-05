# Bruce and multiple firmware on the LILYGO Plus

## Can Bruce run on this board, and alongside Somfy?

### Takeaway
Bruce officially supports the T-Embed CC1101 Plus. Separate unmodified Bruce and Somfy firmware images are alternative bootable applications, not concurrently running applications. A merged firmware would be a new engineering project.

### Cited Findings
- Bruce's official supported-device table explicitly lists Lilygo T-Embed CC1101 Plus, with CC1101 and nRF24 support. Its canonical repository is now BruceDevices/firmware (pr3y/Bruce redirects). — [Official compatibility table](https://bruce.computer/); [canonical repository](https://github.com/BruceDevices/firmware)
- Latest stable Bruce observed was 1.16.1 (2026-08-11). Assets include `Bruce-lilygo-t-embed-cc1101.bin`; no separate Plus-named asset is listed in that release. The stable target uses QIO/OPI and `custom_16Mb.csv`. — [Release](https://github.com/BruceDevices/firmware/releases/tag/1.16.1); [target config](https://github.com/BruceDevices/firmware/blob/1.16.1/boards/lilygo-t-embed-cc1101/lilygo-t-embed-cc1101.ini)
- ESP-IDF bootloader selects one application partition from partition table/OTA metadata, loads it and transfers control. OTA normally writes the inactive slot and changes next boot selection; it does not execute both slot images together. — [Bootloader guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/bootloader.html); [OTA guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/ota.html)

### Inferences
- Running Bruce means the separate Somfy application is not running: HA/web/API reception and lilygo shutter control are unavailable until Somfy is booted again. The existing independent old remote is not inherently dependent on the lilygo service.
- The ESP32-S3's two CPU cores do not provide two independent firmware/OS instances. Concurrent functionality would mean integrating Somfy into one Bruce/FreeRTOS firmware, with shared task scheduling and explicit ownership of the one CC1101 radio/SPI bus. Bruce changing frequency/mode cannot coexist transparently with continuous Somfy receive. This is custom integration, not a checkbox or ordinary dual boot.
- For household reliability, a dedicated shutter controller and a separate experimentation device is the lowest-complexity design. The LILYGO display/buttons suit an interactive Bruce device; shutter service should stay available without relying on which menu/firmware is selected.

### Gaps
- No actual Bruce operation or hardware compatibility test was performed on this particular purchased Plus board.
- No maintained ready-made Bruce plugin providing this complete ESPSomfy controller, HA socket API, persistent identities and rolling counters was confirmed in this bounded research. Do not present raw RF replay as an equivalent shutter-controller implementation.

## Is a selectable multi-boot launcher practical?

### Takeaway
Yes in principle: bmorcelli/Launcher supports the T-Embed family and multiple installed firmware entries. It is feasible to investigate selectable Bruce/Somfy boots, but exact compatibility and persistent-data isolation must be tested before repartitioning this configured controller.

### Cited Findings
- The old bmorcelli/M5Stick-Launcher repository redirects to bmorcelli/Launcher. Stable 2.9.1 (2026-09-04) publishes `Launcher-lilygo-t-embed-all.bin`; the source supports a combined T-Embed/CC1101 environment with 16 MB layout, GPIO15 power, shared SPI and board detection. — [Launcher](https://github.com/bmorcelli/Launcher); [2.9.1](https://github.com/bmorcelli/Launcher/releases/tag/2.9.1); [board config](https://github.com/bmorcelli/Launcher/blob/main/boards/lilygo-t-embed-cc1101/platformio.ini)
- Launcher documentation describes a resident protected application, installed application menu entries, normal/merged/multipart binaries, dynamic app/data partitions and SD-backed data backups. It associates data partitions with apps and supports selecting a boot app; backups require an SD card. — [Project architecture](https://github.com/bmorcelli/Launcher/wiki/Explaining-the-project)
- Current source chooses a target OTA app then reboots. It switches BLE bond state explicitly but this is not a general guarantee of per-app NVS isolation. — [Boot implementation](https://github.com/bmorcelli/Launcher/blob/main/src/app_registry.cpp#L344-L367)
- Stable Launcher's initial 16MB partition table has NVS0x9000/0x5000, OTA metadata0xE000/0x2000, launcher app0 test partition0x10000/0x180000 and coredump0x190000/0x10000. This differs from our two 0x140000 Somfy app slots/LittleFS0x290000. — [Stable launcher partition table](https://github.com/bmorcelli/Launcher/blob/2.9.1/support_files/custom_16Mb.csv)

### Inferences
- 16 MB gives substantially more room than our current 4 MB layout uses, but capacity alone does not establish a safe install plan. A multi-boot experiment needs a compatible bootloader, a deliberate partition plan and matching data labels/firmware assumptions.
- Current ordinary Somfy OTA slots should not simply be treated as Bruce/Somfy slots: a later normal Somfy updater can overwrite the other app slot and normal OTA recovery assumptions differ from launcher multi-boot.
- This is a reversible lab project only after a fresh full configured flash/NVS backup and restore plan. It should not be tried on the sole working bedroom controller during bedtime.

### Gaps
- Launcher has a combined CC1101 target, not a separate Plus-named target. Official Bruce Plus support does not prove all launcher SD/PMIC behavior on the Plus.
- No exact tested Somfy-plus-Bruce package or supported installation/migration matrix was confirmed. 2.9.1 is stable; main has further changes. Do not assume all current-main internals are present identically in stable.

## What must protect pairing and rolling-code state?

### Takeaway
The main risk is not merely losing a firmware image. Repartitioning or swapping data can lose Wi-Fi credentials, paired remote identities and rolling counters. A launcher backup of a filesystem is not automatically a complete, current Somfy recovery snapshot.

### Cited Findings
- Bruce stable's own 16MB image defines NVS0x9000/0x6000, factory app0x10000/0x470000, SPIFFS data0x480000/0xB70000 and coredump0xFF0000. Writing this merged layout would replace our current layout. — [Bruce stable partitions](https://github.com/BruceDevices/firmware/blob/1.16.1/custom_16Mb.csv)
- Bruce mounts LittleFS using its default partition-label argument; our existing Somfy also uses `LittleFS.begin()`. Current Launcher installer looks up data by label and can reuse an existing same-labeled data partition. Thus the fact that apps can be installed does not prove their default `spiffs` stores are isolated. — [Bruce mount](https://github.com/BruceDevices/firmware/blob/1.16.1/src/core/sd_functions.cpp#L30-L33); [Launcher data selection](https://github.com/bmorcelli/Launcher/blob/main/src/partition_install_layout.cpp#L5-L30); [our startup](https://github.com/SidShaytay/ESPSomfy-RTS/blob/main/SomfyController.ino)
- ESP-IDF documents application OTA as safe-update capable but partition table, bootloader and data updates as unsafe-update operations under interruption. — [OTA update modes](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/ota.html)

### Inferences
- Isolate Somfy's filesystem and NVS/counter namespaces, verify both firmware versions find the intended stores, and ensure switching does not roll counters back. A restore of stale Somfy NVS may render future transmissions ineffective even though the UI still says paired.
- Bruce with a new/independent identity would not automatically reproduce our existing paired controller's state. Reusing a snapshot carelessly could create conflicting counter histories. The existing native Somfy backup and raw configured NVS/full flash under issue9 are required inputs, not optional cleanup.
- The practical recommendation is dedicated Somfy service first; dual boot only if the user accepts that HA control is offline whenever Bruce is active. Concurrent Somfy-in-Bruce is a separate integration effort with radio arbitration, durable counter storage and API tests.

### Gaps
- No write, flash, restart, controller query or motor command was performed in this research. No claim of a tested restore or tested multi-boot arrangement is made.
