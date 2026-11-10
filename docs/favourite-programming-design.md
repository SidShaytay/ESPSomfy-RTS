# Explicit Favourite programming

Issue #26 implements Sid's chosen sequence: move deliberately, observe the
shutter stopped, then separately confirm Set or Clear Favourite. This document
records the programming primitive; the LCD workflow is not implemented yet.

## Separate movement from programming

The existing `setMyPosition(position, tilt)` operation can move to a requested
position, or send a short MY to the old favourite, and program automatically
after settling. Reusing it for the final LCD confirmation would violate the
explicit stages. Existing browser callers retain that behavior.

`ShadeBehaviour::programMyPositionHere(clear, now)` instead:

- Requires idle administration state: no movement, pending positioning,
  unresolved transmit, delayed MY or unprocessed received frame.
- Sets the current exact lift/tilt coordinates using the existing long-MY
  transmitter. Setting an already equal favourite does nothing; it never clears.
- Clears only when the relevant current coordinates exactly equal the stored
  favourite. Elsewhere it refuses without moving or programming. The previous
  movement/observation stage must deliberately reach that favourite.
- Preserves tilt-only semantics and fractional coordinates. Toggle and dry-contact
  equipment is refused because MY can have a movement meaning there.
- Uses the existing transmit outcome handling: refusal/drop restores the old
  favourite; persistence is requested only after the whole programming frame
  completes. Accepted transmission is not proof the motor learned it.

Separate internal `set_favorite_here` and `clear_favorite_here` requests route
through `ShadeRequests` and the production `SomfyShadeOps` adapter. They cannot
consume a position/target field or invoke the old automatic operation. The
production wrapper retains model, counter and GPIO admission checks. No new
browser/MQTT protocol operation is exposed by this backend increment.

## Remaining LCD work

The authenticated device workflow must offer separate movement controls with
STOP, an explicit human “observed stopped” confirmation, and a fresh default-Cancel
Set/Clear review. Changing target, configuration, authorization or observed
position after that review invalidates it. Navigation and cancellation must not
send a command. Do not infer physical observation from the estimated model.

Submit a bounded intent through the existing local FIFO with an RF-aware marker,
then revalidate its exact source, target and observed position before calling the
programming primitive. Show queued/accepted/refused honestly; never claim motor
acknowledgement or a verified model-file save from queue admission. GPIO and
simulated-favourite mappings still require their existing driver checks; this
primitive does not establish new hardware support or remove existing support.

Offline tests cover set versus clear, no hidden movement, exact tilt coordinates,
toggle/dry-contact refusal, unresolved transmissions, refusal/drop rollback and
save timing. Hardware programming has not been attempted.
