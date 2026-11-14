# Calibrate shutter positioning

Calibrate each shutter while standing where you can see its full travel. The controller estimates position from time; ordinary Somfy RTS motors do not send their actual position back.

In the web interface, edit a saved shutter and select **Guided calibration**. On the LILYGO, open **Administration → More → Calibrate shutter**, then select the shutter. Both interfaces use the same guide and saved profile.

1. Choose **Direct travel** for a roller without mechanical play, or **Segmented / slack** when gaps or slack must tighten before visible movement.
2. Open fully and confirm the physical endpoint. Confirm only after looking at the shutter.
3. Measure closing and opening separately. Mark the first visible movement, halfway, the end of visible travel and the final stopped endpoint when those steps are shown. Direct travel skips take-up and settling markers.
4. For segmented shutters, measure reversal play near halfway. The guide stops between directions and asks you to mark the first visible movement after each reversal. Confirming **no reversal play** is an explicit alternative only when that describes the hardware.
5. Return fully open, confirm the endpoint, review the measurements and save. Discard keeps the previous profile. Discard is not Stop: use **Stop moving shutter** first if movement must stop.

Timing starts at actual RF transmission, rather than when you click. No pairing or motor-limit programming is performed. One guide owns calibration at a time; unrelated controller commands are refused while it is active. A physical remote can interrupt the guide. Connection loss or cancellation may leave an accepted movement running to its existing motor limit.

Each member uses its own calibration for individual and group percentage moves. Test 25%, 50% and 75% individually, then as a group, in both directions. Record what you actually see, including stopping differences. Group members can start together and stop at different times.

## Recover an uncertain position

Stopping during take-up or final settling, or an incomplete RF press, can leave mechanical progress unknown. The controller preserves that uncertainty across reboot. Percentage moves are then refused; full open and close commands remain available.

Open the guide, open fully, and confirm the physical endpoint. You can then discard the draft to keep the saved calibration. Entering a numeric position or waiting for an estimated endpoint does not count as observing the shutter.

An endpoint confirmation is an operator observation, not motor feedback. Manual movement, missed RF and changing mechanics can still cause drift. Reconfirm endpoints when needed; recalibrate if intermediate positions remain wrong.

## Capture a test report on release firmware

Open **Radio → Motion logs**, select Motion, Radio and/or Calibration, and choose **Capture for 5 minutes**. Optional USB output writes the same numeric events to the serial connection. Capture is off after reboot, expires automatically and supports at most 15 minutes through the API. It does not change the compile-time SDK logging level.

The web view keeps the latest 500 retrieved lines. The board retains only 32 records; a slow reader is told when records were overwritten. Copy the report before **Stop and clear**. Trace records contain controller shutter IDs, timestamps, RF tickets and numeric values, without radio addresses or credentials. Radio events establish transmission outcomes; they cannot prove the motor received a command.

Use the [release-board test routine](release-board-testing.md) to distinguish automated controller checks from observed motion.
