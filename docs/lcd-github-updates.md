# Update firmware from the LCD

**Navigation version:** These paths apply to **4.9.0 and later**. For 4.8.0 or 4.7.3, see the [versioned path map](lilygo-local-ui.md#navigation-versions).

**Home → Updates** (also **System → Maintenance → GitHub updates**) checks and installs a newer stable release from
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
require a new review. A terminal failure needs a new explicit request; temporary download errors during an active confirmed installation follow the retry rules below.

In 4.7.3 and later, display inactivity dims the screen while keeping update progress or its result visible. It does not return to Home. Ordinary Check or review sessions still expire and show a protected read-only result. Confirmed 4.8.0 installations continue after the menus lock, as described below. Keeping the screen does not extend authentication.

After you confirm **Install and restart**, that exact GitHub operation has up to **30 minutes from the original confirmation** to finish. Inactivity does not cancel it. PIN/password menus still lock after two minutes; progress does not renew access to controls or administration. None mode remains open as configured.

Temporary connection interruptions, incomplete downloads and supported temporary server errors retry automatically within that same window. The display shows the retry count and wait. GitHub throttling delays are respected. Retries keep the same confirmed release and do not reset the deadline. Certificate, identity, digest, malformed-data and flash-write failures stop; writing and activation are never automatically retried.

While unlocked and the operation is reversible, **Cancel update** or Back cancels it. When locked, **Back to unlock menus** leaves the progress view without cancelling. Unlock normally and reopen GitHub updates to view or cancel that same operation. Reopening does not start a new installation or extend its deadline. Changes to credentials or relevant controller configuration invalidate the operation. Controls resume only after cleanup finishes.

At the final restart checks, cancellation is no longer available. Keep power connected. These continuation and retry rules apply to confirmed LCD GitHub installs, not metadata checking or browser manual-file transfers.

After restart, **Verifying new firmware** means that continuous boot-health
checks are still running. Confirmation needs 60 healthy seconds. A failure at
the 180-second deadline starts automatic rollback waiting. Brief footer messages
explain whether it is waiting for shutters, work or a saved state. Once ready,
**Rollback scheduled** precedes a checked restart by three seconds. New movement
postpones it again. An unavailable previous image shows **Rollback needs attention**.

**Controls paused** or **Update needs recovery** means cleanup or activation
could not be verified. Leaving the screen does not release those locks or start
another update. A later check does not silently retry a terminal failure or repeat a flash write.

Use **System → Maintenance → Browser files & Web** for the existing browser backup, restore
and manual firmware file workflows.
