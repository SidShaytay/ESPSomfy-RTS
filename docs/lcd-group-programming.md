# Pair or unpair a motor with a group

**Navigation version:** Paths below describe source `015ac39`, intended for the next release and not yet deployed. Installed 4.7.3 uses the older menus; see the [installed-version path map](lilygo-local-ui.md#navigation-versions).

Use the LCD to send the motor and group programming requests, then separately save the group’s controller list. The guide is integrated and independently reviewed with offline input/renderer/adapter/FIFO coverage; physical motor programming remains unverified.

## Before sending

Read the motor’s pairing instructions. Ensure only the intended motor is in learn mode: **the group programming request is broadcast**, and other motors in learn mode may also be affected. The controller cannot check physical learn mode or membership.

The guide offers configured RF devices with the same protocol and effective 56/80-bit format as the group. Pair offers devices absent from the controller list when a slot is free; Unpair offers listed devices. This list is configuration, not proof of motor membership. GPIO devices remain configured and have no RF programming action.

## Follow the guide

1. Open **This controller → Configure → Rooms, devices & groups → Groups → Advanced**, select the group, then **Motor programming**. **Member links** is the separate metadata-only list editor. Unlock with the configured web credential when requested.
2. Choose **Pair motor to group** or **Unpair motor from group**, then select the device. Reviews identify both the group and device by ID and shortened name.
3. Follow the motor’s instructions to enter learn mode. If you use a physical remote, select **I observed learn mode** after checking the motor yourself. Alternatively, choose **Review LCD open memory**: its default-Cancel review offers the selected device’s Prog request with 40 repeats. This may jog the motor; on a one-button motor it may operate the motor. Send only when appropriate, then confirm that you observed learn mode.
4. Review the separate group Prog request with one repeat. Cancel is initially selected. Check that no other motor is in learn mode before choosing **Send group Prog**.
5. **Group request accepted** means only that the controller accepted the request. A jog can mean either adding or removing a controller. Verify the intended physical result using the motor’s instructions before choosing **I verified intended result**.
6. Review **Controller list only**. Separately choose **Save list: Add** or **Save list: Remove** to save the membership change. This sends no RF. Removal clears all occurrences of that device ID.

## Cancel or recover from a refusal

Back or Cancel cancels a request that has not executed. It cannot recall accepted frames or close motor learn mode already opened; follow the motor’s instructions to leave learn mode. The guide never sends an automatic compensating command or retries RF.

Changed authorization, device/group configuration, incompatible format, movement, queued work, radio activity or an active update can refuse a request. Queued actions expire after five seconds. Inspect the physical motor state before deliberately trying again.

If the list changes but its file save fails, **Retry model file save** saves the current model file only. It cannot repeat programming or list changes. Close is selected initially; a busy retry requires another explicit attempt. The existing metadata repair flow remains available for missing controller-list references.
