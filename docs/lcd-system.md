# System menu

Open **Home → System**. Secured controllers require the configured web credential.
The menu and its transitions are protected from screenshot capture.

**Restart** opens a confirmation with Cancel selected. Turn to Restart now and
press only after that choice has reached the display. Restart interrupts service.
The shared restart service checks storage, current model persistence, movement,
GPIO pulses, queued work and update ownership before accepting the request. A
second check runs immediately before shutdown; the initial scheduled result does
not guarantee that shutdown will happen. A later refusal is displayed while the
result remains open. Resolve its cause before making a new explicit request.
Navigation, Back, timeout, wake and a blocked display transfer never request a
restart. Closing a result does not cancel an already accepted restart.

**Browser files & Web** explains the existing browser workflow for Backup,
Restore, Update Firmware and Refresh Cache. It neither starts a transfer nor
reports its completion. Backups contain sensitive configuration and counters;
keep them private. Shared LILYGO firmware contains its web interface. Follow the
[state-preserving flashing procedure](flashing.md) for recovery; restoring an old
backup is not a routine firmware-update step.

The form allocates no draft memory. Its state is capped at 80 bytes and it borrows
the Runtime's existing administration presentation buffers because the two
screens cannot be open together. One 352-byte current view is built on the loop
stack; it is not another retained framebuffer. Graphics loss, sleep, session or
credential changes and inactivity discard the local screen. They do not undo an
already admitted restart or any operation already performed elsewhere.

Native and production Runtime tests use fake restart/storage/hardware boundaries.
They check default Cancel, exact displayed selections, fresh local authority,
refusal reasons, subsequent status changes, browser guidance and graphics cleanup.
These tests do not access a controller or establish physical deployment safety.
