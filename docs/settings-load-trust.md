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
without acquiring another owner. A stale cleanup capability latches a visible
cleanup fault and retains its owner, producer seal and radio token. It cannot
resume/release another session, retry itself or admit another radio save; no
automatic fault recovery is claimed.

A checked initialization-marker failure after live adoption reports an applied
partial outcome, advances the radio section revision and closes the old LCD
draft. It never claims running settings stayed unchanged or emits complete-save
notification. The operator must reopen Radio; a button cannot replay the old
draft. The compact result keeps its original 16-byte host/8-byte target size,
and the existing 64-byte restore-status budget remains unchanged.

Selected restore stages private typed namespace drafts from the same immutable
backup snapshot before counter promotion or live adoption. Opt-in checked record
decoding rejects truncation, cast wrapping, malformed tokens and required missing
fields while ordinary boot decoding keeps its legacy defaults. Whole selected
namespaces must validate before any writer runs; later failures re-mark earlier
verified draft writes uncertain when RAM remains unchanged. Radio runs last
under the existing exact owner/pause capability. Its applied partial result
retains the radio verified/applied mask without certifying other RAM adoption.

Existing backups omit WiFi and MQTT credentials. WiFi is never rewritten or
qualified by Network restore. MQTT needs readable carried enabled/credential
fields; otherwise it refuses before effects. Only fully covered CFG/NTP/IP/radio
records can qualify failed namespaces. Legacy missing checkForUpdate needs a
readable carried CFG; MQTT records before version 22 cannot stand in for settings.
Foreign or incomplete Ethernet records preserve qualified current hardware
values, while complete same-source records require checked source identity.

Projected GPIO validation uses the final candidate radio/Ethernet claims before
model adoption, without exposing private values. None owns no pad. Real output
pads need qualified radio and, on Ethernet builds, CFG mode and ETH provenance.
After complete writes and RAM adoption, existing hooks update hostname/time and
defer MQTT reconnect to the ordinary loop; settings notifications request no
restart. Namespace applied masks are published only after these hooks return.
The restore resource consumer still requires independent joint qualification.
Three eight-byte synchronized ledgers retain radio, Ethernet and configured
GPIO output pad claims for this boot. They add claims before hardware initialization attempts and never
release them because RAM, enabled state or the init marker changed. Projected
GPIO checks include those retained claims; reusing an old hardware pad requires
a reboot and a second explicit edit. GPIO claims are recorded before actual
output initialization; removing a model record or changing its protocol does
not free that actuator's old pad for radio or Ethernet bus use. GPIO validation
does not compare an output against its own historical claim. Selected restore
captures the old GPIO claims before model overwrite and rejects a new bus
conflict before any NVS or model mutation; low-level bus admission also checks
the fresh boot ledger. Ethernet includes the pinned SDK's fixed
RMII data/clock pads as well as configured pins. Native pin lists are synthetic,
not physical qualification. New radio/Ethernet activation also checks current
model GPIO ownership before writes or SDK calls. A disabled cold radio never
accesses a bus that has not actually been initialized.

Restore checks its claimed source before each namespace write and before live
adoption. Expiry after a verified write leaves a truthful partial outcome and
marks unadopted durable drafts uncertain; it never replays old values. No source
or token failure becomes automatic recovery. New failed-namespace
replacement flows remain pending Sid's choice and are not implemented here.

Native tests exercise actual loaders/writers/snapshots with fake NVS: complete
absence, individual read failures, invalid originals, per-field IP repair,
unrelated namespace persistence and high MQTT ports. They perform no hardware
access or network operation.
