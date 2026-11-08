# GPIO configuration admission

GPIO admission checks every pad that the controller can physically configure or
write, including pulse release. This is separate from which logical motor
commands use a category's asserted output.

| Protocol | Physical output assignments |
| --- | --- |
| RTS, RTW, RTV | None |
| GPIO relay | Up and Down |
| GPIO remote | Up, Down and My |

A projected edit is checked before changing the running shade. Assigned pads
must support output on the target chip, be distinct, and be free of board,
radio, enabled Ethernet and other shade ownership. These checks also run before
GPIO setup, pulse triggering and pulse release, so a legacy unsafe assignment
cannot bypass them outside the editor. A different object with a duplicate
legacy shade ID still counts as another physical owner.

Boot/restore decode does not configure pads per record. Setup runs once the
complete model and final radio settings are available, using the same guard.

The canonical unset value is 255. Unset pads do not reserve a GPIO and never
reach `pinMode` or `digitalWrite`; negative values and values above 255 are not
silently cast into assignments. Valid assigned outputs keep their existing
setup/write order, command mapping, timing and polarity.

Unsafe legacy assignments remain stored for inspection and explicit repair.
The guard does not select replacement pins or rewrite the configuration. A
projected repair can be validated without applying it; only the existing
loop-owned edit flow changes settings. Model-file save results still qualify
the model file only, as described in [model persistence](model-persistence.md).

Verification uses the native `test_modeleditor` and `test_shadegpioadmission`
suites and `tools/test-board-profile.sh`, which instruments the production GPIO
methods with fake SDK calls. It covers both GPIO protocols and all 17 shade
categories, physical pad ownership, invalid outputs, duplicate assignments,
all-unset and mixed assigned/unset cases, and unchanged valid sequencing.
These are offline software checks, not a hardware or motor qualification.
