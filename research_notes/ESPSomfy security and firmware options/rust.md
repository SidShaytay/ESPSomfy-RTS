# Rust reimplementation on the LILYGO Plus

## Is Rust feasible on this ESP32-S3, and which stack fits?

### Takeaway
Rust is feasible today; this is a substantial embedded-controller project rather than a mechanical C++ translation. The shortest path is a Rust application using ESP-IDF services, with a small, audited boundary to C; a predominantly Rust no_std/async implementation is possible but has more networking/storage/integration work.

### Cited Findings
- The current Rust-on-ESP book says ESP32/S2/S3 Xtensa targets need Espressif's compiler fork installed with espup; this differs from the standard RISC-V toolchain path. Rust's own platform-support table lists ESP32-S3 Xtensa bare-metal as Tier 3. — [Book toolchain source](https://github.com/esp-rs/book/blob/main/src/getting-started/toolchain.md), [rustc platform support](https://doc.rust-lang.org/beta/rustc/platform-support/xtensa.html)
- Current ESP-IDF Rust work lives in the **esp-rs/esp-idf monorepo** (esp-idf-sys raw bindings, esp-idf-hal driver wrappers, esp-idf-svc services). The former esp-idf-svc repo was archived September 19, 2026 because it moved; it was not abandoned. — [Current monorepo](https://github.com/esp-rs/esp-idf), [move notice](https://github.com/esp-rs/esp-idf-svc)
- The service crate exposes Wi-Fi, Ethernet, HTTP client/server, MQTT, WebSockets, NVS and OTA, with blocking/async variants. It retains ESP-IDF's C implementations underneath. — [Service README](https://github.com/esp-rs/esp-idf/blob/master/esp-idf-svc/README.md), [service source](https://github.com/esp-rs/esp-idf/tree/master/esp-idf-svc/src)
- Its maintainers explicitly describe esp-idf-* Rust wrappers as community-maintained, potentially behind ESP-IDF releases and lacking HIL tests; the monorepo points to Espressif-funded esp-hal for no_std development. — [Maintenance/support distinction](https://github.com/esp-rs/esp-idf#community-effort)
- esp-hal supports ESP32-S3 and provides safe blocking/async peripheral APIs. Its RMT driver generates and captures pulse sequences, but RMT is currently gated under the unstable feature. The current book warns that unstable drivers can break on dependency updates. — [S3 HAL](https://docs.espressif.com/projects/rust/esp-hal/main/esp32s3/esp_hal/index.html), [S3 RMT](https://docs.espressif.com/projects/rust/esp-hal/main/esp32s3/esp_hal/rmt/index.html), [stability policy](https://docs.espressif.com/projects/rust/book/)
- Context7 queried for the esp-rs book, esp-idf-svc and esp-hal, then important claims checked against current official sources. Some Context7 links still point to the pre-monorepo repositories; use the current monorepo for a new project. — [Current layout](https://github.com/esp-rs/esp-idf#repository-layout-and-history)

### Inferences
- **std/ESP-IDF route:** best near-term prototype for Wi-Fi+HTTP+WebSockets+NVS+OTA, reusing mature IDF services. Still needs a new Somfy application, board setup, tests and compatible API. It is not 100% Rust and does not eliminate C-network-stack risk.
- **no_std/esp-hal route:** good for tight control and portable protocol/state code, with Embassy/async networking where suitable. It means owning more infrastructure and carefully pinning evolving drivers. “No std” does not mean no Wi-Fi or no allocation is possible; neither is it automatically safer overall.
- The existing 16MB flash/8MB PSRAM is not the fundamental obstacle, but exact image size, layout and usable PSRAM configuration need a real build; none was performed.

### Gaps
- No Rust binary was built or flashed for this board; PSRAM/USB/RMT/radio timing have not been validated together.

## Can existing Rust radio/protocol code be reused?

### Takeaway
There is reusable Rust Somfy protocol work, but no verified drop-in replacement for this complete controller or the Plus board.

### Cited Findings
- **reitermarkus/somfy-rs** is a Rust RTS library with Frame, Remote, SendFrame and RollingCodeStorage abstractions; latest published release is 0.1.0 (June 28, 2024), while the repository API reported a push on November 2, 2026. — [Repository/release](https://github.com/reitermarkus/somfy-rs), [API metadata](https://api.github.com/repos/reitermarkus/somfy-rs), [library docs](https://docs.rs/somfy/latest/somfy/)
- Its current source uses std and unconditionally depends on **rppal**, the Raspberry Pi/Linux peripheral library. The code is not an ESP32 firmware as-is. — [Cargo manifest](https://github.com/reitermarkus/somfy-rs/blob/main/Cargo.toml), [portable-looking frame code](https://github.com/reitermarkus/somfy-rs/blob/main/src/frame.rs), [storage trait](https://github.com/reitermarkus/somfy-rs/blob/main/src/lib.rs)
- **cc1101-embassy 0.1.0** is an early-development embedded-hal/async packet driver. Its author says it was tested on RP2040/Ebyte modules. Its documented packet TX/RX/GDO path does not demonstrate asynchronous serial OOK RTS timing on this ESP32-S3 board. — [Author README](https://github.com/machshev/cc1101-embassy), [crate docs](https://docs.rs/cc1101-embassy/latest/cc1101_embassy/)
- The existing Somfy application contains custom frame construction, 56/80-bit behavior, repeats, radio timing and rolling-code persistence; the Plus profile adds shared-SPI deselection, power GPIO15 and RF GPIO47/48 initialization. — [Working Somfy implementation](https://github.com/SidShaytay/ESPSomfy-RTS/blob/main/Somfy.cpp), [working board setup](https://github.com/SidShaytay/ESPSomfy-RTS/blob/main/BoardProfile.cpp)

### Inferences
- Audit/reuse portable framing and state abstractions before implementing the protocol again; do not adopt an unrelated packet-radio example as proof of raw RTS support.
- RMT is a promising way to emit/capture waveform timings without software busy-loop jitter, but RF frame, wakeup, synchronization/repeat timing and RX thresholds need comparison against the established implementation on a spare fixture.
- An incremental C ABI boundary around a pure Rust parser/framer or state module is possible. It requires toolchain/ABI/ownership discipline and does not magically make existing Arduino classes safe; beginning with host-testable pure functions is less disruptive than replacing everything.

### Gaps
- No verified maintained Rust library providing complete ESPSomfy-compatible CC1101 asynchronous RTS control, backups, UI, HA API and exact Plus support was found in this bounded search.
- No driver or somfy-rs correctness/security audit was conducted. Recent pushes are not evidence of tested hardware support.

## What are the security and migration tradeoffs?

### Takeaway
Rust can remove many memory-safety failure modes from newly written safe code, but the application still must enforce authentication, TLS, update trust and durable rolling-code rules. Compatibility/persistence is the highest-consequence part of a replacement.

### Cited Findings
- Unsafe Rust/FFI exists because operations outside safe Rust's guarantees need explicit contracts; language choice is not a guarantee about external C implementations or hardware behavior. — [Rust book, unsafe Rust](https://doc.rust-lang.org/book/ch20-01-unsafe-rust.html)
- NVS uses explicit named partitions/namespaces and commit semantics. Espressif says interrupted writes can lose the new value being written, and unencrypted NVS can be read/altered with physical access. NVS's recovery behavior alone does not supply an application-level multi-record transaction. — [ESP32-S3 NVS reference](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/storage/nvs_flash.html)
- This controller's configuration is format24, includes distinct group/shade addresses and counters, and loads the higher file/NVS counter. Therefore replacing persistence or loading an old snapshot has compatibility consequences. — [Config format/restore implementation](https://github.com/SidShaytay/ESPSomfy-RTS/blob/main/ConfigFile.cpp)
- The existing HA integration discovers controller configuration and receives socket state/events, with defined HTTP command endpoints. Rust would need to maintain this interface or provide a new HA integration, not merely transmit equivalent radio frames. — [HA controller/API source](https://github.com/rstrouse/ESPSomfy-RTS-HA/blob/main/custom_components/espsomfy_rts/controller.py)

### Inferences
- A fresh design should fail closed when persistence fails: durably reserve the next protocol-valid counter before transmitting, never reuse an already transmitted counter after reboot, and ensure OTA rollback/config restore cannot silently move counters backward. Whether any skipped committed-but-unsent code is accepted must be validated against protocol behavior, not guessed or tested randomly on the paired motors.
- Preserve all three identities (Bedroom, Bathroom, Both rooms), group membership, calibration and latest counters; do not clone the old remote or create new identities during import. Keep credentials and data partitions out of firmware-image replacements.
- Reusing the existing browser assets saves UI work, but does not fix browser-side security issues. New Rust HTTP services still need explicit route authorization, request validation, TLS/update verification and secure credential handling. Rust also does not strengthen Somfy's over-air protocol.
- Suggested experiment: pure Rust framing/state/storage model with golden-vector and reset/fault tests on the host; then a separate Rust/ESP-IDF spare-board build preserving exact Plus power/RF pins; then scoped API/event compatibility and format24 import/export validation. Only after the raw configured backup and isolated timing/persistence tests should any migration be considered.
- For this user's working bedtime controller, maintained C++ firmware plus focused hardening is lower immediate operational risk than a full rewrite. Rust is worthwhile as a separate engineering project if reducing owned unsafe code and simplifying the service is an explicit goal—not a quick fix for the version label.

### Gaps
- No timing, power-failure, format import round-trip, firmware rollback or HA compatibility tests were run. No code, device settings, radio transmissions or motor commands were changed.
