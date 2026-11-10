# Receive diagnostics on the LCD

**Navigation version:** These paths apply to **4.8.0 and later**. For 4.7.3, see the [versioned path map](lilygo-local-ui.md#navigation-versions).

Open **This controller → Diagnostics → Receive diagnostics**, then explicitly select **Begin observation**.
Cancel is selected first. Opening the screen does not start observation or change
the receiver mode, radio settings, model, counters or motor commands.

The log keeps the latest **two** received frames, including invalid frames and
frames observed during a frequency scan. Choose a frame to pause this log and
inspect its frozen copy. Details include the address, rolling code, observed
protocol and frame length, decoded and raw commands, encoding key, synchronization,
signal strength and scan/validity flags. Valid means the decoder accepted the
frame; it does not acknowledge motor movement. An unavailable RSSI sample is
labelled **Unavailable**.

**View pulse timings** shows up to 300 copied timing values in microseconds.
Each row holds two complete 32-bit values. Rotate to browse; Back returns to the
frame. Missing or clipped source timings are identified rather than filled with
invented values. Browsing has no transmit or programming action.

Back from a frame returns to the paused log. **Resume observation** is explicit.
**Clear view** has a separate confirmation with Cancel selected first; it erases
only this log. It does not reset configuration, counters or the web browser's
separate log. Pausing or clearing the LCD log does not change receiver mode or
other controller processing. Existing controller/radio availability and model
quarantine rules still apply.

These screens require the configured local credential and are protected from
screenshot capture. Leaving the log, sleep, loss of graphics, changed authority
or 120 seconds without user input detaches the observer and wipes its private
storage. Received traffic does not extend the inactivity deadline. A frozen
frame is a copied observation, never authorization to link a remote or send RF.

The lazy handle is at most 32 bytes. Its wiping PSRAM session, including both
complete timing records and allocator overhead, is bounded to 3,072 bytes. It
borrows the existing protected administration presentation frames. No extra
framebuffer, filesystem, transfer session or controller access is required.

Offline tests exercise default Cancel, bounded history and copied pulses, full
32-bit values, unknown/invalid/scan metadata, incomplete timings, expiry, authority
changes, allocation refusal and exact observer teardown. Actual encoder/Runtime
and GFX checks cover unseen Start, frozen details, capture protection and complete
display of timing and sequence values using synthetic receive events.
