# Issue 25 execution record

Implement https://github.com/SidShaytay/ESPSomfy-RTS/issues/25 completely. **INCOMPLETE; continue.** Root owns this record; detailed prior evidence is archived in issue-25.log. Repository is bare with sibling worktrees; preserve all existing work, especially dirty historical #12.

## Current outcome

Authoritative local main is **9d8a2cd**, clean. This integrates the independently reviewed settings/restore vertical (exact source tree52f54ce). Fresh SSH fetch still verifies remote mainfc5a2947928fdf855185f77ecaae69c00b677156. Nothing pushed, released or flashed. No controller, serial, RF/motor/GPIO or HA access occurred. All hardware/NVS/broker test boundaries are fake.

Sid asked for time/token ROI reflection, then explicitly said continue. Root acknowledged poor ROI from overlapping infrastructure work/repeated suites and too few feature closures. New execution rule: finish a vertical, focused regressions during development, one combined integration acceptance pass, broad final checks once. Do not repeatedly rerun unchanged suites or widen into speculative issues.

All three subagents have errored with service usage limit; do not retry calls without new capacity. Their code/evidence is preserved. Root continues. GitHub human auth **now fails** (`SidShaytay` active but token invalid). Root told Sid, stopped GitHub operations, continues offline; do not switch accounts/re-authenticate/read secrets. Issue update must be saved locally until credentials restored. SSH fetch still works. Last posted/readback issue body describes6dfb29b, /tmp/issue25-primary-progress.md and ...verified.json; it now needs updated status, not a closed issue.

## Authority and product choices

Flash authorized after the complete intended reviewed implementation and exact board/build/slot/layout identification, fresh independently verified full-flash backup and recovery/write plan per docs/flashing.md. Flash permission does not authorize movement/pairing/calibration/limits or HA tests/changes. Do not disturb occupants. No live browser/controller tools.

Reuse web credential for LCD controls/admin;120s inactivity and local attempt throttle. Expired STOP only for the original continuing movement/unchanged target+context, ending on observed stop or30s after original expiry; never later movement. Single canonical lilygo-t-embed-cc1101 for CC1101/Plus. Bundle approved: controller/LCD/web update and rollback together, config separate; canonical web-image upload410. Browser owns backup/restore/manual files, no hybrid approval/transfer connection. Standalone updater trusts verified HTTPS fixed GitHub repository/releases and digests, no separate signature.

Unanswered consequential choices remain: fonts/languages; Favourite explicit stages versus old automatic web sequence; Relay separate/Euro tilt; visual-only Light. Failed non-SEC whole-section replacement question also pending (backups omit WIFI/MQTT secrets). Required RF-scan choice already asked: pause all controller commands for scan versus keep independent GPIO controls usable. Do not assume silence approves a dependent flow. No default/security reset or parity deferral approved.

## Completed main and review evidence

Main includes controls/auth/STOP/graphics/capture, settings forms/checked writes, Health, CRUD/Reorder/GPIO, advanced metadata/System Restart, bundled web, SafeRestart, image validator foundations, WiFi scan, ReceiveHub, explicit counter domains and guards, model/MQTT/SEC trust, passive receive diagnostics, received-remote Learn and primary counter LCD. NOT full parity or a working standalone installer.

New9d8a2cd closes typed read-only CFG/WIFI/IP/MQTT/NTP/conditional ETH/radio loading; retained counter load with all read preflight and checked promotion; truthful selected-section restore preparation/adoption; exact source expiry/auth history/resource cleanup/status; stale browser session retirement and raw RF producer admission barrier. Strict external restore validates raw numeric widths, version/counts, active IDs/enums, unique IDs before effects. Ordinary legacy boot codec remains unchanged. Preserve RF-address aliases, unused sentinels, mixed/dangling membership repair. Group own protocol is RF0..2; shades preserve all5 supported protocols.

Pin safety retains actual radio, ETH and GP output claims for the boot before SDK attempts. Metadata removal does not release hardware. New buses cannot reuse those pads; no new owner/journal/reset/decommission flow or wiring guarantee. Mandatory captured masks and fresh runtime unions qualify restore before NVS/model effects. GPIO does not conflict against its own historical map;255/None and ignored Relay MY do not invent ownership.

Independent reviews were split between source authors and peers; Root reviewed final claims/Stage/consumer/socket deltas and actual boundaries. Root combined final52 acceptance: **1153 native pass,1 existing skip**, actual complete ConfigFile/Stage/counter/MQTT/pin/board host harnesses pass. All99JS and actual upload/resource/socket/completion tests passed; final shared/generic SDKs pass17.152/14.779s. Root killed then byte-restored source-expiry (2 actual failures), direct raw retired-client authority (134), and retained GP ownership (134) mutations. Logs: /tmp/issue25-final-joint-{native,host}.log, /tmp/issue25-final-gp-root-{mutant,restored}.log. Source52f matches main tree exactly; no repeated SDK/native needed merely for merge metadata.

## Root active scan increment

Tree issue-25-frequency-scan-service, branch codex/25-frequency-scan-service, tip48542ce after merging newmain as1e0d3f5. It contains reviewed49168a7/41ec244/4576491: nonblocking quiescent Begin never flushes/preempts accepted TX or RX; seal+RF pause retained until tuning restored; old Stop cannot release a newer admin seal; pause RX through restoration; discard only scan-owned RX; txSuspended exposes administrative pause. Token header171f9e7 has3 native tests and canonical boot/generation parsing. Full scan ownership/service/UI and pending command-policy choice still unfinished.

Previous integration hold is now resolved by9d radio-save/apply admission. New coupled actual fixture seeds the REAL scanner scheduler seal+administrative mask and verifies Save refuses before NVS/driver work while preserving mode/seal/mask. Actual scan/start/Stop races, radio settings18 and startup46 pass /tmp/issue25-scan-coupled-acceptance.log. Root peer previously reviewed driver and killed RX no-loss guard;457 tiny pause-return reviewed too.

Current **running Root session91562**: serialized shared thenesp32dev SDK builds at48542ce, logs /tmp/issue25-scan-final-{shared,generic}.log. No source edits in that tree until finished. After both pass, integrate this bounded reviewed increment into main (not a claim of full LCD scan). No full native repeat: only Transceiver integration and one test changed since combined pass; token3 previously passed unchanged. Then update local issue draft and this record.

## Preserved membership work

Agent web_inventory stopped on usage limit. Tree issue-25-group-membership-ui, branch codex/25-group-membership-ui, committed core56d1fd5 plus uncommitted files. DO NOT DISCARD. Source: LocalMembership.{h,cpp}, LocalMembershipRead.h, LocalAdvanced.h/Runtime.cpp, LocalEntityView.h, ModelIdleForAdministration.h, native membership/runtime tests and platform filter. Pure draft24 tests passed; runtime backend guarded rows/selection/admission added; full parent/UI/FIFO/renderer journey not finished or independently reviewed.

Core validates membership IDs before narrowing/history, typed successful noChange for duplicate Add/nonmember Remove with no file/event/history, immediate JS no-op completion. All32 physical slots,255 placeholders/88 missing/duplicates; metadata only, no physical pairing or RF/protocol member filter. Pure metadata admission proves all configured shades idle + queue/radio/restart/exact lease; ignores unknown cached motion of empty/malformed groups only. Strict radio/update modelIdle remains strict. Draft fits existing808B typed union, no new frame/handle/pool. Agent expanded Backend pure virtual membership methods, so check all test fakes when finishing UI. Before final qualification merge latest completed main into same feature, preserving WIP via focused checkpoint if needed.

## Remaining objective and next work

Finish membership UI; remote/repeater/physical programming and settled Favourite; full radio scan; unresolved font/Light/relay decisions. Finish standalone GitHub installer/manual canonical admission, generic FS unmount/recovery, continuous60s/180s boot health/rollback/startup/System Updates. Existing GitUpdate is metadata/download/image-verifier foundation, not installed/running feature; do not blindly merge unfinished historical #12/updater branches. One shared worker/owner/trust path, PSRAM full image, inactive OTA writes, exact final SafeRestart boot selection; preserve counters/data and accepted work. Complete final capability matrix/status, docs, independent whole-change review and offline acceptance, then authorized flash procedure. New independent review may need restored subagent capacity; don't invent a signoff.

## Tooling

Only ~/.agents/skills active; relevant skills already read. Git per-command commit.gpgsign=false (pinentry unavailable), no global config change. Human GitHub command env -u GH_CONFIG_DIR /home/linuxbrew/.linuxbrew/bin/gh is blocked on invalid token. Git network GIT_CONFIG_COUNT=0 plus SSH key ~/.ssh/id_ed25519_2026_vega; no code push done.

PIO ~/.platformio/penv/bin/pio. Native PATH ~/.cache/espsomfy-lilygo/native-toolbin; shared PLATFORMIO_LIBDEPS_DIR=<repo>/issue-25-design/.pio/libdeps. Host CXX via podman exec -w <tree> rocm-7.2.3 /bin/bash -c; ARDUINOJSON_INCLUDE shared native ArduinoJson/src, UNITY_INCLUDE analogous. Previews use ARDUINOJSON_DIR and pinned GFX_DIR instead. Serialize SDK/native per tree; never edit source being compiled. Root review trees clean: issue-25-restore-joint-review52f, settings-full-review8ae, socket-review5db. Detached group reviewfb855 retained by branch codex/25-restore-joint-group-review. Every prior mutation byte-restored. Preserve historical worktrees.
