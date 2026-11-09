# Radio pause for an installation

The updater uses the existing radio and scheduler locks plus one receive-state mux. It does not flush a command, send STOP, change a counter or initialize new hardware to make admission succeed. This checkpoint supplies the actual pause/resume boundary; no installer or startup caller is enabled by it.

`updateActivity(out)` tries the radio and scheduler locks without waiting. A busy observation leaves `out` unchanged. Its ready state respects intentional radio disablement. Scheduler work includes queued/delayed/taken jobs and unobserved outcomes; admission also checks counter work, the raw repeater FIFO, frequency scan, completed RX FIFO and partial-frame decoder state.

`pauseForInstall(token)` requires an empty token and an idle observation under the radio → TX → RX lock order. It seals the empty scheduler without calling the flushing suspend API. It sets receive-paused under the ISR mux before detaching the interrupt, so an ISR already waiting cannot enqueue a frame after the snapshot. The receive FIFO and decoder are not cleared. Partial frames must finish or be rejected by ordinary decoding; the updater does not discard them to force progress.

The ISR, receive FIFO pop, initial FIFO setup and shared decoder reads now use the same receive mux. Decoding/model events and GPIO/SPI driver calls happen after that mux is released. Receive-enable helpers refuse while paused. Existing scan method bodies are unchanged; the separately reviewed scan feature can use this activity/mux rather than introduce another receive guard.

`installPauseHeld(token)` checks exact generation and the retained seal. `resumeAfterInstall(token)` returns resumed, unavailable, busy or stale, preserving the token on busy and refusing old tokens. It restores only the previously enabled receive state, then releases the TX seal; it never resumes a queued transmission, changes GPIO command mapping or retries radio initialization. Failed RX restoration is reported unavailable. The capability remains transient and cannot wrap its generation.

`CXX=g++ bash tools/test-radio-update.sh` compiles the production admission/ISR/receive methods with the actual scheduler and pulse decoder, substituting only physical hardware/locks. It covers pending RX/partial/repeater/scan/counter and waiting/taken/outcome TX, exact tokens, busy/unavailable resume, disabled-radio preservation and an ISR arriving during detach. Removing the paused-ISR guard fails that race regression. These tests establish no physical RF timing, reception, motor or rollback qualification.
