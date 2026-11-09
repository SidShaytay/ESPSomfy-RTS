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

The first concrete checkpoint stages CFG/WIFI/NTP/IP/MQTT/conditional ETH and
removes effects from their load methods. Legacy CFG/WIFI values are only staged;
readers perform no migration writes or cleanup. Startup radio staging, qualified
runtime gates and delayed migration/time effects are required before this feature
is complete. Radio Save must additionally consume the shared same-owner
queue/radio pause admission; a snapshot-only check is insufficient. Selected
restore outcome integration remains a separate required slice.

Native tests exercise actual loaders/writers/snapshots with fake NVS: complete
absence, individual read failures, invalid originals, per-field IP repair,
unrelated namespace persistence and high MQTT ports. They perform no hardware
access or network operation.
