# WiFi scan lifetime

The browser's existing WiFi Scan action still requests a scan and receives its
results asynchronously. Starting a scan does not save a network or choose an
SSID for connection. Existing automatic connection and roaming behavior remains
separate.

Client scans wait for a roaming scan or connection attempt to finish. They also
wait while update, restore or other maintenance owns the device. A browser
request that cannot start within 30 seconds fails instead of starting much later.
Repeated requests during an active scan share its results and do not schedule an
extra scan.

One shared configuration-owner claim spans the physical scan and result cleanup.
A failed start releases only that claim; it does not delete another scanner's
results. A completed scan publishes its results before deleting SDK storage.

The local result feed is loop-owned. It captures a subscriber's exact generation
when the physical scan starts. A cancelled or replaced subscriber cannot inherit
that scan's later completion. Results are borrowed during a synchronous callback;
consumers must copy only their explicitly admitted bounded data. Cancelling a
local subscription stops future local work, but an already running shared scan
may finish normally. It never becomes an automatic second scan.

This backend checkpoint does not yet expose an LCD scan menu. The LCD workflow
will copy a bounded list into wiping PSRAM and require explicit selection before
opening a Network settings draft. Saving that draft remains a separate action.

Run `pio test -e native -f test_wifiscanfeed -f test_wificlientscan` for token,
owner, expiry, failed-start and repeated-request cases. `bash tools/test-wifi-scan.sh`
compiles the actual network adapter with fake SDK and protocol sinks plus the
real maintenance owner. These checks perform no network scan or controller work.
