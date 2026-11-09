# Light capability investigation

Status: existing behavior reproduced offline; physical Light control remains an
open requirement in #28. No hardware was accessed and no RF command was sent.

## What the current code does

- `data-src/index.js`, `Somfy.setShadesList()`: garage categories can expose a
  Light button. Its initial `data-on` uses flag `0x08`.
- `src/ShadeSettings.h`, `joinShadeFlags()` / `splitShadeFlags()`: `0x08` means
  the configured **has-light capability**, not the current lamp state.
- The actual mouse-down handler returns for Light. Mouse-up flips `data-on`
  locally without calling the command service. Another click flips it back.
  This is a cosmetic change, not a physical on/off result.
- `src/ProtoOps.cpp` has no Light command in its strict name table. A protocol2
  `shadeCommand` with `command: light` is refused rather than becoming MY.
- `src/SomfyFrame.h` defines no Light command opcode. `Lighted` in
  `src/ShadeTypes.h` is a separate, unused state flag; it is not an RF opcode.
  No implemented Lighted update path was found in the shared behavior.
- The older permissive `translateSomfyCommand(String)` defaults unknown names
  to MY. It must NOT be used to manufacture a Light implementation: MY can
  move a shutter to its favourite. The strict protocol2 refusal does not
  establish that every legacy entry point rejects unknown names.

## Upstream evidence and limits

The upstream [configuration guide](https://github.com/rstrouse/ESPSomfy-RTS/wiki/Configuring-the-Software)
lists Light for garage categories. However, the upstream
[web handler](https://github.com/rstrouse/ESPSomfy-RTS/blob/main/data/index.js)
also performs the local appearance toggle. The capability label is therefore
not evidence of an implemented or universally supported RF command.

Somfy sells dedicated lighting receivers as well as garage/gate equipment.
Their existence does not establish that a garage’s auxiliary lamp accepts the
same address/command combination. Do not substitute Up, Down, Toggle, Prog or
an unknown raw opcode for Light; these can operate the door or programming.

## Completed verification

`test/js/light-capability.test.js` executes the actual registered mouse handlers
with a fake command sink: clicks change the icon state and dispatch no command.
The protocol command-validation suite explicitly rejects `light`. These are
characterization/refusal checks, not acceptance of a cosmetic-only feature.
Production behavior has not been changed by this investigation.

## Required next evidence

Establish the intended equipment/remote family and a documented supported command
or separately authorized receive-only evidence for that exact protocol. Then
implement the typed shared operation, strict input validation, actual-state
semantics (or explicitly labelled unknown state), and both browser/LCD adapters
with fake RF tests. Physical testing needs its own authorized stage. Until then
Light stays an open capability gap; no omission or fake on/off state is approved.
