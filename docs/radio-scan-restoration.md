# Ending a frequency scan

Ending an active scan restores the configured frequency and receive bandwidth.
It restores reception only if reception was active when that scan began;
otherwise it leaves the radio idle. Starting the same active scan again does not
replace that original receive state.

This cleanup does not save settings, run full radio initialization or retry its
persistence markers. If the radio lock is busy, Stop prevents further scan tuning
and the main loop retries cleanup when the lock becomes available. Applying a chosen frequency remains a separate Radio settings save.
The actual-source offline harness is `bash tools/test-frequency-restore.sh`.

This fix covers restoration only. Exact scan ownership, fresh idle admission,
preservation of accepted work and the standalone LCD scan workflow remain
required work for issue #25. The existing scan-start behavior is not qualified
by this cleanup test. No controller or RF test was performed.
