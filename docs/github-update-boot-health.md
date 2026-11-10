# Canonical update boot health

The shared CC1101 / Plus firmware keeps a newly installed application pending
until it has passed 60 continuous healthy seconds. This applies to its bundled
web interface and controller together. It uses the existing rollback-enabled
bootloader; it does not change settings, counters, partitions or bootloader.

Each observation requires network connectivity, mounted model storage, usable
model/settings/security/radio loads, no recorded settings or explicit-counter uncertainty, a
saved model, registered bundled-web routes, and verified immutable web assets
and bootloader. Enabled radio must be available. The LCD must have initialized
and presented its full graphics frame without a graphics/bus failure. This
checks completed output commands, not physical panel detection.

The pass must finish in under one second, with at least 24 KiB free internal
memory. The global crypto allocator must be initialized. An installer pause,
an updater owner, an unhealthy pass or a gap over one second between health
observations restarts the continuous interval. Sleeping/dimmed LCD operation is
normal. The clock is 64-bit monotonic boot time.

Successful SDK confirmation clears pending state. A failed SDK confirmation
keeps it pending and is retried at most once per second before the deadline.
At 180 seconds from boot this policy records failure and cannot subsequently
confirm that boot. Automatic rollback waits for shutters, queued/radio work and
safe persistence. The LCD status/control footer briefly explains the decision:
“Rollback waiting for shutters”, “Rollback waiting for work”, “Rollback waiting
for saved state” or “Rollback scheduled”. It leaves the target and STOP buttons
visible, preserves private confirmation warnings and never renews a login.

Once ready, the same SafeRestart service saves current state and schedules a
three-second checked restart. Newly detected movement cancels that schedule;
the policy waits again and creates a fresh request when idle. Pending radio
frames and partial receptions are checked after the final save too. Restart
observations run before LCD input, so a cancelled restart does not reject a STOP
press for movement already decoded in that pass.

The source must still be pending verification, the installed bootloader must
match its attestation, and the SDK must report a valid previous image. The
pinned SDK's `esp_flash_partitions.h` documents that a second boot from
`ESP_OTA_IMG_PENDING_VERIFY` becomes `ABORTED`; the rollback-enabled bootloader
then selects the previous valid application. Using the normal checked restart
preserves the existing seals/save/shutdown sequence. No rollback helper bypasses
it with an immediate SDK reboot. An unavailable previous image or invalid clock
shows “Rollback needs attention” and does not force a reboot. Configuration and
counters are not restored from an older backup.

Canonical startup installs the bounded crypto allocation/free pair before
display/capture/web initialization or token HMAC. It then attests the installed
bundle and bootloader and starts the shared updater task without an automatic
network request. Controller preparation runs after accepted normal work drains.
While the installer retains its pause, network/status/UI and cleanup continue;
normal model producers and queue draining wait. Updates are refused while the
current boot is still pending confirmation.

`bash tools/test-update-recovery.sh` exercises the actual recovery policy and
SafeRestart with movement/work/storage delays, new movement, unavailable rollback
and radio input arriving during the final save. Tests never reboot hardware.
`bash tools/test-update-boot-health.sh` compiles the actual startup prefix,
paused-loop body and confirmation block with local SDK boundaries. It covers
continuous health, lost samples, prerequisites, SDK confirmation failures,
deadline intent, clock regression and startup ordering. UI runtime tests verify
that graphics allocation alone is insufficient: a full presentation is needed.
These are offline checks; physical memory headroom, display and radio timing,
power loss and actual bootloader rollback still require authorized hardware
acceptance.
