# LCD everyday remote: repeat lifetime

The reviewed implementation at `728bc3d` provides hold/release input, exact-ticket cancellation, typed receipts and retained LCD ownership. Controller, completed-frame, actual encoder/GFX and FIFO checks are offline evidence. They do not establish installed behavior, physical RF timing, a published release or complete issue #25 acceptance; see the [remaining qualification plan](../lcd-completion-qualification.md).

## Review, hold and release

Select an everyday RF action, its step size when applicable, and a maximum of 1–127 repeats. The final review starts on Cancel. Select **Hold to send**, wait for the complete target/action/count/step review, then hold the encoder for 800 ms after debounce. A tap sends nothing and displays a hold hint. Intermediate step editing and ordinary one-shot controls retain their existing click behavior.

Eligibility is captured at the initial raw press, before debounce: the completed reviewed frame, exact target token, action/count/step, authorization/configuration generations, model/restore history, readiness and absence of another local pending action must already permit submission. A frame finishing during that press cannot grant authority. Rotation, wake-only input, held-at-boot input, Back, context/auth changes, graphics recovery or a backward clock consumes or cancels the gesture; release and a fresh eligible press are required.

An eligible hold submits one fresh command/counter using the reviewed maximum count, then returns to controls with STOP selected when supported. Raw release requests cancellation before release debounce and never emits MY/STOP. Release bounce can cancel early but cannot rearm the same physical press. The active footer says **Release ends repeats; then STOP**. Completion, refusal and expiry show their outcome while held; the private gesture remains consumed through release, including when the maximum count completes or admission fails. Holding longer never starts another batch.

Cancellation only ends future radio frames. A committed frame may finish, and a shutter already moving may continue: release, then use the separate supported STOP action if needed. Input polling can delay observing release. This is not a physical emergency-stop mechanism, and it does not extend the existing STOP authorization/grace policy.

The original requirement is release cancellation; no finite-click alternative is assumed. This behavior applies only to E14 everyday repeats. Pairing, Set/Clear Favourite, Advanced remote chords and other guided programming batches retain their separately reviewed protocol semantics.

## Exact receipt and lifetime ownership

`RadioScheduler::cancel(ticket, now)` removes only the matching queued job and reports one dropped outcome. A taken/on-air job uses the existing frame-boundary cancellation path: start refuses a cancelled taken job, or the next repeat is suppressed. Outcomes remain started then aborted, or finished when normal completion wins the race. Zero, unknown, repeated or terminal tickets cannot cancel unrelated work; rolling codes are not recycled.

`Transceiver::cancelTx()` uses the existing queue lock without waiting behind the radio lock and wakes the radio task after unlocking. The existing sole outcome consumer processes the model first, then forwards the event to the LCD owner. No global flush, parallel queue or second outcome consumer is added.

`ShadeRequests::runOwnedRepeat()` returns the exact accepted ticket from that invocation. Explicit-count command request types always use the existing fresh shade/group command path, preserving counters, flip, movement and group-member bookkeeping. Repeat request types may reuse a frame only when address, protocol, effective bit length, transformed command and step match. Unsupported owned paths refuse before effects; ordinary bool/enum callers keep their behavior.

One bounded pending-control slot retains the exact ticket, context and security source until the matching terminal event. Back, authorization/context changes, timeout and graphics recovery cancel remaining admitted repeats; a false cancellation return does not erase ownership while a terminal event may still be queued. Unrelated/duplicate outcomes cannot clear the owner.

Admission and queued-not-started jobs have a 5-second deadline. Started jobs use 188 + 141 × repeat milliseconds plus the same 5-second allowance, derived from the current 56/80-bit waveforms. This permits a valid 127-repeat job to continue beyond five seconds. A same-target explicit STOP cancels the old repeat and joins the global FIFO tail within the same slot. The old ticket remains owned until terminal; either terminal order must preserve the STOP marker/result. A dropped-before-start model becomes idle before forwarding, preventing a queued STOP from invoking MY/favourite on an idle motor.

## Verification and remaining gates

The initial scheduler RED checkpoint `eb25236` exposed missing post-admission cancellation. Its temporary no-op/SFINAE test adapter is removed. Production scheduler tests cover queued, taken and on-air cancellation, unrelated work, exact outcomes, stale cancellation, queue order and restart accounting. `tools/test-radio-cancel.sh` compiles the actual wrapper with the real scheduler and substituted lock/clock/task boundaries.

The actual-model receipt harness covers fresh/repeated receipts, current wire identity, real counter writes, flip/step binding, group bookkeeping and refusal without mutation. Adapter/FIFO tests cover retained ownership, invalidation, long-job deadlines, STOP grace/full-queue refusal and terminal races. The consumer-prefix harness proves model-before-LCD forwarding outside the queue lock. Controller and actual encoder/GFX tests cover tap/hold/release, 800 ms qualification, SPI completion during debounce, wake/held-at-boot suppression, exact step/count, terminal feedback, recovery and no rearm/release-triggered STOP. Independent guard-removal mutations are caught by the relevant regressions.

Final integrated build/package checks and physical encoder/RF timing remain separate evidence. The [qualification plan](../lcd-completion-qualification.md) retains combined TLS/full-staging/edit/reconnect/fragmentation/long-idle work, physical verification and publication gates. Separate/Euro relay tilt remains a disclosed original-driver limitation, with configurations and supported controls preserved; a new mapping is outside the port. No documentation step authorizes device access or motor commands.
