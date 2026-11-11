# Issue 36 — complete

Objective: make System → Configure → Display follow the local UI's select-then-edit interaction.

Acceptance: wheel turns select rows without changing values; pressing a value row enters editing and pressing again returns to selection; only explicit Save persists the draft; Cancel/Back restore original preferences; authorization, inactivity, and completed-frame guards remain intact. The screen must distinguish selection from editing.

Scope: display controller, renderer, presentation identity, regression tests and user instructions. No device access, flashing, RF or motor tests.

Completed milestones: reproduced the original failure with a native test; implemented select/edit/Save/Cancel; verified the rendered screen, real runtime input flow and full-feature firmware package.

Delivery: reviewed fix integrated into main. Display selection, editing and explicit Save/Cancel are complete; no further offline requirements remain.

Validation: 1,320 native tests passed (one optional private-backup test skipped), including 38 focused controller tests; production renderer/runtime/capture/adapter/queue/update harnesses, 122 JavaScript tests, release-script tests and canonical LILYGO firmware build/package validation passed. An isolated mutation reversing the selection/edit condition caused six controller tests to fail. Source review covered correctness, readability, architecture, authorization/presentation guards and bounded rendering; no blocking findings.

Hardware validation: not performed; no device access or flashing was requested.
