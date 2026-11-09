# Manual transfer ownership and restart source

The Firmware, Application and Restore HTTP handlers acquire the existing
`GitUpdateClass` external owner before touching the SDK or `/shades.tmp`.
Each request carries a generation and kind, rather than trusting global SDK
success state. Failed SDK admission does not abort another operation. Each
SDK/file operation holds a short lifecycle claim; no service or TX critical
section is held across I/O.

An abandoned transfer expires after 30 seconds without progress. Cleanup waits
for an active operation to finish. A retired or expired callback cannot touch
another request's SDK operation, temporary file or owner. Restore and Application
recovery carry the exact token through the existing FIFO queue. The existing
Application recovery placement exception is unchanged. An expired marker performs
no recovery I/O and cannot delete a newer restore file.

Successful transfers retain their source while publishing a restart request.
Actual shutdown atomically hands that source to the same service's non-expiring
restart owner before ending the network. A stale source cannot fall back to a
generic restart. Generic restart publishers explicitly clear source metadata and
refuse to overwrite an armed request. Settings save completion returns typed
restart admission; saved settings need not mean a restart was scheduled.

The queue marker is capped at 80 bytes (80 in the host fixture), a 16-byte increase
per marker or 512 bytes across 32 entries on that host ABI. Request state is
bounded to 40 bytes; transfer lifecycle state remains bounded to 48 bytes. There
is no payload buffer or second updater owner.

## Limits of this checkpoint

This change supplies exclusion and truthful request outcomes. It does not certify
raw Application image compatibility with the mounted filesystem, remount/recovery
policy, installer assets, or complete model/settings durability at restart. The
existing restore persistence implementation has not become a checked transaction.
Those requirements remain separate from ownership.

Explicit counter callers still use the legacy setter in this checkpoint. The
counter-uncertainty query is a prerequisite for later checked domain wiring, and
currently has no production writer. No counter correction UI is enabled here.
The loader's maximum-of-file-and-NVS anti-stale counter policy is unchanged.

## Offline verification

Production upload callbacks run with fake SDK/filesystem boundaries and the real
GitUpdate service and LoopQueue. Cases cover overlapping requests, failed SDK
begin/write/end, expiry during I/O, late callbacks, full queue, short restore
writes, stale restore markers and final source handoff. Native tests cover typed
Settings outcomes and stale recovery; the shutdown harness replaces only the
physical shutdown boundary. No controller, serial connection, Home Assistant or
radio hardware is accessed.
