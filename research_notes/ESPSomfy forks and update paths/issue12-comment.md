## Maintained fork research, 2026-11-03

Upstream's main/release development is dormant: last main commit and latest prerelease were August 19, 2024. It is not archived, and no formal abandonment declaration was found. [History](https://github.com/rstrouse/ESPSomfy-RTS/commits/main/), [releases](https://github.com/rstrouse/ESPSomfy-RTS/releases).

Among 154 retrieved direct forks plus secondary-derivative searches, **cjkas** is the strongest maintained candidate by measured recent volume: 304 main commits since August 5, with stable v4.0.2 released November 1. **xkain** remains active (37 main commits, stable v2.6.1 October 17); **linuxrrze** has update-focused work and v2.4.19 November 2 (66 commits with much release/binary churn). **Pulpyyyy** has active beta development beyond its main branch (v4.2.0-beta.5 November 3 UTC / November 2 local). Counts measure activity, not correctness or complete development on non-default branches. [cjkas](https://github.com/cjkas/ESPSomfy-RTS), [xkain](https://github.com/xkain/ESPSomfy-RTS), [linuxrrze](https://github.com/linuxrrze/ESPSomfy-RTS), [Pulpyyyy](https://github.com/Pulpyyyy/ESPSomfy-RTS).

Existing solutions worth reusing:

- cjkas derives release versions from tags and publishes checksums/chip-specific LittleFS images. It deliberately removes ESP-side GitHub OTA; matching HA integration checks releases and notifies, while installation is manual upload/USB. [Version script](https://github.com/cjkas/ESPSomfy-RTS/blob/main/.github/set_version.sh), [ADR0007](https://github.com/cjkas/ESPSomfy-RTS/blob/main/docs/adr/0007-no-github-update.md).
- xkain firmware already uses its own update repository. Its Enhanced repository is a separate HA component. [Updater](https://github.com/xkain/ESPSomfy-RTS/blob/main/src/GitOTA.cpp).
- linuxrrze centralizes the OTA/recovery repository in `GIT_REPO`; this is a useful narrow reference, but asset selection remains chip-based. [Definition](https://github.com/linuxrrze/ESPSomfy-RTS/blob/main/GitOTA.h).
- Pulpyyyy uses its own repository and S3 4MB/8MB asset selection. Flash-size awareness is not exact Plus board-profile validation. [Source](https://github.com/Pulpyyyy/ESPSomfy-RTS/blob/main/src/GitOTA.cpp).

**No exact T-Embed CC1101 Plus profile/prebuilt was confirmed in the inspected build configurations/assets.** Preserve our proven power/SPI/RF-switch initialization. Generic S3 assets are not sufficient compatibility evidence.

Recommended next step: evaluate cjkas first and xkain as the alternative; compare selective reuse with a full migration. cjkas documents USB provisioning/restore and newer matching HA integration for original/3.x-to-4.x migration. Backup formats/rolling-counter restore semantics differ between forks; no migration or backup round trip was tested. [Migration notice](https://github.com/cjkas/ESPSomfy-RTS#readme), [cjkas restore source](https://github.com/cjkas/ESPSomfy-RTS/blob/main/src/ConfigFile.cpp).

Complete raw configured backup under #9 before any installation. Keep #7 open by user instruction. No controller changes, firmware installation, pairing, calibration or motor commands were performed; do not perform physical tests until the user explicitly resumes them.
