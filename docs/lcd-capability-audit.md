# Issue #25 capability implementation audit

Original inventory baseline: `c72ac0a` / 4.4.0; acceptance corrections below reflect local main through `e321142`. This audit describes source behavior, not the installed image or a published release. Classification of all 83 rows does not establish full acceptance: R3, R4 and R8 remain open. See the [remaining qualification plan](lcd-completion-qualification.md).
All 83 inventory rows are classified below. The [design inventory](lcd-web-capability-matrix.md)
records the original domain and acceptance requirements.

Combined Phase 4 acceptance passed 1,254 native tests (one optional skip), 118
JavaScript tests, actual encoder/renderer/auth/capture/FIFO journeys, installer
and filesystem boundary harnesses, and canonical/shared and generic SDK builds.
Independent review covered the combined features and serialized transfer-clock
fix. These checks use fake radio/GPIO/flash endpoints: motor behaviour, pairing,
RF reception/transmission, physical rollback and sustained hardware load remain
unverified. No physical action is authorized by a row in this audit.

**Implemented** means present in the source baseline with offline evidence.
**Browser-owned** means the LCD supplies guidance and the existing browser owns
the file/clipboard operation. **Known unsupported** identifies a retained setting
or capability without a supported control. **API-only** preserves an existing
API without inventing an LCD action. These categories do not imply deployment.

Accepted scope includes one shared CC1101/Plus image with bundled web assets,
English rotary entry with unchanged UTF-8 names preserved, the existing web
credential for local access, and the bounded continuing-movement STOP grace.
There is no whole-section settings repair wizard. Configured lamps show
**Light unavailable**. Separate-motor/Euro GPIO relay tilt currently remains unavailable while configuration and ordinary supported lift controls stay intact; an exception to working tilt has not been accepted.

## Acceptance requirements

| ID | Current implementation and acceptance |
|---|---|
| R1 | Implemented: protected room/device/group status, current/target lift/tilt estimates, favourites, weather and membership. Group positions are not invented. |
| R2 | Implemented: explicit repeater observation, candidate review and Add; future retransmission consequence, exact list/auth checks and file-only retry. |
| R3 | Incomplete: hold/release cancellation remains pending. Retained tickets, terminal forwarding and Back/auth/context/recovery/timeout cancellation after admission are implemented and tested offline. Current interface supports explicit 1–127 repeats with a fresh typed action/step/target. STOP is one-shot; MY/56-bit Toggle repeats, individual Euro lift count overrides and unsupported GPIO/seek/tilt repeat parameters are unavailable. Ordinary controls stay available. |
| R4 | Unresolved acceptance: separate-motor/Euro GPIO relay tilt. Both modes remain stored/editable. Existing driver tests characterize all five modes under both output polarities; neither mode has a tilt output mapping. Working tilt or an explicit accepted exception is still required; no wiring is inferred. |
| R5 | Implemented: compatible 80-bit native group Favorite uses one group identity and updates estimates. Obsolete member positioning/programming/deferred MY is cancelled without appending member commands. |
| R6 | Implemented: protected live WiFi/Ethernet details with explicit unavailable states. |
| R7 | Browser-owned: LCD guidance for frame-log copying; no LCD clipboard or hybrid transfer flow. |
| R8 | Incomplete: scoped software reviews/tests and normal boot/state evidence exist, but required combined TLS/full-staging/edit/reconnect/fragmentation/long-idle measurements remain outstanding. Installed updater identity is development and refuses GitHub checks; prepare an eligible canonical release build without bypassing the guard. |

## Evidence in the current source

| Area | Implementation | Offline evidence |
|---|---|---|
| Catalogue and controls | [UI](../src/LocalUi.cpp), [policy](../src/LocalControlPolicy.cpp), [adapter](../src/LocalUiAdapter.cpp) | [Native UI tests](../test/test_localui/test_localui.cpp), [actual input/render/runtime](../tests/ui-render/runtime-test.cpp), [FIFO](../tests/ui-adapter/queue-test.cpp) |
| Administration/programming | [Advanced form](../src/LocalAdvanced.cpp), [runtime](../src/LocalAdvancedRuntime.cpp), [entity forms](../src/LocalEntityForm.cpp) | [Advanced tests](../test/test_localadvanced/test_localadvanced.cpp), [runtime tests](../test/test_localadvancedruntime/test_localadvancedruntime.cpp), actual input/render/runtime |
| Settings/status | [Settings](../src/LocalSettingsForm.cpp), [status](../src/StatusAdapter.cpp), [System](../src/LocalSystem.cpp) | [Settings tests](../test/test_localsettingsform/test_localsettingsform.cpp), [status boundary](../tests/status-health/adapter-test.cpp), [board/GPIO checks](../tools/test-board-profile.sh) |
| Receive/scan | [Diagnostics](../src/LocalDiagnostics.cpp), [scan service](../src/FrequencyScanService.cpp) | [Scan/service checks](../tools/test-frequency-session.sh), actual input/runtime/FIFO |
| Installer/recovery/files | [Updater](../src/GitUpdate.cpp), [controller](../src/GitUpdateController.cpp), [filesystem lifecycle](../src/ApplicationFilesystem.cpp) | [Installer](../tests/git-update-install/run.py), [prepared filesystem](../tools/test-application-filesystem.sh), [manual upload](../tools/test-application-upload.sh), [rollback](../tools/test-update-recovery.sh), [clock](../tools/test-manual-update-clock.sh) |

## Device, protocol and model coverage

| ID | Inventory row | Current status and evidence |
|---|---|---|
| C01 | Roller (0), awning (3), shutter (4), drapery left (2), right (7), center (8) | Implemented: dynamic catalogue, typed capability/adapter and entity administration; native policy/catalogue/entity and actual UI/queue harnesses. |
| C02 | Blind (1) | Implemented for supported mappings; known unsupported separate-motor/Euro relay tilt remains configured and unavailable (R4). RF/GP_Remote and supported relay paths retain their guards/tests. |
| C03 | Garage 1-button (5) | Implemented: category actions; accepted lamp-unavailable notice in Phase2. No lamp state inferred. |
| C04 | Garage 3-button (6) | Implemented: category actions; accepted lamp-unavailable notice in Phase2. No lamp state inferred. |
| C05 | Dry contact 1-button (9), 2-button (10) | Implemented: dynamic catalogue, typed capability/adapter and entity administration; native policy/catalogue/entity and actual UI/queue harnesses. |
| C06 | Gate left/center/right (11/12/13) | Implemented: dynamic catalogue, typed capability/adapter and entity administration; native policy/catalogue/entity and actual UI/queue harnesses. |
| C07 | Gate 1-button left/center/right (14/15/16) | Implemented: dynamic catalogue, typed capability/adapter and entity administration; native policy/catalogue/entity and actual UI/queue harnesses. |
| C08 | Protocol RTS (0), RTW (1), RTV (2) | Implemented: dynamic catalogue, typed capability/adapter and entity administration; native policy/catalogue/entity and actual UI/queue harnesses. |
| C09 | GP Relay (8), GP Remote (9) | Implemented for supported mappings; known unsupported separate-motor/Euro relay tilt remains configured and unavailable (R4). RF/GP_Remote and supported relay paths retain their guards/tests. |
| C10 | Tilt none (0), separate motor (1), integrated (2), tilt-only (3), Euro (4) | Implemented for supported mappings; known unsupported separate-motor/Euro relay tilt remains configured and unavailable (R4). RF/GP_Remote and supported relay paths retain their guards/tests. |
| C11 | Rooms/groups/repeaters and names | Implemented: dynamic catalogue, typed capability/adapter and entity administration; native policy/catalogue/entity and actual UI/queue harnesses. |

## Everyday viewing and control

| ID | Inventory row | Current status and evidence |
|---|---|---|
| E01 | Home/config switch, room selector (`index.js:1030`, `2143`, `3949`) | Implemented: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E02 | Names, room, position/target, movement, favourite, tilt, windy/sunny, group member count (`2217`, `2530`, `2784`) | Implemented: protected entity/group details include room, estimated current/target lift/tilt, favourite/weather and member status; actual UI/adapter/native evidence. |
| E03 | Up, Down (`2259`, `3572`) | Implemented: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E04 | MY (`2260`, `3572`) | Implemented: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E05 | Explicit Stop (`index.html:793`) | Implemented: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E06 | Position slider (`index.js:3935`, `3959`) | Implemented: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E07 | Tilt long-press / slider (`2337`, `3648`, `3942`) | Implemented: supported tilt paths; R4 retains separate/Euro relay mapping gap and visible refusal. |
| E08 | Toggle (`index.html:800`, `index.js:2262`) | Implemented: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E09 | Sun automation flag on/off (`index.js:2318`) | Implemented: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E10 | Visible light button (`2257`, `2316`) | Implemented: accepted alternative: read-only Light unavailable, configuration preserved; actual adapter/render/no-request checks. |
| E11 | Set/Clear MY favourite (`2648`, `2727`) | Implemented: explicit movement/observed-stop/Set-or-Clear journey; exact observation history, typed programming-here requests, actual UI/FIFO and failure tests. See [Favourite](lcd-favourite.md). |
| E12 | Group Up/Down/MY, sun flag (`2530`, `3644`) | Implemented: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E13 | Virtual remote target picker (`index.html:780`, `index.js:3593`) | Implemented: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E14 | Virtual remote button hold/repeat (`index.js:3580`, `3590`, `3593`) | Incomplete: hold/release input remains pending (R3); Back/auth/context/timeout cancellation after admission is implemented. Existing interface provides explicit bounded repeats for supported fresh RF actions; step size/count review, Cancel-first confirmation, protected presentation and FIFO guards. MY/STOP/56-bit Toggle repeats and individual Euro lift count overrides unavailable; ordinary controls retained. |
| E15 | 80-bit StepUp/StepDown + 1..127 step size (`index.html:804`) | Implemented: LCD/protocol/domain stepping and browser group step-size/repeat forwarding; initial/held/release regressions. |
| E16 | 80-bit Favorite (`index.html:820`) | Implemented: compatible80-bit device/group Favorite; group frame uses one identity and updates member estimates without additional member commands. Obsolete programming/position sequences cancelled. |

## Entity, room and remote administration

| ID | Inventory row | Current status and evidence |
|---|---|---|
| A01 | New room draft (`2969`) | Implemented: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A02 | New shade/group draft (`2999`, `3080`) | Implemented: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A03 | Add/save room (`3172`) | Implemented: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A04 | Add/save shade metadata (`3204`, `index.html:465`) | Implemented: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A05 | Travel/tilt calibration settings (`index.html:545`) | Implemented: timing/tilt configuration and safe validation; R4 preserves unsupported relay mappings rather than inventing wiring. |
| A06 | Step/reversal/features/repeats (`index.html:572`) | Implemented: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A07 | GPIO configuration (`index.html:480`, `584`) | Implemented: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A08 | Add/save group (`3291`, `index.html:692`) | Implemented: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A09 | Delete room (`3325`) | Implemented: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A10 | Delete shade (`3346`) | Implemented: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A11 | Delete group (`3368`) | Implemented: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A12 | Drag reorder rooms/shades/groups (`2190`, `2383`, `2530`) | Implemented: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A13 | Pair wizard (`3451`) | Implemented: exact device, bounded Prog review, observed learn mode/intended result and separate marker save; actual UI/adapter/FIFO tests. See [pair/unpair guide](lcd-programming-guide.md). |
| A14 | Unpair wizard (`3522`) | Implemented: the same explicit guide with Unpair intent; controller acceptance does not establish motor membership. Marker/file retry cannot repeat Prog. |
| A15 | Set paired metadata (`3425`) | Implemented: metadata-only paired marker and separately confirmed post-programming marker. File retry never resends RF. |
| A16 | Rolling-code editor (`3391`, `3400`) | Implemented: LocalPrimaryCounter + checked CounterRuntime; NVS/RAM aliases/current-file, final witness, quarantine and file-only retry tests. |
| A17 | Link physical remote (`3653`, `2833`) | Implemented: explicit receive-candidate shade linking/removal; protected bounded cache, checked counter path and no first-frame mutation. |
| A18 | Unlink remote (`3903`) | Implemented: explicit receive-candidate shade linking/removal; protected bounded cache, checked counter path and no first-frame mutation. |
| A19 | Add group member (`3680`) | Implemented: metadata-only Member links/repair plus a separate Motor programming guide. Learn mode is observed after optional LCD device Prog or use of a physical remote; reviewed group Prog and verified intended result precede a separately confirmed list save; list retry never repeats RF. |
| A20 | Remove group member (`3805`) | Implemented: metadata-only removal/repair or separate observed motor unpair programming and list removal. Missing references remain repairable without pretending to unpair a physical motor. |
| A21 | Learn repeater (`3664`, `2833`) | Implemented: protected repeater learn/review/explicit Add, exact current list/candidate/auth guards, metadata/file-only retry; no immediate RF command or counter reset. |
| A22 | Remove repeater (`3893`) | Implemented: exact repeater-list removal, consequence confirmation and file-only retry. |
| A23 | Remote button chords (`index.html:828`) | Implemented: named chords/Prog, 1–127 repeats, default Cancel, existing RF-aware FIFO, configuration lease and fresh witness; acceptance-only result. See [Advanced remote](lcd-advanced-remote.md). |
| A24 | Sensor simulation (`3638`, `index.html:862`) | Implemented: device/group Sunny/Windy Keep/Off/On simulation, bounded repeats, default Cancel and guarded typed requests; may trigger motor automation. See [sensor simulation](lcd-sensor-simulation.md). |

## Settings, networking and integrations

| ID | Inventory row | Current status and evidence |
|---|---|---|
| S01 | General options (`index.js:1380`, `1421`) | Implemented: five-section LocalSettingsForm/Runtime + ProtoSettings; checked typed loading/persistence/read/apply, field/secret/IP/revision/failure tests. |
| S02 | Security read/change (`1394`, `1496`) | Implemented: five-section LocalSettingsForm/Runtime + ProtoSettings; checked typed loading/persistence/read/apply, field/secret/IP/revision/failure tests. |
| S03 | WiFi scan/choose (`1720`, `1733`, `1775`) | Implemented: LocalWifiScan + owned WifiScanFeed: explicit scan, bounded results, review to draft only; actual runtime tests. |
| S04 | WiFi settings (`1840`, `1888`) | Implemented: WiFi settings/secret retention and Phase2 protected live SSID/channel/signal detail; typed settings and actual status/UI checks. |
| S05 | Ethernet selection/fallback (`1706`, `1840`) | Implemented: Ethernet fields/ownership validators and Phase2 live adapter details; unsupported/unavailable states explicit. |
| S06 | Ethernet board/PHY/pins (`1611`, `1639`) | Implemented: conditional Ethernet board/PHY/pin configuration and Phase2 live details; fake-pin/status/SDK checks. |
| S07 | DHCP/static IP (`1797`) | Implemented: five-section LocalSettingsForm/Runtime + ProtoSettings; checked typed loading/persistence/read/apply, field/secret/IP/revision/failure tests. |
| S08 | MQTT integration (`4018`, `4034`) | Implemented: five-section LocalSettingsForm/Runtime + ProtoSettings; checked typed loading/persistence/read/apply, field/secret/IP/revision/failure tests. |
| S09 | Radio settings (`1982`, `2002`, `index.html:885`) | Implemented: five-section LocalSettingsForm/Runtime + ProtoSettings; checked typed loading/persistence/read/apply, field/secret/IP/revision/failure tests. |
| S10 | Frequency/bandwidth/deviation/power (`3917`, `index.html:939`) | Implemented: five-section LocalSettingsForm/Runtime + ProtoSettings; checked typed loading/persistence/read/apply, field/secret/IP/revision/failure tests. |
| S11 | Frequency scan start/progress/stop/use best (`2058`, `2080`, `2115`, `2123`) | Implemented: source-owned browser/LCD scan, explicit Stop and verified restoration, review-to-Radio-draft with separate Save. Retained scan/install RF refusals prevent later replay while independent GPIO remains usable. |
| S12 | Reboot (`1468`) | Implemented: LocalSystem/SafeRestart: defaultCancel, shared verified-save/idle/restart admission; actual result/race/refusal tests. |

## Diagnostics, files and firmware

| ID | Inventory row | Current status and evidence |
|---|---|---|
| D01 | Firmware/app/hardware version, free/largest/min heap, uptime (`index.html:211`, `index.js:4187`) | Implemented: real StatusAdapter internal free/min/largest, PSRAM, published loop rate/max and version/uptime; formatting/status/UI tests. Physical load qualification pending. |
| D02 | WiFi and Ethernet status (`1908`, `1921`) | Implemented: bounded protected WiFi/Ethernet status with current fields and explicit unavailable states; actual adapters/UI/SDK evidence. |
| D03 | RF frame log (`2833`) | Implemented: protected owned ReceiveHub diagnostics: bounded copied frames/pulses, Pause/Clear view, expiry/revocation/wipe; no RF/counter reset. |
| D04 | Copy frame log (`2916`) | Browser-owned: System → Browser files & Web → Copy receive log supplies instructions; clipboard stays in the browser. |
| D05 | Clear frame log (`index.html:987`) | Implemented: protected owned ReceiveHub diagnostics: bounded copied frames/pulses, Pause/Clear view, expiry/revocation/wipe; no RF/counter reset. |
| D06 | Refresh Cache (`index.js:1266`, `index.html:250`) | Browser-owned: System guidance points to Refresh Cache. No controller reset is performed by this LCD entry. |
| D07 | Backup/download (`4081`) | Browser-owned: download Backup in the authenticated browser; LCD guidance starts no transfer. Keep configuration/counter backups private. |
| D08 | Restore selected sections (`4149`) | Browser-owned: selected-section restore through existing checked restore/counter paths. LCD guidance does not start, approve or certify a restore. |
| D09 | Manual firmware upload (`4219`) | Browser-owned: manual firmware reception/status, canonical PSRAM staging and exact loop-owned verified installer are implemented. HTTP receipt/queueing is distinct from installation/restart; generic uploads retain their supported path. |
| D10 | Separate web image | Implemented accepted split: canonical shared firmware bundles web/controller/LCD and refuses separate web images. Generic builds use authenticated prepared filesystem transfers with reader drain, exact nonce/source/length, no-format recovery and current-RAM save; browser-owned files. |
| D11 | Firmware available/installed banner (`index.html:131`, `index.js:4213`) | Implemented: installed/current version and eligible release status through the shared verified service; LCD release review/install and boot-health/idle rollback. This is source behaviour, not proof an eligible release has been published. |
| D12 | Verified GitHub checking/install (#12 integration required by issue) | Implemented: verified HTTPS repository metadata/digests, exact source/hardware/image/slot binding, current-state save, checked restart, continuous boot health and automatic rollback once idle. Worker/controller/HTTP/UI/clock/recovery/package evidence; physical update/rollback remains unverified. |

## API-only and infrastructure coverage

| ID | Inventory row | Current status and evidence |
|---|---|---|
| I01 | Correct estimated current lift/tilt position | API-only: existing estimated-position correction APIs preserved; no current web caller or new LCD exposure. These edits do not establish physical position. |
| I02 | Send a raw remote identity/frame | API-only: existing raw remote/frame APIs preserved; no current web caller or casual LCD exposure. No new raw-frame policy or operation is inferred. |
| I03 | Initial sync/resync, paging, schema/defaults, model event handling | Implemented: bounded LocalCatalogue/RawTargetContext/stable-ID histories; sparse/full/duplicate/rename/recreate/order/concurrency tests. |
| I04 | Frame/frequency subscriptions | Implemented: source-owned receive/frequency feeds, teardown, restoration, bounded allocation, expiry/revocation and failure tests. |
| I05 | HTTP command routes | Implemented: shared ShadeRequests/ProtoOps, LCD typed request preparation. Browser/HTTP routes preserved; no loopback. |
| I06 | Login/context/security session | Implemented: LocalAuthorization/credential editor/adapter: same web credential,120s expiry/shared local throttle and exact30s STOP grace; actual auth/FIFO/capture tests. |
| I07 | uPnP/static assets | Implemented: existing public-asset/auth policy and bundled assets; generic FS reader leases retain open-file lifetime during sealing/draining. No new public configuration or transfer data. |
| I08 | Existing LCD brightness, dim/off, Back, details, screenshots | Implemented: preferences/wake/debounce/Back/graphics loss/protected captures; actual GPIO/SPI/renderer/capture harnesses. Physical qualification remains unverified. |
