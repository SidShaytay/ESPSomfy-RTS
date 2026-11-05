# Retaining safe GitHub updates in a maintained LILYGO fork

Snapshot: 2026-11-04, public source and local source inspection only. No controller/network requests, flashes, motor commands or implementation changes.

## Is GitHub pull updating incompatible with cjkas?

### Takeaway
No fundamental hardware or language incompatibility prevents it. cjkas deliberately removed a problematic subsystem. Restoring the old files verbatim would reconnect obsolete UI/API/event contracts and retain the vulnerabilities/reliability problems that motivated removal; a small new update service is preferable on that architecture.

### Cited Findings
- Accepted ADR0007 lists memory allocation failures (35–40KB contiguous TLS allocation), unchecked server certificates, blocking server-task operations/dropped events and mismatched HTTP methods. It removes release listing/check/download/cancel, filesystem recovery downloads and updateProgress events; authenticated manual firmware/LittleFS upload remains. — [ADR0007](https://github.com/cjkas/ESPSomfy-RTS/blob/f17eb4c7bbf68b4bd64456be415022746e698f1f/docs/adr/0007-no-github-update.md)
- Current cjkas Web.cpp uses ESPAsyncWebServer and a bounded loopQueue for work owned by the main/radio loop; centralized HTTP and socket authorization remain outside an optional updater. — [Current Web.cpp](https://github.com/cjkas/ESPSomfy-RTS/blob/f17eb4c7bbf68b4bd64456be415022746e698f1f/src/entryPoints/Web.cpp)
- Protocol2 consolidates sockets on port80 and removes old reads/events. This makes the old GitOTA progress/event integration a porting task, not a clean isolated cherry-pick. — [Protocol](https://github.com/cjkas/ESPSomfy-RTS/blob/f17eb4c7bbf68b4bd64456be415022746e698f1f/docs/PROTOCOL.md)
- Current code is pinned to Arduino3.3.12/IDF5.5 with exact async-server dependencies; verified through GitHub API at f17eb4c7, because a web-cached raw-main response returned an older configuration. — [Pinned build source](https://github.com/cjkas/ESPSomfy-RTS/blob/f17eb4c7bbf68b4bd64456be415022746e698f1f/platformio.ini)

### Inferences
- A cjkas-derived fork can retain its request authentication, queue ownership and protocol, while adding an explicitly requested GitHub update job in a background worker. The HTTP callback should authorize/enqueue/return; it should not perform blocking TLS downloads or arbitrarily mutate shade state.
- “Device can update itself from GitHub releases” need not mean unattended automatic installation. Keep user-triggered pull installation and separate periodic availability checks from installation policy.
- Reintroducing pull updates makes that subsystem our maintenance/security responsibility; the resulting fork can still integrate cjkas changes, but cannot honestly claim to follow upstream policy unchanged.

### Gaps
- No implementation/build/memory measurements were performed. TLS peak allocation, flash-slot capacity and concurrency need measured prototypes on the Plus configuration.

## Which integration path best preserves the user's priorities?

### Takeaway
Avoid broad merging of the new architecture into the legacy tree just to retain one old subsystem. Either make narrowly scoped security/update fixes on the existing base as a bounded interim release, or move the small hardware profile forward to a maintained base and reimplement pull OTA cleanly. Preserving GitHub OTA is a legitimate product requirement, not a reason to disregard security fixes.

### Cited Findings
- The current custom GitOTA selects assets only by chip, hard-codes upstream URLs, writes firmware followed by shared LittleFS, and schedules reboot after the attempted pair update. — [Current fork OTA](https://github.com/SidShaytay/ESPSomfy-RTS/blob/718b7d4/GitOTA.cpp)
- cjkas changes API/socket/build/config contracts and requires USB provisioning plus restore when coming from original firmware/3.x. — [Migration](https://github.com/cjkas/ESPSomfy-RTS#readme)
- xkain's stable2.6.1 has authorization guards but its inspected update source still bypasses certificate verification. linuxrrze centralizes repo URLs but leaves much legacy port8081 API public. Pulpyyyy retains own-fork OTA with CA verification and 4MB/8MB S3 asset selection, still not exact Plus validation. — [xkain stable Web](https://github.com/xkain/ESPSomfy-RTS/blob/v2.6.1/src/Web.cpp), [xkain OTA](https://github.com/xkain/ESPSomfy-RTS/blob/f55fb4cc5aa36ccbc9434ff2bd28a17efd04c628/src/GitOTA.cpp), [linuxrrze middleware](https://github.com/linuxrrze/ESPSomfy-RTS/blob/v2.4.19/Web.cpp), [Pulpyyyy OTA](https://github.com/Pulpyyyy/ESPSomfy-RTS/blob/731ffb35944a53ddf071104d4358d72372032efd/src/GitOTA.cpp)

### Inferences
- **A: Existing base plus selected fixes.** Lowest initial hardware/API migration risk. Borrow designs for actual route/upload authentication, private token keys, write-only secret settings, Origin/Host controls and strict TLS; adapt rather than blindly cherry-pick architecture-dependent commits. Preserves current HA contract and GPIO patch. Cost: we own an old stack and increasing backport effort. It is not “fully security-fixed” merely because a few checks were copied.
- **B: cjkas base plus Plus profile and a new optional OTA service.** Best long-term architectural direction if its migration is acceptable. Port the proven board initialization as an opt-in build, then add the user's differentiator behind tests. Need the new HA integration and tested configuration/counter import. The hardware profile should remain a small patch; OTA should be a separately testable module so later cjkas pulls are easier.
- **C: xkain/Pulpyyyy as maintained classic bases.** Worth an explicit comparison if retaining classic updater/HA compatibility is more important than cjkas architecture. Neither has confirmed exact Plus binary support, and retaining an updater does not establish its safety. This may avoid reintroducing the removed component, but still requires targeted security/board review.
- My preference: B for a maintained long-term product; A for a narrowly bounded interim hardening release. Do not repeatedly merge hundreds of cjkas structural commits into the legacy tree. If time-to-working safe pull OTA dominates, evaluate C alongside B before choosing.

### Gaps
- Patch compatibility and compile tests belong to the separate compatibility investigation. No complete vulnerability-report fix matrix or hardware migration was validated here.

## What minimum release/update architecture would be defensible?

### Takeaway
Retain the feature while addressing its specific failure modes: verified source/artifact identity, bounded nonblocking download work, board-specific assets, and independently protected persistent state. Existing dual application slots do not make firmware plus a single shared filesystem an atomic update.

### Cited Findings
- ESP-IDF safe application OTA writes the inactive app slot and changes boot selection after verification. Its documentation separately describes data-partition update hazards and optional app rollback. — [ESP-IDF OTA](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/ota.html)
- cjkas releases provide checksums and GitHub build attestations, verified on a computer using gh attestation verify. That is not evidence its ESP32 verifies signed firmware or manifests itself. — [Release trust](https://github.com/cjkas/ESPSomfy-RTS/blob/f17eb4c7bbf68b4bd64456be415022746e698f1f/SECURITY.md)
- PSRAM is usable with allocator/capability restrictions; capacity alone does not eliminate internal contiguous-allocation or DMA/cache constraints. — [ESP32-S3 external RAM](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/external-ram.html)

### Inferences
- Keep one explicit fork/channel source and HTTPS certificate/hostname validation. No insecure fallback and no arbitrary user-controlled URL fetch. Validate every redirect or narrowly constrain artifact hosts.
- Releases should publish a manifest naming exact board/profile, flash/PSRAM/build options, partition-layout identity, firmware/application/API versions, size and SHA256. Generic S3 is not a compatible-board identifier.
- SHA256 detects corruption only when its expected value is trusted. A signed manifest with a pinned verification key can authenticate hashes on the device; GitHub attestations remain a separate CI/provenance control. Define anti-downgrade policy and key rotation rather than pretending filenames/checksums are signatures.
- Queue one update job; stream chunks to the inactive application slot, bound memory/timeouts and reserve buffers. Verify digest before activating the image. Suspend new motor-command acceptance during update with clear status; never infer motor testing authorization from updater tests.
- Start with firmware-only OTA and explicit filesystem/API compatibility. The current single LittleFS partition also stores config: replacing it while firmware rollback is possible can strand the old app with incompatible UI/data. A later coordinated scheme needs an independent configuration partition/NVS and compatible or A/B web assets, or web assets embedded with firmware. The Plus's16MB makes larger layouts plausible, but layout changes require planned provisioning, not casual OTA.
- Test signed/unsigned/wrong-board/oversize/corrupted images, TLS/redirect failure, power cut, full flash, credential preservation and counter rollback. A verified native export and full configured raw backup are recovery inputs, not proof that a new partition/config migration works.
- Suggested PR order: (1) board/build and state import on selected base; (2) security-report regression/fix matrix; (3) fork version stamping and compatible release artifacts; (4) authenticated firmware-only background pull OTA; (5) optional filesystem update/recovery improvements after the state boundary is proven. No live installation until isolated tests pass and user authorizes physical verification.

### Gaps
- No secure-boot/signature enforcement, A/B filesystem or partition migration was implemented. No device was queried or modified. This is an architecture recommendation, not a tested patch.
