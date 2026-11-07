# Display deployment record

The authoritative task record is [the display feature task](../main/TASKS.md) at `/var/mnt/data/projects/personal/ESPSomfy-RTS/main/TASKS.md`.

Installed firmware source: `b1ad2e4`, combining completed display feature `ce5f559` with the previously installed issue-23 group firmware. Newer commits on this deployment branch change documentation only. Physical controls are enabled; no assistant RF/motor command or HA service was sent.

Fresh verified full16MiB recovery, exact binaries/ELF and read-only installation evidence are selected by `~/.local/state/espsomfy-lilygo/LATEST_UI_DETAILS_POWER_INSTALL`. The actual board reported battery100%,4146mV, ready radio433.42MHz, unchanged state/counters/settings and stable uptime beyond the OTA confirmation window.

The completed display feature is integrated into local main. Unfinished issue-23 work remains on this deployment branch; its physical reliability is not established. No further hardware update is pending.
