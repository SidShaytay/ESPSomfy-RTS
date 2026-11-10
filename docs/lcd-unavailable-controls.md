# Unavailable controls on the LCD

A device configured with a lamp displays **Light unavailable** on its control
screen. The lamp setting is preserved. This notice offers no button, sends no
command and does not claim that the lamp is on or off. Ordinary supported shutter
controls and STOP keep their existing behavior.

Some garage equipment has an auxiliary lamp, but the existing web lamp button
only changes its appearance. No verified command has been identified for this
project. Sid chose the explicit unavailable notice for issue #25 rather than a
speculative command that could operate the motor.

For a GPIO relay device, Separate/Euro tilt displays an unavailable notice; with
a configured lamp the footer shows **Tilt unavailable; Light unavailable**. All
five tilt modes remain saved and editable, and ordinary supported lift controls
remain available. RF and GPIO-remote tilt behavior is unchanged.

This is a pre-existing limitation of the original web-backed relay driver, not a
missing LCD setting. The original mapping uses tilt direction for Integrated and
Tilt-only; with lift idle, Separate/Euro tilt requests leave the relay outputs
off. Source tracing and fake-GPIO tests characterize this mapping without
claiming physical motor validation. The LCD discloses unavailable tilt rather
than presenting an unsupported command as functional.

Adding new Separate/Euro relay actuation would be a separate driver/wiring
feature. It is not required to port the original web functionality, and no
accepted waiver is assumed. No mode or saved setting is removed.
