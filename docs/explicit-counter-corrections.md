# Checked explicit counter domain

This checkpoint adds a loop-owned domain but does not expose or activate it.
Existing browser setters are unchanged until the transport/RX guards and typed
caller wiring are reviewed. The following describes the checked domain only.

An explicit primary correction or linked-remote code acquires the existing
GitUpdate counter owner, then reserves its identity under the TX queue lock.
Admission checks affected shades, group aliases and related queued group work.
Motion, a pending behaviour operation, an active GPIO pulse, waiting/taken TX,
any repeater work, TX suspension or a pending restart refuses the edit. There
is no queue flush or radio operation. Locks are released before NVS/file I/O.

The NVS writer checks open, set, commit and readback. A definite pre-write
failure leaves running counters and a proposed new link unchanged. Once a
write was attempted, an uncertain result quarantines that exact identity;
there is no rollback or cached counter replay.

After verified NVS, every matching live shade primary, linked remote and group
primary adopts the value in RAM. A new linked slot is installed only then.
Affected configuration histories advance, and old repeat/held-frame contexts
are invalidated even for a same-value explicit correction. Unrelated primary
counter values remain unchanged. The domain then commits the CURRENT model
through the existing checked model-file writer. This matters for a lower
correction: the unchanged loader keeps the maximum of file and NVS counters,
so an old higher primary/group value in the file would otherwise undo it.

A file failure reports applied but not durable. The verified NVS/RAM value and
new link stay applied; the identity remains quarantined as file pending. An
explicit file-only recovery can serialize the current model again without
writing NVS. Only a held known-NVS/file-pending recovery can clear this cause
after verified file success. Unknown NVS state cannot be cleared this way.
An unrelated save does not silently clear quarantine.

NVS and the model file are separate stores. Managed restart must remain blocked
while either quarantine cause exists. Sudden power loss between the stores can
still expose an older higher file value at boot; this operation is not atomic
under power loss. The loader's anti-stale maximum policy is preserved. There is
no journal, boot-time stale rewrite, automatic retry or replacement TX algorithm.

The domain scans at most 32 shades with seven linked slots and 16 group
identities, without collecting a model copy or alias array. Outcomes are capped
at eight bytes; the serialized quarantine policy is capped at 12 bytes.

The offline harness compiles the real model header, checked NVS writer, shared
owner, behaviour and scheduler. Hardware endpoints and model-file I/O are fake;
reservation/identity methods are extracted verbatim from production sources.
Cases check lower corrections across aliases, the loader maximum, every file
failure stage, uncertain NVS without replay, file-only recovery, slot capacity,
GPIO/held-frame/motion admission, waiting/taken/related-group and repeater work,
owner exclusion and same-value repeat invalidation. No hardware is accessed.
