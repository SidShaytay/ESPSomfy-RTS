# LCD settings forms

**Navigation version:** Paths below describe source `015ac39`, intended for the next release and not yet deployed. Installed 4.7.3 uses the older menus; see the [installed-version path map](lilygo-local-ui.md#navigation-versions).

The local settings component covers General, Network, MQTT, Security and Radio. It reads field lists, lengths, ranges and non-contiguous choices from the same tables as socket settings validation. Ethernet fields appear only on Ethernet-capable builds. The shared board's fixed radio GPIOs and its valid WiFi-only connection setting are shown read-only.

Open **This controller → Configure**, then choose **General**, **Security**, **Network → Network settings**, **MQTT** or **Radio** and unlock with the configured credential when required. The LCD controller connects the form directly to the shared settings operations; it does not use a browser or a socket loopback. Scans, restart, entity administration and the updater use their own flows.

## Editing and saving

The chosen section opens directly. Turn to select a field, then press to edit. Rotation changes the draft only. For numeric values, press to open the actions menu: **Edit value**, **Step** and **Apply to draft**. Step cycles through powers of ten within the field's range, allowing large values such as an MQTT port to be reached quickly. Numbers stop at their shared bounds. Unsupported stored choices and out-of-range or nonfinite numbers show Invalid. Pressing one opens **Cancel / Replace invalid value**, with Cancel selected. Back or Cancel keeps the original and any other field drafts. Explicitly choosing Replace starts a bounded candidate: the first supported choice, or zero when within range and otherwise the minimum. The candidate still needs **Apply to draft**, followed by **Save**; opening the field or choosing Replace alone never changes the original. Frequency retains three decimal places; the other fractional radio values retain two.

For text, turn to select a character, press to append it, or choose Delete byte, Exact byte, Apply to draft or Cancel edit. Exact byte accepts nonzero bytes for values that cannot be entered with the ASCII picker. The screen shows the end of long drafts, an ellipsis when the beginning is clipped, and the current/maximum byte count. Text uses exact bytes, including pre-existing non-UTF-8 sequences; it does not require UTF-8. The renderer handles unsupported glyphs separately from the stored bytes.

**Apply to draft** validates the changed fields with the shared settings validator. It does not persist anything. Select **Save** at the end of the field list to persist. Network, MQTT, Security and Radio show a consequence-specific confirmation with Cancel selected initially. General settings save directly after this explicit Save action. Saving with no applied field drafts closes with “No changes to save” and does not write settings.

Back inside an editor discards that field's current edit and returns to the field list. Previously applied field drafts remain. Back from a confirmation returns to the field list. Back from a section or field list, or selecting Cancel, closes the form and discards all drafts.

Fixed board GPIOs remain read-only even if their stored value is invalid. The existing shared radio save normalizes these GPIOs to the board profile before persistence and live application, so an explicit ordinary radio field Apply/Save also repairs them. On WiFi-only hardware, a valid WiFi Connection is read-only. An invalid Connection offers the explicit replacement flow with WiFi as the only candidate; Ethernet choices are refused by shared validation. Unrelated Network saves preserve the invalid original until you explicitly replace and apply that field.

## Passwords and PINs

Stored WiFi passwords, MQTT passwords, security passwords and PINs are never read into the form. The list shows Set or Unset. Keep removes any pending replacement; Replace starts with an empty, masked editor. Replacement text stays masked, including in long-text previews. A nonempty draft appears as Replace pending.

Blank is shown as `Blank (retain*)` because it follows the existing shared save behavior: blank usually retains the saved secret. Changing a WiFi name or MQTT username can clear its saved secret when the submitted password is blank. There is no separate clear-secret flag. Review related identity fields before saving.

Enabling or changing active PIN authentication requires an effective four-byte PIN. Password authentication requires an effective nonempty username and password. These are the browser's byte/whitespace rules; spaces are not trimmed. Unchanged legacy credentials are preserved. Invalid changes are refused before any persistence or runtime effects.

## Failure and session changes

A failed save retains the draft for an explicit retry or Cancel. A partial save also retains it and reports that durable settings may be partly changed while running settings stay unchanged. Retry before restarting. There is no transaction rollback: a later reboot can load partially saved values. Failed and partial results do not reconnect, reboot or complete security-session changes.

If an earlier action is still queued, Save reports **Busy** and keeps the draft. Wait for those actions to finish, then select Save again or Cancel. The form does not save automatically when the queue clears.

A completed save can report **Settings saved / Restart request blocked** with the actual reason. Queued operations, another pending restart, active storage/update work, settings/model/counter storage problems and other restart safety checks are distinct causes. An unknown reason shows a generic refusal rather than claiming uncertain settings storage. The saved section is complete and its draft is cleared: this result offers no Retry Save and never reapplies the settings. Resolve the displayed cause before explicitly requesting Restart where appropriate; a pending restart means another request already exists. See [settings durability](settings-durability.md) for settings-storage recovery and its limits. A settings save does not repair a model-file or counter-storage failure.

The form closes and erases its drafts on authorization-history changes, a changed section revision, cancellation, timeout or allocation failure. The form inactivity limit is 120 seconds; an active value editor also has its existing 30-second inactivity limit. The backend checks authorization and section history again immediately before the shared device apply operation. A successful save closes the form; the shared operation owns notifications, session revocation and any required restart.

## Memory and integration contract

The form owns one changed-only short-key JSON document. Unmodified values are read into one bounded temporary value at a time. Shared validation uses a transient expanded request document under the same injected allocator budget. Firmware supplies PSRAM-only bounded, wiping storage; the allocator must outlive the form and both documents. Admission failure closes the form without a save. Form state is bounded to 1 KiB, and the renderer receives a separate three-row `FormView` containing no raw secrets. Every form frame is protected from public screen capture.

The parent supplies logical turn/activate/Back/tick events after wake suppression and debounce. A press is accepted only after the exact current form selection has finished reaching the LCD. Entering the form always requires a fresh presentation, even when it looks identical to a previous visit. Results stay protected until acknowledged and a complete non-settings frame replaces them. The typed restart-block reason is part of the exact presented-frame identity: a changed reason must finish reaching the display before its result can be acknowledged.

Leaving Settings, sleep, graphics failure and cancellation discard all drafts. The display's configured inactivity behavior can therefore discard a form before its own timeout. Back from an editor still cancels only that edit; Back from the field list discards the whole form.

Firmware uses one PSRAM-only allocator with a 4 KiB requested-byte cap including its headers. SDK heap metadata and alignment overhead need additional headroom. Offline runtime tests use host JSON pools adjusted to the target's 1 KiB pool request size; this checks the bounded path, not physical heap consumption or timing on the board. The production allocator, renderer, controller and shared save bridge are exercised with fake device operations and GPIOs. No controller, RF or Home Assistant access is involved.
