# Separate web updates on generic boards

This guide applies to generic builds that keep web files in LittleFS. T-Embed
CC1101 and CC1101 Plus use the same bundled firmware image; their separate
Application upload returns HTTP 410. Use the firmware update flow on those boards.

## Updating from the browser

1. Download a fresh configuration backup. Keep its counters private; an older
   backup is not automatically safe to restore.
2. Select the matching full-size `.littlefs.bin` image in Application Update.
3. Choose Upload File. The browser first requests preparation. The controller
   waits for accepted work, verifies a save of current settings, pauses RF, and
   waits for open web files to close before unmounting storage. Independent wired
   controls remain available. No image bytes are sent before readiness.
4. Keep the page open while the file uploads and recovery finishes. The page
   reports storage recovery and the verified current-state save separately from
   receiving the image. The controller keeps the current settings and counters
   in memory and saves them after remounting; it does not load the image's old
   configuration.
5. A complete authenticated upload permits one automatic restart attempt after
   recovery and the normal idle/save checks. If it is refused, review the reason
   and explicitly request Restart when appropriate. There is no deferred restart
   permission waiting for a later movement to finish.

Closing the dialog during preparation cancels that source when its identity is
known; an abandoned preparation also expires. Closing during upload aborts the
browser request and asks for reducing cleanup. Neither action undoes bytes
already written. Closing a completed upload's status view ends observation.

If recovery needs attention, resolve the reported problem and choose **Retry
recovery**. This only restores storage access, releases the radio pause and saves
current settings. It cannot send another image or schedule a restart. A failed
mount never triggers formatting. Preserve the current controller state and use
the [documented recovery procedure](flashing.md) if recovery cannot be completed.

A lost connection or stale status does not prove success or failure. Reconnect
and review the exact operation; the browser never automatically resends a file.
The existing HTTP transport does not protect credentials or backup contents in
transit; this flow does not add encryption.

## Existing scripts and older web pages

Direct unprepared POSTs to `/updateApplication` are refused before SDK writes.
An older web page that lacks preparation cannot use its old upload button with
this receiver. Update the matching web assets through the prepared HTTP contract
below or the existing verified offline recovery procedure. Do not bypass the
checks by writing a mounted filesystem.

All requests use the existing `apikey` authentication header:

| Request | Purpose |
| --- | --- |
| `POST /prepareApplication?bytes=<exact image size>` | Queue preparation; returns `nonce`, `generation` and `bytes`. No file or upload-disconnect hook. |
| `GET /applicationUpdateStatus?nonce=<nonce>&generation=<generation>` | Observe only that exact source. Wait for `phase: ready` before sending bytes. |
| `POST /updateApplication?nonce=<nonce>&generation=<generation>` | Send one multipart `file`, matching the prepared full-partition size exactly. HTTP 202 confirms recovery handoff, not completed save or restart. |
| `POST /applicationUpdate?nonce=<nonce>&generation=<generation>&action=cancel` | Cancel preparation or an unfinished receive; cleanup remains owned. |
| `POST /applicationUpdate?nonce=<nonce>&generation=<generation>&action=retry` | Explicitly retry a source in `attention`; grants no receive or restart permission. |

Keep `nonce` and `generation` as canonical decimal **strings**, including in
JavaScript; 64-bit values can exceed JavaScript's exact numeric range. Compare
both identities and the byte count on every response. Poll until `completed`,
`refused` or `attention`, and inspect `uploaded`, `restartScheduled`,
`restartReason`, `fault` and `desc`. Authentication and security history are
rechecked on receiving and completion. Preparation and abandoned transfers are
bounded by the existing 30-second no-progress lifecycle.
