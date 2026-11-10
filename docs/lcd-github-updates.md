# Update firmware from the LCD

**Navigation version:** Paths below describe source `015ac39`, intended for the next release and not yet deployed. Installed 4.7.3 uses the older menus; see the [installed-version path map](lilygo-local-ui.md#navigation-versions).

**This controller → Maintenance → GitHub updates** checks and installs a newer stable release from
`SidShaytay/ESPSomfy-RTS` without a computer. The shared CC1101 / Plus image
includes the controller, LCD and web interface together.

The shared canonical firmware must already have a compatible layout/profile.
Eligible release assets must be published in that repository; current main is
not automatically an available release. Development identities, incompatible
hardware/assets or unavailable verification show an unavailable/refused result.
Checks use verified HTTPS, repository metadata and file digests; there is no
browser companion or separate LCD web-image upload.

1. Unlock with the controller's configured PIN or password, if enabled.
2. Choose **Check GitHub release**. Opening the screen starts no download.
3. In 4.7.3 and later, an eligible result opens its review automatically with **Cancel** selected. Read the full release version. Earlier builds require choosing **Review release** first.
4. Turn from the default **Cancel** to **Install and restart**, then press.
5. Keep power connected while the controller verifies and installs the update.

Installation saves the current model and checks that movement and pending work
have finished. If the release or controller state changes, it can refuse and
require a new review. No failed operation retries automatically.

In 4.7.3 and later, display inactivity dims the screen while keeping update progress or its result visible. It does not return to Home. An expired session shows a protected read-only result; wake the display, then press **Back: System** to leave. This does not extend authentication.

During reversible work, **Cancel update** or the Back button cancels the
operation. Controls resume after cleanup finishes. The Cancel action disappears
during final restart checks; Back then leaves the status screen. The original
session must remain valid, and progress updates do not extend its two-minute
inactivity timeout.

After restart, **Verifying new firmware** means that continuous boot-health
checks are still running. Confirmation needs 60 healthy seconds. A failure at
the 180-second deadline starts automatic rollback waiting. Brief footer messages
explain whether it is waiting for shutters, work or a saved state. Once ready,
**Rollback scheduled** precedes a checked restart by three seconds. New movement
postpones it again. An unavailable previous image shows **Rollback needs attention**.

**Controls paused** or **Update needs recovery** means cleanup or activation
could not be verified. Leaving the screen does not release those locks or start
another update. A later check does not silently retry the failed installation.

Use **This controller → Maintenance → Browser files & Web** for the existing browser backup, restore
and manual firmware file workflows.
