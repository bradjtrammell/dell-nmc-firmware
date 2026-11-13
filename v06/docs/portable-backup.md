# H910P portable configuration backup and safe recovery staging

## Implemented native portable backup

`src/backup.c` and `include/ou_backup.h` implement a freestanding, bounded, deterministic **O6BK version-1** portable backup for the three independent *non-secret* modeled management sections:

1. System/network configuration (nine allowlisted values, no passwords or protected credentials).
2. Three-outlet shutdown policy (all existing modeled master and outlet thresholds).
3. Seven-day, 30-minute weekly ON/OFF schedule.

The native binary record has a 48-byte header followed by the three existing versioned, canonical records. Header fields define version, exactly three sections, total length and individual lengths; a SHA-256 digest checks the header and every payload byte. Maximum record is **1,536 bytes**, with no dynamic allocation and no architecture-dependent structure padding. The SHA-256 digest detects accidental corruption; **it is not a digital signature and cannot authenticate the file or authorize a restore**.

`ou_backup_export` requires a positive separately supplied authorization callback and validates every section *before* creating a complete deterministic record. `ou_backup_preview` verifies exact lengths, section count and ordering, reserved fields, SHA-256 and the individual validated record codecs, and only commits the result to the caller's **in-memory model** when all sections pass. A maliciously rehashed bad inner record is rejected by its independent strict schema decoder.

The native backup API does **not** contain an apply/commit method. This omission is deliberate: the existing CONFIG, SHUTDOWN and SCHEDULE journals are independently transactional, but restoring all three one by one could leave inconsistent configuration if power fails between writes. An authenticated **group transaction coordinator** with a durable recovery intent, previous-generation references, staged candidate records, and power-loss recovery is required before on-card restore can be enabled. Hardware flash geometry, independent recovery, credentials, live settings and TLS must also be verified. Do not invent these prerequisites by marking the UI's review action as an actual backup.

## System-page operator workspace

The System management page now offers **Export local workspace** and **Import local workspace JSON**. This is intentionally a separate *human-editable review format*, not the binary O6BK package and **not an installable card configuration**. It includes only local draft values already entered under Network, System, Weekly Schedule and Shutdown Parameters, with exactly three named sections, `applied:false` and `installable:false`. It contains no account database, password, private key, SNMPv3 secret, endpoint credential or firmware asset.

Imports are size-bounded at 16 KiB, require the exact version/section and supported key inventories, validate all per-field types and ranges, reject unknown keys, unsafe/incomplete static network profiles, invalid schedule slots or same-day conflicts, and invalid shutdown thresholds. Invalid/malicious files **cannot partially change any of the three local models**. A successful import changes only the current browser-memory drafts, with an explicit warning that nothing was written to the H910P. The UI's local JSON is not represented as the native binary backup or as proof of a physical device snapshot.

## API and qualification

`GET /api/v1/backup/capabilities` truthfully reports the compiled native backup codec, its v1 format and 1,536-byte maximum, three supported sections, and that no live device backup, credential import, on-card restore, cross-partition atomic commit or cryptographic backup signature is currently enabled. Host simulator GET/HEAD remain read-only and all HTTP mutations return 405. No `/backup/restore` or `/backup/apply` endpoint exists.

## Tests and still-required work

The host C suite covers known-good round-trip, deterministic re-export, auth denial, truncated/oversized files, version/reserved-field corruption, malformed section lengths, SHA mismatch, independently invalid section after attacker-controlled rehash, and all-or-nothing target immutability. The browser suite covers System export, valid local import, rejection of unknown/secret keys, invalid outlet ranges and retention of previously validated draft values. HTTP tests assert truthful read-only capabilities. Big-endian ARM7 linking and host sanitizers are required.

Outstanding production features include a protected backup encryption/signing policy when secrets are eventually included, whole-device configuration inventory support beyond the initial nine fields, coordinated/transactional multi-section restore, verified flash layout and physical recovery, and authenticated HTTPS administrator workflows. Neither firmware updates nor power controls are changed by this feature.
