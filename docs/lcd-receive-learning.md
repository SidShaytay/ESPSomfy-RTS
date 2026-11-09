# Received-frame observation and learning

The receive observer is a shared, bounded input for LCD learning and diagnostics.
This checkpoint does not expose a Learn screen, link a remote, send a command or
change receiver configuration. The checked received-link service must be reviewed
and integrated before the LCD can offer linking.

One hook publishes metadata for actual received frames, independently of browser
feed subscriptions. Calls without a raw RX record do not become learn candidates.
Invalid packets and frequency-scan packets remain available to diagnostics;
learning applies its own eligibility and freshness rules.

`LocalReceive.h` defines a 32-byte immutable observation: monotonic receive time,
boot-local event sequence, address, rolling code, RSSI, protocol, bit length,
command, raw command, encoding key, hardware sync, step size and packed validity/
scan flags. RSSI is bounded to its signed field. The encoding key is an RF field,
not evidence that a packet authenticates its sender.

There are exactly two named observer slots: learning and diagnostics. Registration
cannot replace another sink. Detachment requires the exact channel, registration
generation and sink, so an old session cannot detach its replacement. Registration
during delivery starts with the next event. Nested delivery is refused.

The observation reference and optional pulse view expire when the callback
returns. Pulse counts are clamped to the decoder's `MAX_TIMINGS` bound; a missing
pointer with a nonzero count yields an empty clipped view. Diagnostics may copy
within its own admitted budget; learning must not retain raw pulse history. All
registration, delivery and destruction run on the model-owning loop. The hub
allocates no heap storage and holds no controller/model pointers.

The native tests cover registration lifetime, replacement and callback behavior,
metadata copying, scan/invalid flags and pulse bounds. The isolated host harness
compiles the complete actual `Transceiver::emitFrame` body and actual JSON writer:
observers receive when the web feed is disabled, and existing web serialization
continues when enabled. It provides no RF, GPIO, NVS or motor endpoint.

The next cache/UI increment will require explicit candidate selection and review.
Receiving a frame alone must never link it. Received counters must pass both cache
freshness checks and the shared checked NVS/live-alias no-rollback service. Normal
configured receive/repeater behavior continues while an LCD observer is active;
observing does not make the whole controller radio-silent.
