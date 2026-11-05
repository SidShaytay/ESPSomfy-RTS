# LILYGO PR compatibility with cjkas

## Is PR10 directly compatible?

### Takeaway
The hardware knowledge is reusable, but the merged PR is not a clean cherry-pick into cjkas v4.0.2/current main. A small forward port is plausible; no compile or hardware compatibility has been demonstrated.

### Cited Findings
- User PR10 includes code commit f07d172 and tooling commit83a80df. The code adds BoardProfile power/SPI deselection/filter setup, fixed pins, reserved GPIO checks, radio readiness/availability checks and host tests. — [PR10](https://github.com/SidShaytay/ESPSomfy-RTS/pull/10), [code](https://github.com/SidShaytay/ESPSomfy-RTS/commit/f07d1726f83e06ddb8eb0e49d8b8838dbd570117), [profile](https://github.com/SidShaytay/ESPSomfy-RTS/blob/f07d1726f83e06ddb8eb0e49d8b8838dbd570117/BoardProfile.cpp)
- Tested locally against an isolated shallow cjkas checkout at stable v4.0.2 commit07bffdec6b540cc99f31d02d7e6de6d3d1e7f8f3: original patch apply-check fails because root ConfigFile.cpp/Somfy.cpp/Somfy.h/SomfyController.ino are absent. Prefixing src still fails ConfigFile/Somfy.h hunks; Somfy.cpp and .ino no longer exist. This was `git apply --check`, not a mutation or compilation. — [stable source tree](https://github.com/cjkas/ESPSomfy-RTS/tree/07bffdec6b540cc99f31d02d7e6de6d3d1e7f8f3/src)
- cjkas split the former monolithic Somfy implementation into Transceiver, RadioScheduler, ShadeGpio, SomfyShade and related modules; startup is src/SomfyController.cpp. — [Transceiver](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/src/Transceiver.cpp), [startup](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/src/SomfyController.cpp)
- Stable v4.0.2 uses pioarduino Arduino3.3.12/IDF5.5 and ArduinoJson7.4.3, versus our pinned Arduino2.0.17/ArduinoJson6.21.5. — [platformio.ini](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/platformio.ini), [our tooling](https://github.com/SidShaytay/ESPSomfy-RTS/blob/main/tools/setup-lilygo.sh)

### Inferences
- Reuse BoardProfile constants, power/filter sequencing and GPIO-reservation intent, and adapt host tests. Port calls into new startup, Transceiver configuration and GPIO-edit paths.
- Do not copy the old entire radio implementation into the refactored fork: it would discard scheduler, locking and driver fixes.

### Gaps
- No cjkas Plus firmware was built or flashed. No devices, HA or motors were contacted. Exact Plus environment/PSRAM/USB behavior remains untested.

## What semantic compatibility work is required?

### Takeaway
The differences exceed file moves. Arduino3 SPI ownership and concurrent radio access require adapting initialization, while partition/HA migration needs an explicit plan.

### Cited Findings
- cjkas still uses the ELECHOUSE/SmartRC API, but vendors SmartRC2.5.7 under lib with fixes. Its driver avoids calling pinMode on SPI-owned pins (stops the S3 SPI bus) and uses gpio_get_level plus a bounded10ms MISO readiness wait. Our BoardProfile::prepareRadio currently calls pinMode(miso,INPUT) and configures SCK/MOSI on each preparation/retry. — [vendored driver](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/lib/SmartRC-CC1101-Driver-Lib/ELECHOUSE_CC1101_SRC_DRV.cpp), [our readiness probe](https://github.com/SidShaytay/ESPSomfy-RTS/blob/main/BoardProfile.cpp)
- cjkas Transceiver serializes SPI/radio access using a recursive mutex and a separate TX scheduler/worker. Config changes preempt current work under RadioLock; direct old-code calls must respect this. It still transmits waveform GPIO pulses using GPIO_OUT registers, so the low GPIO3 TX assignment remains relevant. — [radio source](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/src/Transceiver.cpp)
- Our release uses app slots0x140000 at0x10000/0x150000 and LittleFS0x290000 size0x160000. cjkas release table uses app slots0x180000 at0x10000/0x190000 and LittleFS0x310000 size0xE0000. Both use NVS0x9000,size0x5000. Its N16R8 debug environment instead has one3MB app slot/no OTA. — [our manifest logic](https://github.com/SidShaytay/ESPSomfy-RTS/blob/main/tools/build-lilygo.sh), [release table](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/esp32_3MB.csv), [debug table](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/esp32_huge_app.csv), [build environments](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/platformio.ini)
- cjkas still defines backup format24 and its record readers handle version24, matching our export format. However its explicit restore takes the file rolling counter even if lower than existing NVS; our original-base loader retains the maximum. — [format](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/src/ConfigRecords.h), [record readers](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/src/ConfigRecords.cpp), [counter policy](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/src/RollingCode.h)
- cjkas4.x unifies API/events onto port80 and replaces old read endpoints with its socket protocol. A matching cjkas HA integration is required; current main migration instructions require3.0.1+. — [current README](https://github.com/cjkas/ESPSomfy-RTS/blob/f17eb4c7bbf68b4bd64456be415022746e698f1f/README.md), [protocol](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/docs/PROTOCOL.md)
- Fetched current main f17eb4c7bbf68b4bd64456be415022746e698f1f differs from stable in README, data-src/appversion/index.html/index.js and ConfigSettings.h; the inspected Transceiver, startup, platformio and ConfigFile code did not differ. — [compare](https://github.com/cjkas/ESPSomfy-RTS/compare/v4.0.2...f17eb4c7bbf68b4bd64456be415022746e698f1f)

### Inferences
- First adapt startup-only pad configuration and safe readiness checking to Arduino3. Preserve cjkas locking and patched driver.
- Add an explicit Plus release environment:16MB flash,8MB OPI PSRAM, hardwareUSB, own board define and manifest identity. Existing debug N16R8 is not a released Plus image.
- Current partition layout cannot be assumed to fit cjkas firmware. Choose a validated layout then backup/USB provisioning/import on spare hardware; do not upload its stock image as a routine OTA on our running device.
- Format24 reader support makes migration promising, not proven. Verify latest identities, all three counters, group membership/calibration/credentials and old/new HA behavior in isolation; do not restore a stale snapshot.

### Gaps
- No binary-size fit, backup import round trip, hardware initialization or receive/transmit tests were performed.

## Pull cjkas changes backward or port the board forward?

### Takeaway
Both directions are possible engineering, but preserving the current fork through selective backports is distinct from broadly merging cjkas4.x. The small board patch is easier to forward-port than the whole cjkas architecture is to backport.

### Cited Findings
- cjkas4.0.2 reorganizes model changes, HTTP entry points, socket protocol and radio scheduling across many modules and upgrades core/dependencies. — [source tree](https://github.com/cjkas/ESPSomfy-RTS/tree/v4.0.2/src), [build](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/platformio.ini)
- Its accepted ADR0007 removes GitHub device downloads, while retaining browser-upload OTA. Retaining the original updater in a cjkas-based fork is an intentional divergence, not just preserving a configuration flag. — [ADR0007](https://github.com/cjkas/ESPSomfy-RTS/blob/main/docs/adr/0007-no-github-update.md)

### Inferences
- Conservative option: keep the working fork and backport bounded fixes plus harden fork-owned, board-aware GitHub OTA. This preserves current HA contract/layout, but carries maintenance responsibility for code the maintained fork removed.
- Modern-base option: fork cjkas, forward-port the small Plus profile, then independently retain/rebuild secure device-side GitHub updates around the new asynchronous architecture and validated partition layout. This imports more maintained improvements at the cost of one planned migration.
- A recurring blanket merge from cjkas into the old monolith will incur conflict/behavior risk; selective commits or a deliberate rebase onto the new architecture are clearer strategies.

### Gaps
- No automatic merge or cherry-pick was committed; testing was restricted to clean patch-application checks in a temporary checkout. Original working tree remained unchanged.
