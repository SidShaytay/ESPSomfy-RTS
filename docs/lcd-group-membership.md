# Change a group’s controller list

**Navigation version:** These paths apply to **4.9.0 and later**. For 4.8.0 or 4.7.3, see the [versioned path map](lilygo-local-ui.md#navigation-versions).

Use **System → Configure → Rooms, devices & groups → Groups → Advanced**, then select the group.
Unlock with the configured web credential when requested.

## Add a device

1. Scroll past the group’s 32 entry slots to **Add device (metadata)**.
2. Choose a configured device. Devices already listed are omitted.
3. Review the group and device IDs, addresses and radio formats. **Cancel** is selected initially.
4. Select **Add** and press to submit. **Membership saved** means the controller list and model file were saved.

This changes the controller’s list without sending RF or physically pairing a motor.
A listed device need not be marked paired or use the group’s format; existing control
capability checks still apply. Listing a device does not prove physical pairing or
compatible group operation.

## Remove or repair an entry

Select an occupied slot and review **Remove ALL #… entries**. Confirming removes
every occurrence of that ID, including duplicate or missing references. Other IDs
remain in the list. Free slots have no removal action.

An unused **#255** placeholder is a repairable entry, not a configured device.
Its confirmation clears all #255 placeholders. A missing device is identified by
its stored ID without inventing an address or format.

Removal sends no RF and cannot physically unpair a motor. A motor previously
paired to the RF group can continue responding after its controller entry is removed.

## Handle a refused or failed save

Movement, queued work, radio activity, recovery or restart can prevent application.
A changed group/device or expired unlock discards the pending intent. Navigation
and rotation never change membership. Back cancels an action that has not executed;
it does not undo an applied change.

If the list changes but its file save fails, the result offers **Retry model file save**
and **Close**, with Close selected. Retry saves the current model file only; it never
repeats the membership operation or changes pairing. A busy retry requires another
explicit attempt. Queued work expires after five seconds rather than running later.
