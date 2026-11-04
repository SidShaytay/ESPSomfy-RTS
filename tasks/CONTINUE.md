# Continue the lilygo shutter setup

Resume on **vega**, `/var/mnt/data/projects/personal/ESPSomfy-RTS`. Read `AGENTS.md` and this file; GitHub Issues tracks work and `LILYGO.md` documents build/recovery. User prefers small physical steps and familiar names.

## Next actions and authority

User requested: fully close both shutters for bedtime; back up NVS, calibration and radio settings locally; commit/push all port changes, create PR and update main; continue remaining GitHub issues. **No more calibration or other motion tests tonight. Keep #7 OPEN explicitly**, including calibration evidence. Bedtime Both rooms DOWN was sent successfully; physical full closure was not separately observed.

Current work: publishing the reviewed port. Verify current branch/PR state before proceeding. Builds and reviews passed; see validation below. Raw configured NVS/full-flash backup **requires data USB reconnection**: no `/dev/serial/by-id` or ttyACM/ttyUSB was present. User was asked asynchronously. HA API returns401; authenticated setup and iPhone HomeKit pairing remain unavailable. Continue these when access exists; do not close blocked acceptance items.

## Verified controller state

- lilygo: LILYGO T-Embed CC1101 Plus external antenna, headless; dark display expected. UI `http://192.168.1.187`. Radio433.42MHz,56-bitRTS, initialized; fixed pins/profile verified on hardware.
- Individual **Master Bedroom** (shade1), **Master Bathroom** (shade2), and native group **Both rooms** (group1, members1/2) use distinct controller-generated identities. All pairing first/second jogs were user-confirmed. Individual/group UP/DOWN/mid-travel STOP passed. Old remote retained: channel1 both rooms,2 Bedroom,3 Bathroom; all checked after pairing. All LEDs together select independent channel5, apparently unused.
- Exact shade/group/radio snapshots matched before/after reboot; post-reboot group DOWN/STOP was user-confirmed. Last individual counters13/5; group counter15 after bedtime DOWN. No further RF commands needed.
- Saved rough travel times: Bathroom down19300ms/up21150ms; Bedroom down22100ms/up23750ms. Bedroom derived as Bathroom+2.8s down/+2.6s up; timed first arrival only. Percentage accuracy and reboot after saving calibration remain untested.
- Existing Bedroom upper-end oscillation predates lilygo. Research: Somfy staff report suggests brake fault, not a diagnosis: https://forum.somfy.fr/questions/960717-lt50-instable-position-haute-pourtant-fin-course-bien-regle . Identify model/gravity drift vs powered reversal and warranty later. No resets/limits changed.
- Initial UP/MY failure: software position0 made existing handler treat shutter as idle; with favorite unset it suppressed MY transmission. Later mid-travel STOP passed. RTS positions are estimates; old-remote identities have not been linked for tracking.

## Private backups and verification

All private artifacts: `~/.local/state/espsomfy-lilygo/`. Factory full16MiB image: `factory-2026-11-03/`, verified SHA256; predates configuration. Configured export: `configured-20261104T063124Z/` with SHA256SUMS, snapshots, firmware/manifest and RECOVERY.md. Format24 record lengths, identities and calibration verified. **Export excludes Wi-Fi/MQTT/security credentials; raw NVS absent. No destructive restore tested.** Reboot evidence: `pairing-check-20261104T062010Z/`.

Builds: `bash tools/build-lilygo.sh`; pinned cache `~/.cache/espsomfy-lilygo/`. Fresh LILYGO firmware/LittleFS and generic ESP32/C3/S2/S3 builds passed. Host tests passed through `podman exec ... rocm-7.2.3`; wrong-switch mutation failed as expected. No flash writes tonight. Only custom firmware-only updates preserve this profile and existing NVS/LittleFS; never blindly restore stale rolling counters.

Tracker: #1 parent, #2–#6 closed, #7 intentionally open, #9 backup incomplete pending USB, #8 HA/HomeKit pending authentication. Public updates omit credentials/device identifiers.
