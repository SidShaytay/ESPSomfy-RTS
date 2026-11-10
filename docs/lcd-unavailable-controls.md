# Unavailable controls on the LCD

A device configured with a lamp displays **Light unavailable** on its control
screen. The lamp setting is preserved. This notice offers no button, sends no
command and does not claim that the lamp is on or off. Ordinary supported shutter
controls and STOP keep their existing behavior.

Some garage equipment has an auxiliary lamp, but the existing web lamp button
only changes its appearance. No verified command has been identified for this
project. Sid chose the explicit unavailable notice for issue #25 rather than a
speculative command that could operate the motor.

If a configured GPIO relay device also has an unsupported separate/Euro tilt
mapping, the footer shows **Tilt unavailable; Light unavailable**. Its configured
modes remain saved. Those electrical mappings are still under investigation;
the notice does not establish working tilt hardware support.
