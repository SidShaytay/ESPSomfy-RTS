# GPIO command admission

GPIO commands now check both the complete physical pin assignment and the
logical output required by the existing driver before entering the shade
behaviour or reading a rolling code. Unsafe assignments, missing required
outputs and commands without a GPIO handler return the existing refused result.
The final driver guard remains in place.

The LCD uses the same read-only admission when displaying available controls.
Partial assignments retain the direct controls their existing outputs support.
A position seek requires both directions and the stop path because its later
automatic stop must also be possible. Relay separate-motor and Euro tilt remain
unavailable: no additional motor wiring is inferred. One-button remote MY keeps
its existing Toggle normalization and output polarity. The two-contact relay's
existing reversed contact mapping is preserved. This does not validate physical
wiring, calibration or movement.

`bash tools/test-gpio-commands.sh` builds the production remote transport,
public shade wrappers, behaviour, MotorLink and GPIO driver against fake
storage, scheduler and GPIO. It checks refusal before storage/frame/estimate
changes, partial assignments, flip semantics, and every category/opcode against
the actual remote driver. `tools/test-local-ui-adapter.sh` checks visible masking
and fresh dispatch refusal; `tools/test-board-profile.sh` checks reserved pads
and final driver safety. None of these tests sends RF or touches hardware.

Ordinary RF groups still retain their existing behavior of notifying configured
GPIO members through `processInternalCommand`. The LCD already rejects mixed
protocol groups. Shared-domain group-member/wizard preflight is a separate
remaining review; this change does not claim that mixed-group issue is solved.
