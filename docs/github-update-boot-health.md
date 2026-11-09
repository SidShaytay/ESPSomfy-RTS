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
At 180 seconds from boot, or on a backward/invalid clock, this policy emits one
rollback-required intent and cannot subsequently confirm that boot. The current
deadline integration records the failure only: managed rollback execution and
its LCD recovery workflow remain unfinished. It never directly invokes an
immediate reboot around movement or persistence checks.

Canonical startup installs the bounded crypto allocation/free pair before
display/capture/web initialization or token HMAC. It then attests the installed
bundle and bootloader and starts the shared updater task without an automatic
network request. Controller preparation runs after accepted normal work drains.
While the installer retains its pause, network/status/UI and cleanup continue;
normal model producers and queue draining wait. Updates are refused while the
current boot is still pending confirmation.

`bash tools/test-update-boot-health.sh` compiles the actual startup prefix,
paused-loop body and confirmation block with local SDK boundaries. It covers
continuous health, lost samples, prerequisites, SDK confirmation failures,
deadline intent, clock regression and startup ordering. UI runtime tests verify
that graphics allocation alone is insufficient: a full presentation is needed.
These are offline checks; physical memory headroom, display and radio timing,
power loss and actual bootloader rollback still require authorized hardware
acceptance.
