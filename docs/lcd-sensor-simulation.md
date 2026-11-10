# LCD sensor simulation

**Navigation version:** Paths below describe source `015ac39`, intended for the next release and not yet deployed. Installed 4.7.3 uses the older menus; see the [installed-version path map](lilygo-local-ui.md#navigation-versions).

Sensor simulation sends Sunny/Windy radio status through the controller's
existing sensor-command handler. A motor may react through its configured
weather or sun automation. This is an active command, not a sensor reading or a
harmless radio probe.

Open **This controller → Configure → Rooms, devices & groups → Devices → Advanced**, select the device, then
**Sensor simulation**. For a group, choose **Groups → Advanced**, select the
group, then **Sensor simulation**. The group's **Members** entry remains
available for reviewing and repairing membership.

Administrator unlock is required when web security is configured. Supported RF
targets use the existing protocol and frame-length settings. Wired GPIO targets
show this operation as unavailable; their ordinary wired controls are retained.

1. Choose **Sunny** and **Windy** independently. Press cycles **Keep → Off → On**.
   Keep leaves that status unchanged in the shared handler at send time; it does
   not confirm what a physical weather sensor is reporting. Both initially say
   Keep, and no request can be sent until at least one is chosen explicitly.
2. Set **Repeats** if needed. Turn to adjust within 1–127; Press or Back returns
   to the draft. Editing and navigation send nothing.
3. Select **Review and send**. The confirmation shows the target, both statuses
   and the repeat count in full, with **Cancel** selected by default. It reminds
   you that the command may trigger motor automation.
4. Select **Send sensor status** only when ready. The existing FIFO rechecks the
   exact target/membership, authorization, idle state, radio availability and
   operation ownership. A queued request expires after five seconds; Cancel or
   Back removes it before execution.
5. **Request accepted** means the shared handler accepted the request. It is not
   confirmation of RF reception or physical motor movement. Observe the actual
   result. Close/Back cannot undo an accepted RF job or its selected repeats.

Busy, unavailable, changed or expired requests do not retry automatically. Close
and review the target again before deliberately sending a new request. Unknown
group movement is refused using the existing group-readiness rules. The flow does
not pair hardware, change paired markers, invent a raw identity or reset counters.
It uses the same typed shade/group sensor requests as the web interface.

Offline verification covers request routing and bounds, default Cancel, unchanged
Keep drafts, queued cancellation/expiry, changed authorization/context, radio
backpressure, owner and motion refusals, and the actual encoder/display/runtime
path with a fake RF sink. Development tests do not transmit RF or establish a
motor's physical behavior.
