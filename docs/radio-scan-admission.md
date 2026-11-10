# Radio scan admission

A frequency scan begins only after the loop queue has no accepted work or
in-flight publisher and the existing radio scheduler/receive checks are quiet.
The loop-queue seal is held only across Begin admission and released afterward;
it does not freeze independent GPIO controls for the scan's duration.

Begin returns success only when the driver actually starts scanning. Refused
web starts report Busy instead of an empty success. RF admission stays paused
until tuning and the previous receive state have been restored.

Queue markers record whether a scan was active at publication. On loop, current
RF targets and unpair operations received during scanning are refused before
execution or head-of-line holding, including if Stop completed before draining.
GPIO targets retain normal execution. Explicitly RF local markers are refused
at publication; the queue never reads the mutable model from a producer task.
This prevents a delayed unpair from transmitting when the scan ends.

The scan driver remains the owner of its active indication. A loop-owned session
uses the existing configuration owner and binds a scan token to the browser
client or authenticated LCD session, security history and radio revision.
Ownership is retained until actual driver restoration completes. Foreign or
stale Stop/result requests cannot operate a later scan. Source revocation,
backwards time and the five-minute limit request cleanup, not an automatic retry.

The LCD and browser offer explicit Start, Stop and completed-result review.
A fresh result check precedes copying to the Radio draft; saving remains separate.
See [LCD radio scan](lcd-radio-scan.md) and the protocol reference. No physical
scan or RF test has been performed for this change.
