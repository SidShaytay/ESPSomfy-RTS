# Issue #25 capability implementation audit

Audit date: 2026-11-09. Authoritative local main `e38fe16`; freshly fetched remote
main `fc5a294` is an ancestor. **Incomplete.** This records source/evidence coverage,
not independent signoff, combined acceptance or hardware qualification.

The [design inventory](lcd-web-capability-matrix.md) retains domain, applicability,
authorization, input, effects and acceptance cases. This audit assigns every one
of its 83 rows a current implementation status; row IDs follow table order within
that inventory. Main means present in local main, not published GitHub main.
Candidates are isolated, unpushed and await independent review/integration.

Accepted latest scope: configured lamps show read-only Light unavailable;
configuration stays intact. Existing recovery routes remain; no whole-section LCD
repair wizard. Browser owns clipboard/cache/backup/restore/manual files. One shared
CC1101/Plus bundled firmware. These decisions do not grant hardware/RF/GPIO/HA access.

## Remaining acceptance work

| ID | Concrete gap and next action |
|---|---|
| R1 | Complete bounded device/group status view: room, estimated current/target lift/tilt, favourite and relevant sun/wind flags. Main Target currently carries current positions and moving/count only. Preserve stale-target/auth/capture policy and safe STOP navigation. |
| R2 | Add protected repeater learn/review/explicit-add to existing bounded receive/Advanced/domain path. Main exposes removal only. No first-frame linking, raw counter reset or invented radio command. |
| R3 | Complete everyday bounded repeat selection/cancellation through exact target/last-command shared semantics. The advanced-remote candidate covers chord/Prog repeats only. Keep safety-sensitive STOP semantics intact. |
| R4 | Resolve relay separate/Euro tilt mapping without removing configured modes or inventing extra-pin/electrical semantics. Current real-driver fake tests characterize unsupported mapping; guards and visible refusal remain. |
| R5 | Verify shared 80-bit group Favorite and expose supported behavior; main groupActions currently omits it although individual actions include it. No member-identity substitution. |
| R6 | Complete bounded live network details from existing status sources (connected WiFi SSID/channel/strength and applicable Ethernet status), with protected identifying values and honest unavailable state. Main status carries link/IP only. |
| R7 | Add truthful LCD guidance for browser-owned frame-log copying. Main receive diagnostics and browser-files guidance currently omit that entry. |
| R8 | Independent component reviews, deliberate conflict reconciliation into main, complete combined tests/package/docs and separately authorized physical measurements/deployment. All candidate evidence is component-only. |

API-only estimated-position correction/raw-remote operations remain separately
inventoried below; no new casual LCD raw-frame operation is proposed.

## Device, protocol and model coverage

| ID | Inventory row | Current status and evidence |
|---|---|---|
| C01 | Roller (0), awning (3), shutter (4), drapery left (2), right (7), center (8) | Main: dynamic catalogue, typed capability/adapter and entity administration; native policy/catalogue/entity and actual UI/queue harnesses. |
| C02 | Blind (1) | Partial: configured modes retained; RF/GP_Remote and supported relay paths guarded/tested. Relay separate/Euro physical mapping remains unresolved (R4). |
| C03 | Garage 1-button (5) | Main category actions; accepted lamp-unavailable candidate e979ca4. No lamp state inferred. |
| C04 | Garage 3-button (6) | Main category actions; accepted lamp-unavailable candidate e979ca4. No lamp state inferred. |
| C05 | Dry contact 1-button (9), 2-button (10) | Main: dynamic catalogue, typed capability/adapter and entity administration; native policy/catalogue/entity and actual UI/queue harnesses. |
| C06 | Gate left/center/right (11/12/13) | Main: dynamic catalogue, typed capability/adapter and entity administration; native policy/catalogue/entity and actual UI/queue harnesses. |
| C07 | Gate 1-button left/center/right (14/15/16) | Main: dynamic catalogue, typed capability/adapter and entity administration; native policy/catalogue/entity and actual UI/queue harnesses. |
| C08 | Protocol RTS (0), RTW (1), RTV (2) | Main: dynamic catalogue, typed capability/adapter and entity administration; native policy/catalogue/entity and actual UI/queue harnesses. |
| C09 | GP Relay (8), GP Remote (9) | Partial: configured modes retained; RF/GP_Remote and supported relay paths guarded/tested. Relay separate/Euro physical mapping remains unresolved (R4). |
| C10 | Tilt none (0), separate motor (1), integrated (2), tilt-only (3), Euro (4) | Partial: configured modes retained; RF/GP_Remote and supported relay paths guarded/tested. Relay separate/Euro physical mapping remains unresolved (R4). |
| C11 | Rooms/groups/repeaters and names | Main: dynamic catalogue, typed capability/adapter and entity administration; native policy/catalogue/entity and actual UI/queue harnesses. |

## Everyday viewing and control

| ID | Inventory row | Current status and evidence |
|---|---|---|
| E01 | Home/config switch, room selector (`index.js:1030`, `2143`, `3949`) | Main: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E02 | Names, room, position/target, movement, favourite, tilt, windy/sunny, group member count (`2217`, `2530`, `2784`) | Gap R1: current name/category/protocol/position/motion/count shown; complete room/target/favourite/weather detail absent. |
| E03 | Up, Down (`2259`, `3572`) | Main: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E04 | MY (`2260`, `3572`) | Main: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E05 | Explicit Stop (`index.html:793`) | Main: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E06 | Position slider (`index.js:3935`, `3959`) | Main: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E07 | Tilt long-press / slider (`2337`, `3648`, `3942`) | Main supported tilt paths; R4 retains separate/Euro relay mapping gap and visible refusal. |
| E08 | Toggle (`index.html:800`, `index.js:2262`) | Main: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E09 | Sun automation flag on/off (`index.js:2318`) | Main: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E10 | Visible light button (`2257`, `2316`) | Accepted alternative candidate e979ca4: read-only Light unavailable, configuration preserved; actual adapter/render/no-request checks. |
| E11 | Set/Clear MY favourite (`2648`, `2727`) | Candidate 84b9bb7: complete move/observed-stop/Set-or-Clear journey; exact observation witness, real UI/FIFO and failure tests. |
| E12 | Group Up/Down/MY, sun flag (`2530`, `3644`) | Main: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E13 | Virtual remote target picker (`index.html:780`, `index.js:3593`) | Main: LocalUi/LocalUiAdapter/LocalControlPolicy and actual adapter/runtime/FIFO tests; normal capability/auth/context admission. |
| E14 | Virtual remote button hold/repeat (`index.js:3580`, `3590`, `3593`) | Partial R3: ebf4cf0 provides bounded chord/Prog repeats; everyday explicit repeat-count/hold coverage still needs completion. |
| E15 | 80-bit StepUp/StepDown + 1..127 step size (`index.html:804`) | Main LCD/protocol/domain group stepping; web helper forwarding candidate64461f5 with initial/held/release regressions. |
| E16 | 80-bit Favorite (`index.html:820`) | Partial R5: device Favorite exposed; group action mask currently excludes Favorite. Verify/complete shared-domain group behavior. |

## Entity, room and remote administration

| ID | Inventory row | Current status and evidence |
|---|---|---|
| A01 | New room draft (`2969`) | Main: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A02 | New shade/group draft (`2999`, `3080`) | Main: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A03 | Add/save room (`3172`) | Main: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A04 | Add/save shade metadata (`3204`, `index.html:465`) | Main: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A05 | Travel/tilt calibration settings (`index.html:545`) | Main timing/tilt configuration and safe validation; R4 preserves unsupported relay mappings rather than inventing wiring. |
| A06 | Step/reversal/features/repeats (`index.html:572`) | Main: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A07 | GPIO configuration (`index.html:480`, `584`) | Main: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A08 | Add/save group (`3291`, `index.html:692`) | Main: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A09 | Delete room (`3325`) | Main: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A10 | Delete shade (`3346`) | Main: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A11 | Delete group (`3368`) | Main: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A12 | Drag reorder rooms/shades/groups (`2190`, `2383`, `2530`) | Main: LocalEntityForm/Runtime, EntityFields/ProtoEdits, ModelCommit and administration/order; native actual adapters and renderer/input tests. |
| A13 | Pair wizard (`3451`) | Candidate0dbfe3d: exact device, bounded Prog review, observed learn mode/intended result and separate marker save; actual UI/adapter/FIFO tests. |
| A14 | Unpair wizard (`3522`) | Candidate0dbfe3d: exact device, bounded Prog review, observed learn mode/intended result and separate marker save; actual UI/adapter/FIFO tests. |
| A15 | Set paired metadata (`3425`) | Main metadata-only paired marker; candidate0dbfe3d adds separately confirmed post-programming marker. File retry never resends RF. |
| A16 | Rolling-code editor (`3391`, `3400`) | Main LocalPrimaryCounter + checked CounterRuntime; NVS/RAM aliases/current-file, final witness, quarantine and file-only retry tests. |
| A17 | Link physical remote (`3653`, `2833`) | Main explicit receive-candidate shade linking/removal; protected bounded cache, checked counter path and no first-frame mutation. |
| A18 | Unlink remote (`3903`) | Main explicit receive-candidate shade linking/removal; protected bounded cache, checked counter path and no first-frame mutation. |
| A19 | Add group member (`3680`) | Candidates2774835 list metadata/repair and03160d4 staged motor programming. Separate observations/list save; exact group identity and partial-file tests. |
| A20 | Remove group member (`3805`) | Candidates2774835 list metadata/repair and03160d4 staged motor programming. Separate observations/list save; exact group identity and partial-file tests. |
| A21 | Learn repeater (`3664`, `2833`) | Gap R2: main Repeaters list only removes. No LCD link_repeater/learn-add path in any ready candidate. |
| A22 | Remove repeater (`3893`) | Main exact repeater-list removal, consequence confirmation and file-only retry. |
| A23 | Remote button chords (`index.html:828`) | Candidate ebf4cf0: named chords/Prog,1–127 repeats, defaultCancel, existing RF-aware FIFO and acceptance-only result. |
| A24 | Sensor simulation (`3638`, `index.html:862`) | Candidate b6d1f42: device/group Sunny/Windy Keep/Off/On, defaultCancel and actual guarded typed-sensor/UI tests. |

## Settings, networking and integrations

| ID | Inventory row | Current status and evidence |
|---|---|---|
| S01 | General options (`index.js:1380`, `1421`) | Main five-section LocalSettingsForm/Runtime + ProtoSettings; checked typed loading/persistence/read/apply, field/secret/IP/revision/failure tests. |
| S02 | Security read/change (`1394`, `1496`) | Main five-section LocalSettingsForm/Runtime + ProtoSettings; checked typed loading/persistence/read/apply, field/secret/IP/revision/failure tests. |
| S03 | WiFi scan/choose (`1720`, `1733`, `1775`) | Main LocalWifiScan + owned WifiScanFeed: explicit scan, bounded results, review to draft only; actual runtime tests. |
| S04 | WiFi settings (`1840`, `1888`) | Main saved WiFi fields/secret retention. Live connected SSID/channel/signal detail remains partial (R6). |
| S05 | Ethernet selection/fallback (`1706`, `1840`) | Main conditional Ethernet fields and board/pin ownership validators; fake pin/SDK checks. Live adapter detail remains partial (R6). |
| S06 | Ethernet board/PHY/pins (`1611`, `1639`) | Main conditional Ethernet fields and board/pin ownership validators; fake pin/SDK checks. Live adapter detail remains partial (R6). |
| S07 | DHCP/static IP (`1797`) | Main five-section LocalSettingsForm/Runtime + ProtoSettings; checked typed loading/persistence/read/apply, field/secret/IP/revision/failure tests. |
| S08 | MQTT integration (`4018`, `4034`) | Main five-section LocalSettingsForm/Runtime + ProtoSettings; checked typed loading/persistence/read/apply, field/secret/IP/revision/failure tests. |
| S09 | Radio settings (`1982`, `2002`, `index.html:885`) | Main five-section LocalSettingsForm/Runtime + ProtoSettings; checked typed loading/persistence/read/apply, field/secret/IP/revision/failure tests. |
| S10 | Frequency/bandwidth/deviation/power (`3917`, `index.html:939`) | Main five-section LocalSettingsForm/Runtime + ProtoSettings; checked typed loading/persistence/read/apply, field/secret/IP/revision/failure tests. |
| S11 | Frequency scan start/progress/stop/use best (`2058`, `2080`, `2115`, `2123`) | Candidate fe27208: owned browser/LCD frequency scan, retained RF refusal and independent GPIO; stop/restore/review-to-draft tests. |
| S12 | Reboot (`1468`) | Main LocalSystem/SafeRestart: defaultCancel, shared verified-save/idle/restart admission; actual result/race/refusal tests. |

## Diagnostics, files and firmware

| ID | Inventory row | Current status and evidence |
|---|---|---|
| D01 | Firmware/app/hardware version, free/largest/min heap, uptime (`index.html:211`, `index.js:4187`) | Main real StatusAdapter internal free/min/largest, PSRAM, published loop rate/max and version/uptime; formatting/status/UI tests. Physical load qualification pending. |
| D02 | WiFi and Ethernet status (`1908`, `1921`) | Partial R6: main link/IP status present; complete connected WiFi/Ethernet fields not carried in bounded LCD status snapshot. |
| D03 | RF frame log (`2833`) | Main protected owned ReceiveHub diagnostics: bounded copied frames/pulses, Pause/Clear view, expiry/revocation/wipe; no RF/counter reset. |
| D04 | Copy frame log (`2916`) | Accepted browser clipboard equivalent; LCD copy-log browser guidance missing (R7). No clipboard/transfer implementation required. |
| D05 | Clear frame log (`index.html:987`) | Main protected owned ReceiveHub diagnostics: bounded copied frames/pulses, Pause/Clear view, expiry/revocation/wipe; no RF/counter reset. |
| D06 | Refresh Cache (`index.js:1266`, `index.html:250`) | Main truthful LocalSystem browser guidance; authenticated existing browser owns cache/backup/selected restore. Checked restore/counter paths remain required. |
| D07 | Backup/download (`4081`) | Main truthful LocalSystem browser guidance; authenticated existing browser owns cache/backup/selected restore. Checked restore/counter paths remain required. |
| D08 | Restore selected sections (`4149`) | Main truthful LocalSystem browser guidance; authenticated existing browser owns cache/backup/selected restore. Checked restore/counter paths remain required. |
| D09 | Manual firmware upload (`4219`) | LCD browser guidance main; candidate21f7e14 qualifies canonical manual firmware staging/status/install lifecycle with updater. |
| D10 | Separate web image | Main canonical bundled UI/410 route; generic prepared filesystem/update recovery candidate eebb167. Separate canonical web image retired by decision. |
| D11 | Firmware available/installed banner (`index.html:131`, `index.js:4213`) | Candidate21f7e14: same #12 verified HTTPS/digest/metadata service, standalone LCD review/install and boot-health/idle rollback; worker/HTTP/UI/package tests. |
| D12 | Verified GitHub checking/install (#12 integration required by issue) | Candidate21f7e14: same #12 verified HTTPS/digest/metadata service, standalone LCD review/install and boot-health/idle rollback; worker/HTTP/UI/package tests. |

## API-only and infrastructure coverage

| ID | Inventory row | Current status and evidence |
|---|---|---|
| I01 | Correct estimated current lift/tilt position | API-only inventory: existing APIs preserved, no current web caller or LCD exposure. Any raw-frame LCD proposal needs explicit reviewed policy; no new scope inferred. |
| I02 | Send a raw remote identity/frame | API-only inventory: existing APIs preserved, no current web caller or LCD exposure. Any raw-frame LCD proposal needs explicit reviewed policy; no new scope inferred. |
| I03 | Initial sync/resync, paging, schema/defaults, model event handling | Main bounded LocalCatalogue/RawTargetContext/stable-ID histories; sparse/full/duplicate/rename/recreate/order/concurrency tests. |
| I04 | Frame/frequency subscriptions | Main owned receive diagnostics; frequency ownership candidate fe27208. Feed lifetime, teardown and allocation/failure tests. |
| I05 | HTTP command routes | Main shared ShadeRequests/ProtoOps, LCD typed request preparation. Browser/HTTP routes preserved; no loopback. |
| I06 | Login/context/security session | Main LocalAuthorization/credential editor/adapter: same web credential,120s expiry/shared local throttle and exact30s STOP grace; actual auth/FIFO/capture tests. |
| I07 | uPnP/static assets | Main public-asset/auth policy and bundled assets; generic reader leases during filesystem update candidate eebb167. |
| I08 | Existing LCD brightness, dim/off, Back, details, screenshots | Main preferences/wake/debounce/Back/graphics loss/protected captures; actual GPIO/SPI/renderer/capture harnesses. No physical qualification claimed. |
