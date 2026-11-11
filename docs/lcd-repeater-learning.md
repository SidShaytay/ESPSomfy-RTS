# Add a repeater from the LCD

**Navigation version:** These paths apply to **4.9.0 and later**. For 4.8.0 or 4.7.3, see the [versioned path map](lilygo-local-ui.md#navigation-versions).

Use **System → Configure → Rooms, devices & groups → More → Repeaters**, then **Learn/add repeater**.
This candidate is offline-tested and awaiting independent review/integration; no
physical repeater behavior has been tested.

A repeater retransmits future frames received from the selected remote address.
Use an address on only one repeater controller: unnecessary repeating can create
radio congestion or echoes. Adding a repeater is separate from pairing a motor.

1. Unlock with the configured web credential when requested. A full seven-entry
   list refuses learning until an entry is removed.
2. Choose **Begin observation** deliberately; Cancel is selected initially. The
   bounded receive-only window lasts 30 seconds and keeps at most eight candidates.
   Entering the list or receiving the first frame never adds a repeater.
3. Operate the intended physical remote only when doing so is safe, then select
   its received candidate. The protected review shows its address and observed
   frame information. Those observations do not prove motor reception.
4. Review the future-retransmission warning. Choose **Add repeater** explicitly;
   Cancel is initially selected. The queued addition expires after five seconds
   and rechecks authorization, the current repeater list and the selected frame.
5. A successful save records the address. No motor command or rolling-code write
   is issued by the addition itself. Future received frames may now be repeated.

Duplicate addresses, a full list, malformed or changed observations, expired
unlock, other work or a changed list refuse the addition. There is no automatic
retry. Back ends observation and returns to the repeater list; another Back
returns to Administration’s More page.

If the address is added in memory but its model-file save fails, repeating can
already be active. **Close** does not undo that addition. The result initially
selects Close and offers **Retry model file save**; retry saves current state only
and cannot add again, transmit a programming command or rewrite a rolling code.
Use the separate confirmed Remove action to disable the configured repeater.
