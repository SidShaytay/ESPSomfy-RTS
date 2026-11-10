# Set or clear a Favourite from the LCD

Use a device's normal controls to position its shutter, then separately confirm
Favourite programming. Unlock with the configured web credential if required.

1. Open the device controls. Use Up, Down, STOP, Position and any supported tilt
   controls to reach the desired position. To clear an existing Favourite, first
   use **MY position** to reach it.
2. Observe the shutter itself and wait for it to stop. The displayed position and
   movement are estimates, not confirmation from the motor.
3. Open **More → Set favourite here** or **Clear favourite**. The first selection,
   **Move with normal controls**, returns to those controls without sending a
   command. Choose it if more positioning is needed, then reopen the Favourite action.
4. Select **I observed it stopped** only after looking at the actual shutter.
   The controller also requires its movement and pending-work estimates to be idle.
   Clear requires a known Favourite and its exact estimated position.
5. Review the device and position again. **Cancel** is selected by default. Turn
   to **Send Set favourite** or **Send Clear favourite**, then press to program.
6. The LCD returns to the normal control screen with STOP selected. A queued
   request can be cancelled with Back before execution. **Command accepted** means
   controller acceptance, not motor acknowledgement or a verified settings save.
   Observe the physical result before attempting anything further. Reopen the
   device if its changed Favourite invalidates the previous control selection.

Any intervening movement, position correction, Favourite change, authorization
change or target/configuration change discards the observation and confirmation.
This includes changes smaller than the displayed rounded percentage and a move
away followed by a return to the same position. Reopen and observe again.
Programming is refused while other shutters are moving or movement/transmit work
is unresolved; it never waits to program later after an observation becomes stale.

Set and Clear are separate actions. Setting an already matching Favourite does
nothing; it cannot clear it. Clear refuses elsewhere instead of moving back and
programming automatically. Toggle and dry-contact equipment has no programming
entry. Existing GPIO capability checks still determine which wired controls
are available.

Expiry cancels programming and returns to normal controls. The existing scoped
STOP grace remains limited to the original continuing movement and ends at its
observed stop or 30 seconds after session expiry. It never permits starting a
Favourite movement after expiry.
