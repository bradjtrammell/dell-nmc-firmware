# Native notification routing policy — crash-safe journal

The native notification rule engine now has a **versioned, fixed-memory,
non-secret persistent policy codec**. `src/notify_policy.c` and
`include/ou_notify_policy.h` add a strictly canonical
`O6NR` version-1 record containing at most eight routing rules.

## Record layout and safety

An eight-byte header includes magic `O6NR`, version, configured rule count
and reserved zero bytes. Eight ordered 12-byte records contain each rule's
enabled state, email/webhook/SNMP-trap transport, provisioned destination
slot (0–7), eight-event bitmask and cooldown in milliseconds. Unused
records must be entirely zero. Total record length is **104 bytes**;
encoding is big-endian and independent of C structure layout.

The decoder rejects invalid versions, oversized rule counts,
unknown transport codes, invalid destinations, event-mask overflow,
invalid cooldowns, nonzero reserved bytes, and nonzero unused slots.
It validates every rule before changing its target engine. Restoring
policy withdraws any queued notifications from previously provisioned
destinations; it never transfers jobs into a newly configured recipient.

## Crash-safe storage model

`ou_notify_policy_checkpoint` requires an externally supplied,
positive authorization callback, an independently verified
NOTIFICATION journal partition, known NOR erase geometry and write-enabled
storage. The existing dual-slot journal handles generation counters,
CRC verification and commit-marker-last updates. A failed staged
write preserves the last completely committed rule set. The reload
path validates the loaded record before changing the active engine.

The native service exposes
`ou_native_service_notifications_checkpoint` and
`ou_native_service_notifications_restore`. Restore is only intended
for service startup, before source-dependent alerts are queued; the
implementation refuses a known active verified telemetry session
or pending alerts. These are **explicit BSP operations** and
are not automatically called on the card.

**No physical NOTIFICATION partition has been verified.** This is
a storage model, not a firmware claim that configuration now survives
reboots in deployed hardware. Real authenticated administration,
credential provisioning, durable flash layout and power-loss
verification remain necessary.

## API/UI and tests

The read-only notification-policy endpoint now advertises
`policyJournalCompiled:true`, `policyRecordBytes:104` and
`policyPersistenceQualified:false`. Email and SNMP rule-editing pages
remain local drafts with no mutation capability.

Host tests validate deterministic record round-trip, eight-rule
limits, strict reserved fields, rule semantics, corrupted final record,
authorization failure, unknown geometry, disabled writes,
interrupted second-slot commits, falling back to a prior valid record,
and all-or-nothing live engine behavior. Native-service build checks
assert unqualified storage is refused. ARM7 link and sanitizers must
pass before this change is accepted.
