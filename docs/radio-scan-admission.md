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

The scan driver remains the owner of its active indication. These changes add
no second scan engine or configuration writer. Full authenticated browser/LCD
scan ownership, bounded result review and explicit Apply remain unfinished in
this feature branch. No physical scan or RF test has been performed.
