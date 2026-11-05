# Forward-port the Plus profile onto pinned cjkas v4.0.2

For [issue #13](https://github.com/SidShaytay/ESPSomfy-RTS/issues/13), choose the maintained-base forward port over selective backports into rstrouse v2.4.7. Pin [cjkas v4.0.2](https://github.com/cjkas/ESPSomfy-RTS/tree/07bffdec6b540cc99f31d02d7e6de6d3d1e7f8f3), commit `07bffdec6b540cc99f31d02d7e6de6d3d1e7f8f3`. This selects a source baseline for an isolated migration candidate. It does not authorize deployment or change the installed controller.

The maintained baseline brings Arduino 3, asynchronous networking, a dedicated radio scheduler and lock, native configuration/protocol tests, and protocol 2. Selectively copying these systems into the old tree would require maintaining both implementations and adapting the same dependencies. Import the baseline separately, then forward-port only the Plus hardware constraints and offline compatibility changes. Preserve the fork’s research, recovery evidence, tests and history.

## Considered options

- **Selective backports into v2.4.7.** Keeps current partitions/API and initially changes fewer files, but requires transplanting tightly related framework, networking, security and concurrency changes. Rejected for this candidate.
- **Cherry-pick PR #10 onto cjkas.** Rejected: source paths split, Arduino SPI ownership changed, radio scheduling and locks changed, and release partitions differ. Reuse the board pin definitions, power sequence and tests; retain cjkas’s newer driver and scheduler.
- **Follow an unpinned maintained branch.** Rejected because build and compatibility evidence would move under review.

## Consequences

- `SidShaytay/ESPSomfy-RTS` remains the repository and future release source. Device-side GitHub pull updates remain required under [issue #12](https://github.com/SidShaytay/ESPSomfy-RTS/issues/12). Upstream [ADR 0007](0007-no-github-update.md) explains cjkas’s removal; it is a description of that baseline, not this fork’s final update policy. This port does not restore the old GitOTA. Safe authenticated background pull updates and release identity remain separate work.
- The Plus environment explicitly chooses 16 MB flash, 8 MB OPI PSRAM and its own two-slot partition table. It cannot fit the installed v2.4.7 app slot. A migration needs separately approved provisioning and preservation/restoration of current state; changing the partition table is not firmware-only OTA.
- The Plus profile is headless and compile-time opt-in. It owns the shared bus’s pins and power/RF controls, fixes radio pins after every configuration entry point, and reserves board/USB/flash/PSRAM pins even when the radio is disabled. GPIO preflight must release SPI ownership before changing pin modes, then reinitialize the mapped bus. Radio work remains under the existing recursive radio lock.
- A format24 restore retains the greater of NVS and file rolling codes for each primary remote address. This deliberately differs from cjkas’s restore policy, which takes the file value even when lower. Format24 has linked-remote addresses but not their counters. A new board with empty NVS cannot reconstruct counters absent from the export.
- Keep cjkas’s wire protocol 2 and a pinned compatible HA integration. Existing v2.4.7 clients cannot be carried over unchanged. Offline tests establish parsing and fake-device compatibility, not actual radio or motor behavior.
- Hardware and recovery validation remain prerequisites for any deployment. [Issue #9](https://github.com/SidShaytay/ESPSomfy-RTS/issues/9) requires independent extraction and reviewed restoration. Old full-flash/NVS snapshots can contain stale counters. Do not substitute pairing, calibration or regenerated identities for a faithful restore.
