# Issue 23: native group percentage moves

Implement [issue 23](https://github.com/SidShaytay/ESPSomfy-RTS/issues/23) offline. Firmware branch `codex/issue-23-group-target` is based on standard-layout commit `7a8e529`, in `/mnt/data/projects/personal/ESPSomfy-RTS/issue-23-group-target`. HA branch of the same name is in `/var/home/sid/.local/state/espsomfy-ha-homekit/issue-23-group-target`, based on endpoint workaround `c6c5ab4`.

No service window is authorized. Do not access the controller, USB/serial, live HA, or send RF, flash, restart, deploy, pair, or calibrate. Deployed revisions remain unverified because live inspection is outside authorization. Sid authorized pushing both feature branches after verification; this does not authorize deployment. Preserve existing worktrees and private recovery material.

## Implementation

- [x] G23-1: native shared starts, per-member timed Stops, individual fallback, repeated endpoints, queued-work cancellation and state updates. Timing follows actual TX start/completion; unsupported motor modes are refused.
- [x] G23-2: explicit schema-advertised `groupTarget` operation 55 (`g`, `t`), strict validation, bounded per-connection replay protection, fixtures and protocol documentation.
- [x] G23-3: HA capability-gated native operation, older-firmware fallback, refusal handling and percentage readback tests. Commit `34eb042` pushed to the HA fork's `codex/issue-23-group-target` branch; remote SHA verified.
- [x] G23-4: independent review and integrated host harness completed; firmware implementation committed as `ecd84b1` (scheduler `b8abf4e`). Final native suite: 682 passed, one optional private-fixture test skipped. LILYGO Plus, ESP32, C3 and S3 firmware builds and LILYGO LittleFS build all passed. Both feature branches are to be pushed and their remote SHAs verified before reporting completion.
- [!] G23-5: physical validation. Need: Sid's explicit service-window authorization. No hardware or live-service access occurred during implementation.

Next: wait for service-window authorization. Do not merge, close issue 23, or infer RF reliability from software tests.

## Contract and evidence

`groupTarget` uses logical closed percentage: 0=open, 100=closed. HA sends `100 - HA position`. Ordinary member stored target equals this value independent of `flipPosition` (HA readback cancels the display flip); awnings invert the stored target. Wire direction accounts for `flipCommands`. Native starts affect every member, so mixed direction/already-correct members require independent planning.

HA: 142 tests passed; nine HA contract cases also passed against the actual new firmware fixtures. Web: 63 tests passed. Manifest tooling: 24 tests passed. Board/reserved-GPIO harnesses and release-version script passed. Independent review findings were fixed; mutation checks caught missing eligibility, flag supersession, capability and stale-ID guards.

Use `PATH=/var/home/sid/.cache/espsomfy-lilygo/native-toolbin:$PATH` and `/var/home/sid/.cache/espsomfy-lilygo/pio-python/bin/pio` for native tests. The pinned firmware build tool is the same `pio`; use only build targets. See `docs/PROTOCOL.md` for interruptions, initial/Stop completion, partial admission and unsupported configurations.

Once authorized, capture fresh revisions/configuration, deploy the reviewed firmware and HA revisions with recovery precautions, and compare native UI, HA arrows, HA slider and Apple Home at endpoints and an intermediate percentage. Include repeated endpoints and mid-move Stop/replacement. Record observed motor outcomes separately from estimates and accepted transmissions.

Next ID: G23-6.
