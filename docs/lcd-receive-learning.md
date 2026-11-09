# Receive observation and remote learning

Open **Administration → Devices → Advanced**, choose the configured device by
its stable ID, then select **Learn received remote**. **Begin observation** is
explicit, with Cancel selected first. Opening the page, rotating, receiving a
frame and waking the display never link a remote or send a command.

Observation uses the existing receiver. It does not enable or reconfigure RX,
start a frequency scan, suppress normal control processing or stop configured
repeaters. Use it only when ordinary incoming commands are safe. The observation
itself is passive; it does not make the entire controller radio-silent.

The list keeps at most eight address/protocol/length records for a **30-second
total window**. There is no eviction that can move a selected row to another
sender. Overflow is indicated. Choose a record to review the exact target ID,
received address, code and observed protocol, length, command and signal. Missing
signal strength is labelled Unavailable. **Link received remote** also defaults
to Cancel and needs its complete protected review to reach the LCD first.

Selection freezes the candidate; it never borrows a newer code automatically.
The selected address remains watched while reviewing or queued. A higher code
invalidates the old selection and requires explicit selection and review again.
A changed format, lower code or possible wrap requires a new observation session;
no rollover is inferred. Back or Cancel from the review returns to the frozen
list, not a refreshed discovery list. Cancel learning or Back to the device menu
ends the watch. Start a new list only through a new explicit **Begin observation**.
Duplicates can refresh heard metadata without changing the reviewed payload or
extending the observation deadline. Traffic and render polling do not renew auth.

The observed protocol and bit length describe the received frame. They are **not
stored as a binding or receive filter**, and are not required to match the target's
transmit format. The existing receive path matches linked addresses. Valid Sensor
frames, including code zero, remain eligible. Valid means accepted by the decoder;
it does not authenticate a sender or prove that a motor moved.

Linking queues one ordinary FIFO marker that expires after five seconds. The
receiver watch, credential session, configuration history and exact target are
rechecked immediately before the shared checked link operation, including after
request allocation. Back can cancel an operation that has not begun; an accepted
older operation stays in front. A changed candidate, expiry or failed admission
does not run later or retry automatically.

The shared link operation reads current retained NVS and every live alias of the
address under its counter reservation. It never lowers a received code below
that high-water value. Storage read errors refuse before writing. If a stored or
live code is newer, Cancel learning, then Begin observation again. The LCD never
substitutes the newer stored value for the frame that was reviewed.

Linking stores the address/counter association and current model file. **This
operation sends no RF or motor programming command.** Future ordinary received
commands for the linked address may affect the selected device. Linking is not
physical motor pairing.

A verified result reports the link and model file. If linking applied but the
model file failed or its verification is uncertain, the result retains that
fact and offers **Retry model file save**, with Close selected first. This is a
fresh serialization of the current model through the shared recovery service;
it never repeats Link, rewrites the counter or sends RF. If a counter write may
have persisted without verification, the LCD reports uncertainty and offers
Close only. The identity and restart remain quarantined; neither a stale replay,
an automatic reset nor an unrelated save clears unknown counter storage.

Cancel, leaving the flow, loss of graphics, sleep, auth/history changes and expiry
retire the exact queue ticket and observer before private storage is destroyed.
Copied records, selected values, the inner view and retained current/presented
projections are erased when their private data is discarded. Capture stays
protected and another activation needs a newly completed frame. A result after
an applied link has no executable linking payload left.

The shared receive hub has two fixed, independently registered slots: learning
and diagnostics. Its immutable observation is at most 32 bytes. Raw pulses are
borrowed only during the callback; learning retains none. Registration cannot
replace another sink or overwrite its active receipt, and exact generations
protect cancellation and replacement. The single hook publishes actual RX
metadata independently of browser feed subscriptions. Calls without an RX record
are excluded; invalid and scan frames remain available to diagnostics.

The outer Learn handle and FIFO registry are each at most 32 bytes. The lazy
Advanced session plus wiping allocator header is bounded to 2,304 bytes (native
measurement: 1,664); the existing 4,096-byte draft pool is unchanged. Administration
remains 1,408 bytes in the native build and is bounded to 1,536. The 80-byte queue
marker holds a ticket, not a model pointer or full JSON copy. Learn cache/selection and the primary-counter draft share a typed union. The
existing protected presentation frames are borrowed. These allocation bounds exclude SDK heap
metadata and are checked again by target compilation.

Offline tests use synthetic receive events, fake storage/domain endpoints and
the production parser, cache, registry, backend, queue, encoder Runtime and GFX
renderer. They exercise default Cancel, unseen-frame refusal, code/format/context
changes, FIFO/held-head expiry, callback lifetime and exact cancellation, Sensor
zero, full identities and complete warnings, counter uncertainty, applied file
failure, file-only recovery and private projection erasure. Separate checked
counter-domain tests exercise production NVS orchestration with fault injection.
No controller, live RF, motor or HA action is performed by these tests.
