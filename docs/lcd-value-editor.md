# LCD value draft API

`LocalValueEditor.h` provides one bounded local draft for the setup forms in issue 25.
It has no settings, persistence, network, GPIO or RF dependency and performs no domain
operation. The form owner validates and applies a result through the shared loop-owned
service only after an explicit Apply and a successful `take*` call.

```cpp
LocalUi::ValueEditor editor;
editor.beginText(settings.hostname, 31, LocalUi::TextEncoding::utf8, false, now);
// Physical-input owner supplies debounced, wake-filtered events:
editor.rotate(+1, now); // select a character; changes only the draft
editor.press(now);     // append selected byte / erase / enter byte mode
editor.apply(now);     // only from the presented Apply action
char hostname[32];
if(editor.takeText(hostname, sizeof(hostname))) {
  // Revalidate authorization, entity/settings generation and the shared schema,
  // then call the domain save service. Report its actual result.
}
```

The API is concrete enough for hostname/name/SSID, travel times, RF tuning, booleans and
enumerated setup fields. `begin*` rejects invalid initial values and discards any previous
draft; it never silently truncates or retargets an initial value. No renderer files or
physical input handling are changed by this module.

| Draft | Begin operation | Applied result |
|---|---|---|
| Text | `beginText(original, maxBytes, encoding, masked, now)` | `takeText(buffer, capacity)`; exact bytes with terminator |
| Integer | `beginInteger(value, minimum, maximum, step, now)` | `takeNumber(int64_t&)` |
| Two decimals | `beginFixed2(doubleValue, minHundredths, maxHundredths, stepHundredths, now)` | `takeNumber(int64_t&)`, in hundredths |
| Boolean | `beginBoolean(value, now)` | `takeChoice(uint8_t&)`, zero or one |
| Enumerated choice | `beginChoice(index, count, now)` | `takeChoice(uint8_t&)`, selected list index |

For RF frequency, `beginFixed2(433.42, 30000, 92800, 25, now)` edits MHz in 0.25 MHz
increments and returns 43367 after one positive turn. Fixed-point bounds/step are scaled
integers; NaN, infinity, overflow, reversed bounds and out-of-range originals fail.
Integer stepping saturates at its validated limits without signed overflow, including
`INT64_MIN`/`INT64_MAX`. Choice lists stop at their ends; the owner maps an index to its
actual enum value rather than assuming every protocol enum is contiguous.

## Text, exact bytes and secrets

The caller supplies a terminated model/form string readable through its bound plus one.
At most 64 bytes are owned per text value, with separate original/draft buffers. Unchanged
original bytes are retained exactly, including legacy malformed UTF-8. Edited text in
`TextEncoding::utf8` must be valid UTF-8 when Apply is requested; an incomplete multibyte
sequence remains an editable draft and reports `invalidUtf8`, without replacement or
substitution. `TextEncoding::bytes` permits exact non-NUL bytes for fields whose existing
schema treats them as bytes, such as SSIDs. It does not relax their domain validation.

`LocalCharacterPicker.h` isolates the shared ASCII/Byte rotary selection: printable
ASCII 32–126, Erase byte, Byte 01–FF, Apply and Cancel. Byte mode excludes NUL, which is the
stored model's string terminator. Counts and capacity are **bytes**, not codepoints.
Erase removes one byte; an edited UTF-8 value must be complete before Apply succeeds.
A full buffer reports `full` and keeps its previous bytes. No output is truncated:
`takeText` with an undersized buffer fails without changing the destination or consuming
an applied result.

`preview(buffer, capacity)` is the renderer accessor. It masks every byte when the draft
is marked secret and never reveals the original or typed secret. `masked()` tells the
owner to protect the whole entry screen and its presentation transitions. Nonsecret
UTF-8 previews avoid splitting a valid codepoint at the output buffer boundary; the
production renderer must still sanitize unsupported/control glyphs for its font/layout.
The module exposes no raw draft accessor. `length()`, `maximumBytes()`, `selected()` and
`byteMode()` provide nonsecret input/count metadata.

For a stored password/PIN/passphrase replacement, start with an **empty** masked draft;
do not prefill stored secrets. The owner's retain/replace/clear action determines the
shared service request. Test fixtures use synthetic secrets only. On successful `take*`,
the caller owns the result and must wipe secret output buffers after submission/error.
Owned original/draft buffers are wiped on take, Cancel, expiry, a new begin (including a
failed begin) and destruction. The class is noncopyable and allocates no heap memory.

## Lifecycle and ownership

Call `tick(monotonicMs)` even without input. A 30-second inactivity timeout, or a backwards
clock, wipes/discards the draft. This also expires an applied result that was never taken.
`active()` means input is still editing; `outcome()==applied` means a value awaits a take.
After Apply, further wheel/select events do not extend the pending result's lifetime.
`apply`, `press`, `rotate` and `select` check expiry before acting. Cancel always discards;
it cannot undo an operation the form owner already submitted to the domain service.

Authorization, settings/model generations, source identity, screen presentation and
capture protection belong to the form owner. Cancel the editor immediately when any
applicable guard changes. Do not call `select` on every repaint, because real input—not
rendering—owns activity. Physical wake-only input, debounce, held-button suppression and
STOP accessibility remain the Controller's responsibilities. Integer/fixed/choice press
means Apply only when the owner has presented that action; Back maps to Cancel.

The focused native suite is `pio test -e native -f test_localvalueeditor`. It covers exact
original bytes, UTF-8 validity, capacity, secret masking/wiping, cancellation and pending
result timeout, saturating integer limits, finite two-decimal values, booleans and choices.
Removing secret masking or the numeric Apply gate causes the relevant test to fail.
An ESP32-S3 crosscompiler probe compiled the fixed2/preview/take paths. Host ASan/UBSan
execution was unavailable because the existing toolbox lacks their runtime libraries;
no sanitizer result is claimed. Production-screen integration/preview and the final
shared firmware build remain the form owner's integration checks. No device was accessed.
