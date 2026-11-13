# v0.6 native configuration persistence — power-failure-safe foundation

**Implemented as a hardware-independent C module. Not connected to the H910P flash driver.** The actual flash chip, partition map, erase blocks and UPS bootloader recovery behavior are still unverified. No flashable firmware generated or distributed.

## Design and boundaries

`board/config_journal.c` and `include/ou_config_journal.h` provide a two-slot, NOR-flash-oriented configuration journal with no heap, file system, RTOS, XML or I/O dependencies. All flash operations are abstract callback functions and are **disabled unless three explicit flags are positively set**: physical geometry verified, independent dedicated configuration slots verified, and writes enabled. Without these, it refuses writes.

Each slot is at least one hardware erase block and must independently erase/program/read. The proposed logical header format is 24 bytes in big-endian order:

| Offset | Size | Field |
| --- | ---: | --- |
| 0 | 4 | Magic `OU6C` |
| 4 | 4 | Record format version `1` |
| 8 | 4 | Monotonic 32-bit generation |
| 12 | 4 | Payload length (1–4096 bytes) |
| 16 | 4 | IEEE CRC-32 of payload |
| 20 | 4 | Commit marker `0xA55AC33C`, written **LAST** |
| 24 | 1–4096 | Caller-supplied settings payload (no parsing/credentials interpretation) |

The active slot is selected by a validated marker and a streaming CRC read, with wrap-aware generation comparison. Committing a new record **never erases the active slot**: the inactive slot is erased, written, fully read back and verified, then marked committed. This is a preparation for interrupted firmware/power writes; the recovery test does not rely on a live card. Raw secrets are not logged.

The module intentionally does **not** prescribe which actual flash sectors to use. The early Dell release notes report a sector-60 logs/filesystem overlap, so inventing an offset before verifying the current card could destroy logs or credentials. Integration must be conditional on the protected layout validator and actual erase geometry.

## Test evidence

- Host NOR emulator exercises 1→0 programming, slot erasure, full 4096-byte record, interrupted writes, bad CRC, inactive-slot reuse, read failures, write-lock refusals and non-power-of-two erase blocks.
- Freestanding ARM7TDMI big-endian compilation and link succeed; no proprietary Dell code or device I/O is included.
- The journal does not make the 302 settings writable: that requires a typed config schema, authorization, safe migration and actual protected flash regions.

Run from `v06`: `make clean all test footprint-check board-sanitize`. The output remains `native-research-NOT-FLASHABLE.elf` and `.dat` only, never a BIN.

**Future gate:** inspect real NMC memory controller and flash chip, verify two separate erase-aligned config slots outside boot/recovery/app areas, validate readback and watchdog power-failure behavior on recoverable hardware, then permit a physical backend.


## Native event archive — separate partition only

The 440-byte `OU_EVENT_ARCHIVE_VERSION=1` event snapshot is
distinct from settings data and must use its own independently
qualified two-slot journal. It stores the last 32 typed event records
and their boot-generation labels, next event ID and overwrite count.
It never stores credentials or raw device payloads.

`ou_event_archive_checkpoint` is an opt-in operation that calls
`ou_journal_commit` only when all original journal safety conditions
are met, and `ou_event_archive_restore` uses the CRC-verified
load path. Uncommitted, interrupted, malformed or corrupt snapshots
cannot change the in-memory event ring. A previous valid slot
survives failure of the latest slot. Unit/integration tests exercise
both slots, simulated NOR 1→0 writes, readback corruption,
invalid geometry, forbidden writes, restored ordering, and boot epochs.

No event flash partition has been verified on the installed H910P,
and no production runtime calls the checkpoint method automatically.

## Native typed configuration model (v0.6 development)

`src/config_model.c` and `include/ou_config_model.h` implement a
**bounded, validated and deterministic** configuration record separate from
legacy OEM settings and the event journal. The first nine deliberately
allowlisted, non-secret fields are host name, domain, DHCP/static IPv4,
IP address, mask, gateway, contact, location and language. They are checked
against the generated 302-field metadata so a changed schema fails closed.
The original 302-field inventory and all required management options remain
in scope; unsupported fields cannot be silently persisted.

Each field is type-checked; network profiles additionally require coherent
assignment mode, IP address, subnet mask, usable host/gateway addresses and
same-subnet gateway. An incomplete network draft may be edited locally but
**cannot** be serialized or committed. The canonical `O6CF` version-1
record has a 12-byte header followed by strictly ordered, one-byte
field IDs and lengths with no secrets. Parsing rejects unknown IDs,
duplicates, noncanonical addresses, control characters, overlong strings,
broken lengths, and inconsistent complete profiles without changing the
previously loaded model.

`ou_config_model_save` additionally requires an explicit external
authorization callback returning exactly 1 and the established two-slot
journal's independent physical partition, geometry and writes-enabled
gates. This callback is a boundary, **not** a completed login or TLS
implementation; nothing in HTTP/UI can invoke it. The recoverable journal
preserves the previously committed setting record on failed writes, and
the entire decoded state is swapped only after validation.

This remains **offline engineering only**. On-card credentials, signed
updates, physical flash mapping, safe network reconfiguration with rollback,
activation, and a verified configuration migration path remain unfinished.

## Two-phase management-network activation (offline model)

`src/config_activation.c` and `include/ou_config_activation.h` implement
a bounded, cooperative **apply → reconnect → confirm or rollback** transaction.
The model first loads an independently CRC-verified, committed baseline from
the qualified two-slot CONFIG journal. Before an apply callback can run,
it requires the complete validated candidate, a different active profile,
live authorization, existing-management reachability, a physically
qualified network adapter and rollback channel, a verified dedicated
configuration partition, and write-enabled storage. A stale or
concurrently changed journal generation rejects the attempt.

Applying a candidate **does not persist it**. The operator gets a
10–180-second confirm window; transient lost connectivity can recover during
that window. A hard link failure, timeout or explicit cancel restores the
previous physical profile while leaving its disk record unchanged. A
confirmation requires the authorized operator and proven reconnect, then
commits the new profile atomically. Failed commits trigger restoration.
An interrupted commit that actually wrote its final commit marker is
detected and, if possible, rolled back with a new verified journal entry;
an unexpected third-party journal modification is never overwritten
silently. If hardware restoration or persistence verification fails, the
controller reports `FAILED_ROLLBACK` instead of pretending the network
is accessible.

This code **does not** provide real H910P Ethernet reconfiguration,
remote authentication, recovery boot, TLS, or storage partitions. All
callbacks in the automated tests are simulated. The development HTTP
server still refuses all writes (405) and no browser control can invoke
`ou_config_activation_begin`. The physical adapter is deliberately
unbound until actual card support and independent recovery are proven.

Tests cover authorization denial, unverified network/rollback gates,
write-disabled storage, stale candidate rejection, reentrancy,
connectivity interruptions, bounded confirmation, 32-bit clock wrap,
manual cancel, incomplete apply, write interruption, ambiguous
post-commit readback, prior-record retention and fatal restoration errors.
