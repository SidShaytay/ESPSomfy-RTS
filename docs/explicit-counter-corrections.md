# Checked explicit counter domain

Browser primary-counter correction and remote linking use the checked loop-owned
domain. This feature adds no LCD counter/learning screens and accesses no hardware
during development.

An explicit primary correction or linked-remote code acquires the existing
GitUpdate counter owner, then reserves its identity under the TX queue lock.
Admission checks affected shades, group aliases and related queued group work.
Group readiness uses the shared complete-member classifier: fully tracked idle
members remain eligible after a completed Up/Down even when the group remembers
that direction. Missing members, active members or untracked movement evidence
refuse a correction. Empty physical slots (0/255) are ignored.
Motion, a pending behaviour operation, an active GPIO pulse, waiting/taken TX,
any repeater work, TX suspension or a pending restart refuses the edit. There
is no queue flush or radio operation. Before correction or linking, the loop
queue must have no pending work (the current popped marker is excluded).
Otherwise the operation refuses without changing history, preserving an already
accepted unrelated STOP. All live shades must also be administratively idle,
including unrelated motion, held input, unresolved TX and GPIO pulses. This
counter-edit restriction preserves ongoing/grace STOP context under the current
global model-history binding; it does not freeze commands. File-only recovery does not change that history and
retains its existing queue allowance. Locks are released before NVS/file I/O.

Received/browser linking freshly reads retained NVS under the same reservation
and compares the proposed code with NVS and every live RAM alias. It refuses a
lower code, including a retained key after unlink and 65535-to-0; only exact key
absence counts as zero. A read error refuses before writing, without inventing a
zero value or a new quarantine. Deliberate lowering remains the separate explicit
primary-counter correction operation.

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
An unrelated save does not silently clear quarantine. A browser counter correction
that applied but failed file persistence closes its original dialog and offers
only the existing **Retry Commit** action. The Firmware panel also always offers
**Save current model**, independently of the warning banner; it saves the current
model file, never writes rolling codes or sends motor commands. That deliberate browser COMMIT carries
an internal counter-file-recovery mode through the same queue; generic metadata
file retries and automatic saves cannot clear quarantine. Unknown NVS refuses this
recovery without another write. Replies reuse existing applied/durable/model-file
fields; neither the correction nor linking is replayed on persistence retry.

While reserved or quarantined, the exact identity is rejected before counter
peeking, frame/behaviour changes, GPIO output and scheduler admission. Unrelated
commands remain available. A group containing a quarantined primary cannot start
new movement. Its independent My STOP is allowed only for one unambiguous group
with complete, ordinary RF-compatible members, all currently moving. Mixed/idle,
toggle/dry-contact, missing-member or same-address alias contexts refuse; there
is no idle My or fallback transmission. An existing unquarantined alias collision
can start an idle alias's favourite; this feature preserves ordinary behaviour and
refuses that ambiguity only for the quarantine STOP exception.

Raw repeater frames captured after reservation/quarantine cannot retransmit the
blocked identity (or a group start that would affect its primary). Only the
refused head is retired; later unrelated frames stay in order. Decoding uses a
copy only while the policy is active. Pre-existing waiting/taken/raw repeater
work prevents correction admission, so correction does not flush accepted work.

RX still observes motion while blocked counter writes are skipped. The guarantee
is this requested operation's verified storage, not global RF anti-replay or motor
acceptance: the existing unblocked RX path accepts a checksum-valid, non-Sensor
linked frame without a monotonic comparison and can later write a lower code.
The automatic TX/RX algorithm is unchanged. Future learning UI must independently
bind candidate freshness, identity, session and observation high-water.

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
