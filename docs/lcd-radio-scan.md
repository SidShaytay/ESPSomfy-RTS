# Scan the radio frequency from the LCD

**Navigation version:** These paths apply to **4.9.0 and later**. For 4.8.0 or 4.7.3, see the [versioned path map](lilygo-local-ui.md#navigation-versions).

Use **System → Diagnostics → Radio frequency scan** to observe signal strength and copy a
completed scan's strongest frequency into the Radio settings draft.
Unlock with the configured web credential first if security is enabled.

1. Read the warning, turn from **Cancel** to **Start radio scan**, then press.
   Start requires the controller to be idle; refused starts never retry automatically.
2. Watch the current frequency and strongest signal. **Stop scan** ends sampling;
   **Close and stop** or Back also ends your scan.
3. Wait for **Restoring radio** to finish. RF commands remain paused until the
   previous tuning and receive state have been restored.
4. Review the peak. Select **Copy to Radio draft** if you want to use it.
   **Close** is selected by default; no copy is offered without a valid signal.
5. Review the Radio draft and explicitly save it to change the configured frequency.
   Copying alone does not save or transmit.

During a scan, independent wired GPIO controls remain available through other
control clients. RF requests are refused, including requests still queued when
you stop; they do not play back afterward. A separate physical remote can still
move its motor. Signal strength is not evidence that a shutter moved or stopped.

Back, loss of authorization, changed security/radio settings, and LCD inactivity
end your scan. The scan also has a five-minute limit. Another session cannot stop
it or copy its result. If the screen says **Scan needs attention**, restoration
could not be verified; close the screen and inspect the controller state before
starting another scan.
