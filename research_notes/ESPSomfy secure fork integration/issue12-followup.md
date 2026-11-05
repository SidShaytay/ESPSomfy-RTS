## Compatibility and security follow-up

The user's priority is to retain device-side GitHub release downloads and the proven Plus profile while adopting maintained security improvements.

- PR #10 does not apply directly to cjkas v4.0.2: read-only apply checks fail after its source split/moves. Board knowledge is reusable, but Arduino3 SPI ownership, radio locking/scheduling, partition layout and HA API changes require a forward port. [Driver](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/lib/SmartRC-CC1101-Driver-Lib/ELECHOUSE_CC1101_SRC_DRV.cpp), [counter policy](https://github.com/cjkas/ESPSomfy-RTS/blob/v4.0.2/src/RollingCode.h).
- Recommended long-term path: keep this repository, adopt a cjkas baseline, port the small Plus profile and add a separate secure background GitHub updater. Existing-base selective hardening is a bounded interim option. Do not restore the old blocking GitOTA unchanged. [Removal rationale](https://github.com/cjkas/ESPSomfy-RTS/blob/main/docs/adr/0007-no-github-update.md).
- Require authenticated enqueueing, verified HTTPS, exact board/partition manifests, trusted hashes/signatures, bounded streaming and persistent-counter preservation. Begin with firmware-only inactive-slot updates; shared LittleFS updates need a separate consistency/rollback design.
- The recalled Daybreak Blue report has not been located. Daybreak Blue is an OpenAI access program; no scan was run here. Closest verified material: cjkas [SECURITY_PLAN](https://github.com/cjkas/ESPSomfy-RTS/blob/main/docs/SECURITY_PLAN.md), [PR #65](https://github.com/cjkas/ESPSomfy-RTS/pull/65) and [PR #64](https://github.com/cjkas/ESPSomfy-RTS/pull/64). Do not call the earlier source review a Daybreak scan.

Suggested PR sequence: board/build + latest-state import; security finding/fix matrix; fork versions and board-specific releases; authenticated firmware-only pull OTA; later filesystem update/recovery. Format24 reader support is not import validation, and cjkas's lower-counter restore behavior needs explicit handling.

No implementation, build, flash, device/network or motor changes were made. Preserve #7 as open and require explicit user resumption before physical verification.
