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
