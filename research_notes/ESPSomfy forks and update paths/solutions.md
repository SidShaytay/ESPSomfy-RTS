# Existing LILYGO and update solutions

Research snapshot: 2026-11-03 America/Los_Angeles. Source inspection only; no installation, device interaction or motor commands. Activity ranking is covered by the other researcher. The question here is whether active derivatives already solve our concrete update/profile problems.

## Which projects solve version identity and release updates?

### Takeaway
Several derivatives already solve important parts of issue #12, so borrowing proven changes is preferable to inventing everything. None inspected establishes safe turnkey support for this exact LILYGO Plus board. The most substantial changes are in cjkas, whose latest architecture deliberately removes on-device GitHub downloads.

### Cited Findings
- **cjkas/ESPSomfy-RTS firmware:** v4.0.2 released November 1, 2026. Its assets include SHA256SUMS and separate firmware/LittleFS images for each chip; examples are `SomfyController.ino.esp32s3.bin` and `SomfyController.littlefs.esp32s3.bin`. Assets are chip-specific, not LILYGO Plus-specific. [Release and asset list](https://github.com/cjkas/ESPSomfy-RTS/releases/tag/v4.0.2).
- **cjkas intentionally removes GitHub OTA:** accepted ADR0007 says release checking/downloading, recoverFilesystem, and the ESP-side HTTPS client are removed. Users manually upload firmware/LittleFS or use USB. It cites TLS heap fragmentation, disabled certificate checking, blocking operations, wrong HTTP verbs and lost update events. Matching HA integration from 3.0.4 checks GitHub itself; the update entity notifies but cannot install. [ADR0007](https://github.com/cjkas/ESPSomfy-RTS/blob/main/docs/adr/0007-no-github-update.md).
- **cjkas release versioning is now synchronized:** `.github/set_version.sh` takes the release tag and updates the firmware fallback string, appversion file, browser cache suffix and JS appVersion. Build workflow invokes it before producing release artifacts, and commits the bump after publication. `app_version.py` also derives FW_VERSION from `data-src/appversion`; current firmware fallback is v4.0.2. This is the current source answer to older version-display confusion. [Version script](https://github.com/cjkas/ESPSomfy-RTS/blob/main/.github/set_version.sh), [build workflow](https://github.com/cjkas/ESPSomfy-RTS/blob/main/.github/workflows/build.yaml), [build version extraction](https://github.com/cjkas/ESPSomfy-RTS/blob/main/app_version.py), [fallback string](https://github.com/cjkas/ESPSomfy-RTS/blob/main/src/ConfigSettings.h).
- **xkain firmware is a different repository from xkain HA enhanced integration.** The device firmware is `xkain/ESPSomfy-RTS`; `xkain/ESPSomfy-RTS-enhanced` contains the Home Assistant custom component and cannot replace the code on the lilygo. The HA component README explicitly relies on ESPSomfy firmware. [Firmware repo](https://github.com/xkain/ESPSomfy-RTS), [HA integration repo](https://github.com/xkain/ESPSomfy-RTS-enhanced).
- **xkain firmware has its own fork update channel:** `GitOTA.cpp` lists releases from `api.github.com/repos/xkain/ESPSomfy-RTS/releases` and downloads both releases and filesystem recovery from that fork. It still selects firmware by chip, not our compile-time Plus profile. Main says v2.6.2; most recent stable firmware release inspected was v2.6.1 October 17, 2026. [Updater](https://github.com/xkain/ESPSomfy-RTS/blob/main/src/GitOTA.cpp), [version](https://github.com/xkain/ESPSomfy-RTS/blob/main/src/ConfigSettings.h), [v2.6.1](https://github.com/xkain/ESPSomfy-RTS/releases/tag/v2.6.1).
- **linuxrrze centralizes the fork repository:** `GitOTA.h` defines `GIT_REPO "linuxrrze/ESPSomfy-RTS"`, used by release lookup, Internet check, selected-release download and filesystem recovery. Firmware main v2.4.19. `setFirmwareFile` still chooses generic chip filenames. This is a useful small reference for eliminating scattered hard-coded upstream URLs, not proof of board-safe OTA. [Repo definition](https://github.com/linuxrrze/ESPSomfy-RTS/blob/main/GitOTA.h), [OTA implementation](https://github.com/linuxrrze/ESPSomfy-RTS/blob/main/GitOTA.cpp), [version](https://github.com/linuxrrze/ESPSomfy-RTS/blob/main/ConfigSettings.h).
- **Pulpyyyy also owns its update channel:** `GIT_REPO_PATH` is `Pulpyyyy/ESPSomfy-RTS`. Its S3 asset selection distinguishes physical flash >=8MB versus smaller flash, choosing `_esp32s3_8mb.bin` or `_4mb.bin`. A 16MB Plus therefore matches its 8MB path; this is flash-size awareness, not verification of RF-switch/power/profile compatibility. Latest stable v4.1.0 assets include those 4MB/8MB variants. Release notes claim safer filesystem updates, update preflight, failed-update recovery, RF command serialization, and frame spacing. These are source/release claims, not tests on our board. [Repository definition](https://github.com/Pulpyyyy/ESPSomfy-RTS/blob/main/src/GitOTA.h), [selection implementation](https://github.com/Pulpyyyy/ESPSomfy-RTS/blob/main/src/GitOTA.cpp), [v4.1.0 release](https://github.com/Pulpyyyy/ESPSomfy-RTS/releases/tag/v4.1.0).

### Inferences
- For our existing code, linuxrrze is a small, concrete reference for fork-owned update URLs. cjkas is the stronger reference for a modern release/version pipeline and a deliberate decision to avoid ESP-side downloads altogether.
- These forks generally keep firmware and web application versions together; none inspected proves an independent component-compatibility scheme that would automatically make mixed firmware/UI versions safe.

### Gaps
- Did not test any binary, upgrade, backup import or HA migration. Current source claims are not runtime verification.
- Pulpyyyy has active beta branches beyond latest stable; this inspection focused on default-branch OTA and latest stable release rather than every beta.

## Is exact LILYGO T Embed CC1101 Plus support already solved?

### Takeaway
No turnkey prebuilt/profile for our exact Plus hardware was confirmed in the inspected candidate build configurations, source entry points and release assets. Generic ESP32-S3 support must not be confused with integrated CC1101 power/filter-switch initialization.

### Cited Findings
- xkain PlatformIO enumerates generic ESP32/S3, including 4MB/8MB S3 variants; no named T-Embed Plus environment appears there. [PlatformIO config](https://github.com/xkain/ESPSomfy-RTS/blob/main/platformio.ini).
- cjkas build configuration and release assets enumerate chip targets, not a named Plus target. [PlatformIO config](https://github.com/cjkas/ESPSomfy-RTS/blob/main/platformio.ini), [release assets](https://github.com/cjkas/ESPSomfy-RTS/releases/tag/v4.0.2).
- Upstream issue #598 includes a specific explanation that RF switches GPIO47/48 need HIGH for the 434MHz path; another Plus user still reports radio difficulties. This establishes that generic S3 firmware does not by itself prove correct operation of the integrated board. [Issue #598](https://github.com/rstrouse/ESPSomfy-RTS/issues/598).

### Inferences
- Retain our observed-working power/SPI/RF-switch profile unless/until an alternate source proves the exact initialization and compatible build settings. No candidate stock image should be flashed merely because its filename contains esp32s3.

### Gaps
- Absence of a named environment/asset is not proof that no branch or user patch supports Plus; exhaustive inspection of every branch was outside this research.
- No inspected candidate demonstrated our exact GPIO15 power, GPIO47/48 filter, SPI11/9/10, CS12 and TX3/RX38 sequence as a supported release profile. This remains unconfirmed rather than a universal claim of impossibility.

## What are the adoption and migration risks?

### Takeaway
Adoption could mean a substantial migration, not replacing one update URL. Our raw configured backup is still incomplete and no migration should be attempted during the user's quiet period.

### Cited Findings
- cjkas README explicitly states that an upgrade from rstrouse firmware or 3.x to 4.x through the old UI fails; take backup, flash onboard image over USB, restore backup, and first upgrade its HA integration to 3.0.1 or newer. [Migration notice](https://github.com/cjkas/ESPSomfy-RTS#readme).
- cjkas config code documents different rolling-counter behavior on restore: `restoring` is true and a restore takes codes in the file, rather than always preserving the newer local value. Its `keepRollingCode` passes stored/exported values and the restoring flag to a policy function. [ConfigFile.h](https://github.com/cjkas/ESPSomfy-RTS/blob/main/src/ConfigFile.h), [ConfigFile.cpp](https://github.com/cjkas/ESPSomfy-RTS/blob/main/src/ConfigFile.cpp). This makes fresh backup/counter verification especially important; do not assume original max(NVS,export) behavior.
- xkain current source defines backup header format26 and linuxrrze format25; our original code uses format24. Source contains version-dependent readers, but no actual import/export round trip was tested here. [xkain ConfigFile.cpp](https://github.com/xkain/ESPSomfy-RTS/blob/main/src/ConfigFile.cpp), [linuxrrze ConfigFile.cpp](https://github.com/linuxrrze/ESPSomfy-RTS/blob/main/ConfigFile.cpp).
- Pulpyyyy v4.1.0 specifically instructs firmware-first then filesystem when coming from4.0.0, so its filesystem preservation protection lives in firmware and update order matters. [Upgrade note](https://github.com/Pulpyyyy/ESPSomfy-RTS/releases/tag/v4.1.0).

### Inferences
- Best next step: evaluate/reuse selected upstream-fork fixes in issue #12 with source tests and board-aware release artifacts, not an overnight migration of a working motor controller.
- If adopting a full fork, validate its partition layout, our Plus profile, format24 restore, all identities/group membership and current rolling counters in isolation before using it on the working device. No control tests until the user explicitly allows them.

### Gaps
- cjkas current file format number and exact full migration conversion were not independently established; README-supported migration route is authoritative but not tested.
- HA enhanced integrations use distinct domains and migration behavior; their names/versions are not firmware versions. The matching integration for cjkas is linked in ADR0007; do not blindly swap HA integrations because both say ESPSomfy.
