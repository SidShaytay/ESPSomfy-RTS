# Checked namespace loading

The namespace reader distinguishes exact absence from failed reads. Each concrete
settings loader reads into a bounded typed draft, then publishes only a complete
structurally readable result. Optional missing legacy keys retain their specified
existing defaults. Read errors, wrong NVS types, oversized strings and malformed
booleans are unavailable; they do not become stored defaults. WiFi/MQTT temporary
credentials are wiped.

Read status is separate from write durability. Readable values outside the shared
field metadata remain visible for explicit replacement while their runtime effects
stay blocked. Invalid IPv4 text retains its exact original in an 80-byte shadow
(five 16-byte fields). A validated replacement clears only its own shadow; Cancel
and unrelated field changes preserve the rest. An unresolved original prevents
writing that whole affected namespace. Other qualified namespaces remain usable.
Unknown or failed namespace reads require deliberate validated selected restore;
there is no reset/default-replacement path.

The socket/LCD snapshot refuses an entire section before emitting fields when a
required namespace is structurally unavailable. Readable invalid originals are
still returned for the existing Replace editor. Historical signed NVS MQTT ports
retain their unsigned runtime bit pattern, including 32768 through 65535.

Startup stages CFG/WIFI/NTP/IP/MQTT/conditional ETH and radio, including the
radio initialization marker, before delayed migration/time effects. All load
methods are read-only. Legacy migration only runs after complete checked reads,
and does not replay on repeated startup-finalization calls. Obsolete CC1101 keys
are retained. Radio frequencies must fit the pinned driver's literal PA/calibration
software-admitted bands (300–348, 378–464 or 779–928 MHz); the integrated board keeps its
387–464 MHz restriction. Gaps and nonfinite originals remain invalid, without
clamping or remapping. The driver uses 378 MHz despite a contradictory 387 MHz
comment; these checks do not establish physical RF qualification outside the
integrated profile. Unsupported originals are labelled invalid in the LCD form;
Cancel preserves them. Only explicit Replace seeds 433.420 MHz, with separate
Apply and Save boundaries and no change to other drafts. The existing fallback SoftAP is transport only on bad network
reads: no unknown stored connection/SSID/IP/Ethernet values are applied, no NVS
reset occurs, and security authentication remains unchanged.

Unavailable radio settings still allow synchronization and empty queue resources
to initialize for authenticated recovery admission. Every board withholds radio
availability and RX until checked marker writes and hardware initialization
succeed. Marker-only success never certifies the entire namespace or clears old
write uncertainty. Pin-ownership settings must be usable before physical GPIO
admission or output; ordinary RF is unaffected by bad network, clock or MQTT
loading. The new GPIO failure uses the existing validation-code field.

Restart/upload admission and OTA health consume read trust separately from write
durability. Health also requires radio initialization when enabled, no uncertain
settings writes and no dirty uncommitted model. An unavailable snapshot closes
its LCD form with an explicit unavailable notice, without entering Replace or
retaining a draft. Only readable invalid originals use the existing Replace flow.

Live Radio Save now acquires the existing configuration owner, proves every
model member administratively idle, seals the loop producer queue without
removing accepted work, and retains the exact shared radio pause through
persistence/application. Scans, foreign owners, queued/repeater/radio work and
movement refuse before persistence. Cleanup retains the owner and seals when
resume is busy, then reduces them on later loop passes; there is no flush or RF
command. Already-owned restore uses a separate exact owner/pause-token path,
without acquiring another owner.

A checked initialization-marker failure after live adoption reports an applied
partial outcome, advances the radio section revision and closes the old LCD
draft. It never claims running settings stayed unchanged or emits complete-save
notification. The operator must reopen Radio; a button cannot replay the old
draft. The compact result keeps its original 16-byte host/8-byte target size,
and the existing 64-byte restore-status budget remains unchanged.

Selected restore outcome integration remains required. Existing backups omit WiFi and
MQTT credentials, so they cannot qualify unreadable uncovered fields using
running defaults. New failed-namespace replacement flows remain pending Sid's
choice and are not implemented here.

Native tests exercise actual loaders/writers/snapshots with fake NVS: complete
absence, individual read failures, invalid originals, per-field IP repair,
unrelated namespace persistence and high MQTT ports. They perform no hardware
access or network operation.
