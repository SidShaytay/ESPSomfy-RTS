# Test a release board

Use the ordinary release firmware for this routine. Special instrumented firmware remains necessary for raw LCD input injection and internal fault injection. Release builds expose the normal command API, bounded motion diagnostics and authenticated LCD screenshots.

Before a test, record the physical board, firmware source revision and binary hash, version, active slot, current profiles and shutter/group membership. Save settings and counters privately. Arrange a quiet period so unrelated automations do not alter the results. Use the full backup procedure before firmware writes.

## Check the controller without movement

From the repository, run the focused live checks with Python and pytest:

```bash
python3 -m pytest test/e2e/test_calibration.py --device <controller-address> -q --junitxml=<private-report-path>
```

The existing e2e fixtures support the device's authentication options; see [the suite guide](../test/e2e/README.md). These checks exercise trace configuration/expiry/validation and guide Begin, Poll, refusal of synthetic endpoint confirmation and Discard. They verify unchanged calibration and rolling codes. They do not transmit motor commands.

Capture the LCD using `tools/capture-local-ui.py --help` for the supported options. Screenshots prove what was rendered, not whether every physical button works. Check free memory and network responsiveness before and after a test. Keep reports, settings and raw logs outside Git.

## Check actual RF and motion

Only run movement stages during an authorized RF window. Enable Motion and Radio capture first; add Calibration while using the guide. Keep one operator with a clear view of the shutters and access to Stop.

| Stage | Automated evidence | Operator observation |
| --- | --- | --- |
| Full open / full close | accepted command, RF started/finished, direction and elapsed time | both physical endpoints and motor self-stop |
| Guided calibration | actual RF start origin, measured draft, save result | take-up, halfway, settling and endpoint markers |
| 25%, 50%, 75% each way | target, start/Stop tickets and deadlines | actual visible position and stopping accuracy |
| Same-direction retarget | original run clock retained, latest deadline | continuous movement and correct final stop |
| Reversal | complete Stop before new directional transmission | physical stop, reversal play and new movement |
| Group positions | shared start when compatible, independent member Stops | each member's position and stop |
| Reboot | same profiles/settings/counters and healthy application | no unexpected movement; uncertain reference remains uncertain |

For each stage record requested position, expected timing, transmitted timing, reported state, observed position and pass/fail. Report maximum and typical timing differences when repeating tests. Do not turn a software estimate into a physical pass. A missing observation is **not observed**.

The controller has no position feedback from ordinary RTS motors. A fully automated RF test can establish correct command ordering and timing, but physical accuracy requires observation or separate sensors. No production test should pair/unpair motors, program limits or rewrite rolling codes.
