# Issue 25 execution record

Implement complete standalone LCD control/admin and the shared CC1101/Plus firmware. **Incomplete.** Root owns this record; completed evidence is in issue-25.log. Repository: /var/mnt/data/projects/personal/ESPSomfy-RTS. Current local main **d7971d5**, clean; remote last verified fc5a294. Nothing pushed, released or flashed. Preserve all worktrees, especially dirty historical #12. Issue25 original requirements/status rewritten and verified; last public status describes 5ea5cf3, needs next checkpoint.

## Authority and decisions

Flash authorized only after intended implementation/review and actual hardware/build/slot/layout identification, fresh verified full-flash backup and exact recovery plan per docs/flashing.md. No live controller access yet. **No RF/motor/GPIO commands, pairing, calibration, limits or HA changes/tests.** Do not disturb occupants.

Reuse web credential;120s inactivity/local throttle. STOP grace ends at original movement end or30s after original expiry, exact continuing target/context only. One canonical lilygo-t-embed-cc1101. Bundle approved: matching controller/LCD/web update+rollback, configuration separate; canonical web-image upload410. Browser owns backup/restore/manual files; no hybrid. Standalone LCD updater: verified HTTPS fixed GitHub repository/releases+digests, no separate signatures.

Pending user decisions: fonts/languages, explicit favourite stages versus automatic web sequence. Relay separate/Euro tilt and visual-only Light semantics remain unresolved. Do not implement unsettled dependent flows or silently defer parity.

## Milestones and active work

1–5.[x] Issue/instructions/matrix/design; main controls/auth/STOP/graphics/capture; checked settings/model persistence,49 settings fields,Health, CRUD/Reorder/GPIO/packed allocation; advanced metadata; protected System Restart; bundled web; shared monotonic restart; staged image verifier. These are not complete parity or a working installer. Archive has commits/test evidence.

6.[x] WiFi backend main5ea5cf3: exact-generation borrowed result feed, configuration owner, foreign SDK scan double-probe, no foreign deletion. Two independent reviews, guard mutations,8 focused/full1025 native+skip, actual adapter and both SDKs passed. **No LCD scan UI yet.**

7.[x] Counter caller activation main e425653, root ae963f5/actor ed8f471. All-alias/RAM/NVS max checked for received linking; READONLY preflight before writable opens; explicit primary lowering remains separate. TX/RX/raw/group quarantine safeguards, explicit browser COMMIT file-only recovery and permanent Save current model action. Root1040 native+skip/89JS/12actual domain/joint radio+receive+GPIO+adapter hosts/shared44.65s+generic38.26s passed; removed TX sink guard RED/restored. Existing unblocked automatic RX can lower code; no global anti-replay claim.

8–9.[x] ReceiveHub main6042ffa and radio pause preceding5ea5cf3. Fixed two named sinks,32B observations/borrowed pulses/exact receipts; nonblocking exact-owned idle receiver pause/resume without queue flush; one RX task/ISR mux, no SDK/log under critical, null mutex refusal. Root6+49 native, actual joint hosts/mutations/both SDKs passed.

13.[x] Learn cache main d7971d5, root274ff45/actor79c9eba. Eight candidates/30s total/frozen review/address-wide highwater, no heap/pulses/UI. Root first aggregation-loop mutation initially survived; added dropped higher-then-lower cross-format regression, now RED/restored9. Joint44 native/canonical42.64s; both SDK44.75/36.36 before joint. LCD consumer still absent.

11.[-] **Root model-load review** issue-25-model-load-review/codex/25-model-load-review latest2403306 plus sink-test commit pending/current. From latest main d7971d5 reconciled checked counter guards. Actor b234656 adds immutable64KiB PSRAM snapshot, mounted/exact absence/full record preflight, bad-load quarantine, full-model checked restore, MQTT preservation. Known absent allows edits but withholds MQTT until checked save; browser fanout retained on first edit file failure. Root actual snapshot/parser/caps/MQTT/13counter modes/restart/GPIO/radio all green; restart guard mutation RED/restored;36 UI native+actual GFX and independent notice review green. **Both SDK running session27862**, logs /tmp/issue25-model-root-{shared,generic}.log. Check source clean/test result before integrate. Boot counter NVS and selected settings restore remain required acceptance gaps.

12.[!] **SEC review** issue-25-security-load-review/codex/25-security-load-review d280f58, source clean. Actor e25f1df includes typed READONLY load/auth gates +securityUnavailable restart/transfer/OTA/reconcile and qualified fake upload fixtures. Root21 native/actual GFX, tokenAccepted mutation RED/restored actual web/socket/upload/LCD/OTA+SafeRestart green. Actor1023native+skip/bothSDK green. Need actor independent root notice review; then merge MODEL main, reconcile enum/text/model+SEC health conjunct/NVS fake fields and actual fixtures, joint builds before integrate. NonSEC CFG/WIFI/IP/MQTT/NTP/ETH/CC1101 read trust, migration/cleanup and selected restore remain required; actor plans next bounded feature only after SEC main notice.

14.[-] **web_inventory WiFi LCD UI**, NEW latest-main worktree. Lazy wiping16-row bounded Session; explicit Start/defaultCancel,30s/auth/network history, exact teardown; immutable candidate review→existing Network SSID draft only, never save/connect. Shared borrowed frame/unseen-action guard. Child input must propagate real activity to System; tick/render/completion cannot extend inactivity. Learn UI next ONLY after reviewed WiFi integration. Agent knows cache+counter now main.

15.[!] **build_audit** model/MQTT ready b234656, await model main notice. Next NEW latest-main bounded checked boot counter loader:272 unique max entries, RO preflight, checked promotions/readback, no NVS during adoption. Preserve TX/RX algorithm; later typed TX-read refusal seam also needed. Selected restore must stage settings/radio, check every selected save, truthfully report model-applied/settings-partial and refuse restart on failure; coordinate SEC actor. Installer waits prerequisites.

10.[ ] Remaining: LCD Learn/counter/programming/group/repeater wizards, settled Favourite, frequency scan/diagnostics, fonts/unresolved actions, standalone installer/manual shared admission/health/rollback/startup, final matrix/docs/whole-change independent review/offline acceptance then authorized flash procedure. No power-loss atomicity claim across NVS/file, no journal.

## Operations

Human GitHub fallback verified SidShaytay: env -u GH_CONFIG_DIR /home/linuxbrew/.linuxbrew/bin/gh. Git network GIT_CONFIG_COUNT=0 and SSH id_ed25519_2026_vega. Per-command commit.gpgsign=false. Active skills only ~/.agents/skills. Root alone writes record.

PIO ~/.platformio/penv/bin/pio; native PATH ~/.cache/espsomfy-lilygo/native-toolbin; PLATFORMIO_LIBDEPS_DIR=issue-25-design/.pio/libdeps. Host compiler podman rocm-7.2.3, ARDUINOJSON_INCLUDE=.../libdeps/native/ArduinoJson/src; GFX_DIR pinned canonical Adafruit GFX. Serialize SDK/native per tree; never mutate source during compile. All localhost browser/HTTP fixtures closed; no CUA/live browser/controller. NextID16.
