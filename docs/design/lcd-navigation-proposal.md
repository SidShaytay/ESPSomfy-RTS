# Approved LCD navigation

Sid approved this navigation tree. It is implemented and released in **4.8.0**; these paths apply to 4.8.0 and later. See the [versioned path map](../lilygo-local-ui.md#navigation-versions) for 4.7.3. Release and installation do not establish physical navigation acceptance or full raw-flash qualification; those checks remain separate.

The original action inventory used baseline `8b746f2`. The approved layout keeps three daily shortcuts together, separates browsing devices and groups from configuring the controller, and retains existing operations and privacy rules. Navigation adds no backend action or permission.

## Top-level tree

```text
Home
├─ Quick entry 1 ─┐
├─ Quick entry 2 ─┼─ Existing target controls (shared subtree below)
├─ Quick entry 3 ─┘
├─ Browse all
│  ├─ Rooms → selected room → Devices / Groups → target controls
│  ├─ Devices → target controls
│  └─ Groups → target controls
└─ This controller
   ├─ Configure
   │  ├─ General → existing complete General settings form
   │  ├─ Security → existing complete Security settings form
   │  ├─ Network → existing complete Network settings form
   │  │  └─ WiFi scan → Start → received list → choose → network draft
   │  ├─ MQTT → existing complete MQTT settings form
   │  ├─ Radio → existing complete Radio settings form
   │  ├─ Display → Brightness / Dim after / Sleep after
   │  ├─ Quick entries → select slot → Pin device / Pin group /
   │  │  Remove shortcut / Move up / Move down → Save review / Cancel
   │  └─ Rooms, devices & groups → existing Administration subtree below
   ├─ Maintenance
   │  ├─ GitHub updates → Check → eligible release review (Cancel default)
   │  │  → explicit Install and restart / Cancel → progress / result
   │  ├─ Restart → Cancel / Restart now → admitted/refused/result
   │  └─ Browser files & Web → Backup / Restore / Manual firmware /
   │     Refresh browser cache → existing browser instructions only
   ├─ Status → existing wake-up status screen directly
   ├─ About → existing scrolling Details page
   └─ Diagnostics
      ├─ Network details → current WiFi/Ethernet rows / Back
      ├─ Receive diagnostics → Begin observation / Cancel → received frames
      │  → frame details / pulses; Pause / Clear view / Back
      ├─ Radio frequency scan → explicit Start / Stop → Review → Apply to
      │  Radio draft → separate settings Save / Cancel
      └─ Copy receive log → existing browser-copy instructions only
```

Rooms retain the existing all-room/unassigned handling, stable IDs, filtering and bounded pages. Empty, unavailable and changed targets keep their current refusal behavior. Pinning a quick entry never changes a motor's MY/favourite.

## Shared target controls

Opening a target only navigates. Controls retain their current capability-dependent actions: Up, STOP, Down and Toggle; More contains lift percentage, tilt Up/STOP/Down/percentage, MY position, native 80-bit Favorite, Step up/down with step size, Sun on/off, Set favourite here and Clear favourite. The repeat page retains the reviewed action/step/count → Hold to send → release-cancels behavior. Unsupported actions remain unavailable; Light remains unavailable.

Numeric moves keep explicit Apply/Go; favourite programming keeps separate movement, observed-stop and programming confirmations. No additional RF/GPIO command, category support or driver mapping follows from this tree. Back from a quick entry returns Home; Back from a browsed target returns its originating list. Eligible moving-target STOP and critical update context must remain reachable and must not be displaced by idle navigation.

## Existing Administration subtree

This is one intact destination under Configure, preserving the current forms and stable-ID selection:

```text
Rooms, devices & groups
├─ Rooms → Add room / selected room (edit, delete) / Reorder
├─ Devices → Add device / selected device (edit, delete) / Reorder /
│  Advanced → choose device → device advanced subtree below
├─ Groups → Add group / selected group (edit, delete) / Reorder /
│  Advanced → choose group → group advanced subtree below
└─ Repeaters (currently More → Repeaters)
   → existing list / Learn-add → observe → choose frame → review → Add
   → selected repeater → confirmed Remove
```

Room editing retains name and sort order. Device editing retains name, room, address, category, protocol/frame bits, lift/tilt timings and tilt mode, step size, reversals, sun/light/simulated-MY settings, repeats and all applicable GPIO fields. Group editing retains name, room, address, protocol/frame bits, command reversal and repeats. Reorder retains the complete draft permutation and explicit Save/Cancel. All existing validation, confirmations and file-only retry outcomes remain.

- **Device advanced:** Paired marker; Linked remotes → inspect/confirmed unlink; Learn received remote → explicit observation/selection/review/link; Primary rolling code → explicit reviewed correction; Status details; Advanced remote; Pair/unpair guide; Sensor simulation.
- **Group advanced:** Member links → inspect/add/remove/repair, including missing members; Status details; Advanced remote; Motor programming → Pair/Unpair with the chosen device; Sensor simulation.
- **Advanced remote:** named Up + Down, MY + Up + Down, MY + Up, MY + Down and Prog; bounded repeats; default-Cancel send review and result.
- **Pair/unpair and group motor programming:** existing explicit Prog stages, user observations and separate metadata/member-list acknowledgement. Entry or navigation never transmits; a file-save retry never repeats programming.
- **Sensor simulation:** Sunny/Windy Keep/Off/On, repeats, reviewed send/Cancel and result.
- **Status details:** existing room/category/protocol/availability, estimated current/target lift/tilt, favourites, movement, sun/weather, membership and paired-marker rows, as applicable.

## Settings and read-only pages

The five settings forms retain **all current shared fields**, conditional board/protocol applicability, secret Keep/Replace semantics, explicit Save/Cancel and persistence results. This includes General identity/discovery/time; Security mode/account/PIN/password; Network adapter/WiFi/Ethernet/IP/DNS; MQTT connection/account/discovery/topics; Radio enable/protocol/frame/noise/pins/frequency/bandwidth/deviation/power. WiFi and frequency scans still only populate a draft; selection never connects, saves or installs by itself.

Status is the existing wake-up page, not a new status submenu or widget. About reuses only current Details data: firmware/application versions, existing uptime/power/link/address/web/radio/frequency/build rows and internal-memory/PSRAM/loop health rows. It introduces no hardware identity field. **Privacy requirements:** Every moved page retains its current authorization and screenshot protection. Public wake/Details views must not acquire protected Network, entity or receive data. These requirements are unchanged and do not need a new decision.

## Resolved layout and unchanged behavior

Administration stays together under Configure. WiFi scan is under Network, frequency scan and copy-log guidance are under Diagnostics, and Maintenance keeps the other four browser-guidance entries. About reuses existing Details and health rows.

Quick-entry management is under Configure. Status opens the existing wake screen directly; startup, wake-only input, held-button suppression, moving-target STOP and update-context retention remain unchanged. The navigation work does not implement the separately requested 30-minute installation lifetime or temporary-download retries.

There is no new lock/reset/repair wizard, clipboard, file-transfer session, raw-frame action or estimated-position correction UI. Source implementation is not physical acceptance evidence.

## Source basis

See [current navigation and input](../../src/LocalUi.cpp), [Home and Details rendering](../../src/LocalUiRenderer.cpp), [Administration](../../src/LocalAdministration.cpp), [advanced actions](../../src/LocalAdvanced.cpp), [entity fields](../../src/EntityFields.cpp), [settings](../../src/LocalSettingsForm.cpp), [System destinations](../../src/LocalSystem.cpp), [update workflow](../../src/LocalUpdates.cpp) and [receive diagnostics](../../src/LocalDiagnostics.cpp). The [capability audit](../lcd-capability-audit.md) remains the implementation inventory; this navigation document is not acceptance evidence.
