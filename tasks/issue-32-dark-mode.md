# Issue 32: dark mode

Objective: add a readable dark theme with a browser-local System/Light/Dark choice.
Acceptance: preference applied before paint, persisted where available, live system preference changes, usable forms/dialogs/status on desktop/mobile, automated tests and offline screenshots.
Scope: web assets and tests only. No device access or flashing.
Baseline: main 1b44ab8; GitHub API confirmed remote main matches (SSH fetch unavailable).
Next milestone: failing preference tests, then implement appearance and verify both themes.

Completed:
- System/Light/Dark selector outside authentication/configuration panels; browser-local preference initializes before stylesheet loading and follows live OS changes in System mode.
- Neutral theme variables cover page/panels/forms/tabs/lists/position controls/status/overlays and dynamically generated dialogs. Existing per-shade colors and icon geometry remain intact.
- Preference tests first failed, then passed; full web suite: 122 passed. Asset generation: 9 Python tests passed. Bundle tests: 10 Python tests plus compiled immutable HTTP/version harnesses passed in existing rocm-7.2.3 container (host lacks c++).
- Offline Chromium checks: explicit selection, reload persistence, live emulated system changes, computed panel colors, desktop/mobile selector positioning, dialog rendering; no page errors after disabling device initialization in the temporary fixture.
- Screenshots: build/dark-mode/{light-desktop,dark-desktop,dark-mobile,dark-dialog}.png. Fixture uses current HTML/CSS with device startup disabled; no live controller operations.
- Review covered preference edge cases, CSS fallbacks, dynamic inline colors, icon colors, mobile layout, storage exception handling, dependency and payload impact. No blocking findings remain.
- Validation is offline web/asset validation; no full firmware build, flashing or hardware testing performed.
