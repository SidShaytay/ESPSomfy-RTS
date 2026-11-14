# Calibration usability

Objective: one firmware version everywhere, calibration in Device > More, and an observation-led wizard on LCD and web.
Acceptance: updater/About/API agree on firmware version; source identity remains diagnostic metadata; More opens calibration for the selected supported device; recognizable cover types select the correct measurement flow; instructions explain preparation, full open/close, first movement, halfway by distance, final settling, reversal and save; unsupported mechanisms are not driven; regression tests, display/browser checks and canonical build pass before main integration.
Scope: UI, shared guide presentation, version display and supporting tests/docs. Preserve radio admission, existing profiles and motor limits. No RF testing: prior dated window has expired.
Completed milestone: selected-device entry and navigation regressions pass (63 cases); updater worker reports the shared firmware version even for a development identity; 131 web protocol tests pass.

Completed: per-device More entry, plain cover choices, LCD preparation, observation-led LCD/web instructions, required direction-change checks, actionable review order and updated user guide/screenshots. Focused navigation/form suites: 65 passed. All existing native suites: 1,459 passed and one existing skip before the final form-only edits; the affected suites passed afterward. JS: 131 passed. Production renderer/runtime/capture/adapter and updater worker checks passed. Instruction bounds and rendering cover every guide stage.

Next milestone: clean canonical build, final review, main integration and deployment with fresh recovery evidence.
