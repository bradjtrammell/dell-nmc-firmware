# H910P v0.6 — native firmware engineering (no UI redesign)

**Status:** Offline native firmware development. **NOT boot-tested and NOT flashable.**

Stable **v0.5.10 on `main`** and the existing custom management UI under top-level `source/` remain unchanged. This directory contains the ARM7 C firmware core, the native JSON API, validated 302-field configuration metadata, a read-only device-data provider interface, a loopback-only host server for test, and a standalone research-only ARM ELF linker target. No user interface files are part of this directory.

The v0.6 application replaces Dell's application/management stack *only when* verified native board drivers, safe startup and recovery are complete. Retaining the original bootloader is permitted if the installed H910P bootloader is proven compatible.

## What can be tested now

Run from the repository root:

```bash
cd v06
make clean all test footprint-check
./build/openh910p-sim --port 8760
```

`make test` executes native C API tests, native provider stale-data and sanity checks, JSON endpoint integration tests, schema-generation tests and offline OEM-reference tests. `make arm-link-check` compiles and links a **full standalone big-endian ARM7 ELF** with the observed Dell entry vector and verified reset literal. It is not a BIN and has no board drivers.

From another terminal **on the same machine**:

```bash
curl --fail http://127.0.0.1:8760/api/v1/health
curl --fail http://127.0.0.1:8760/api/v1/ups/status
curl -i -X POST http://127.0.0.1:8760/api/v1/ups/outlets
```

**All values in the HTTP host adapter are simulated.** That adapter binds only `127.0.0.1`. It cannot connect to or control the live NMC.

## Architecture

- `include/openh910p.h`, `src/core.c`: bounded freestanding C99 JSON API; typed health/device/capabilities/status/measurements/outlets/events and paged schema metadata. All mutating routes reject with 405.
- `include/ou_schema.h`, `src/schema.c`: all 302 validated field names, types, units and read/write classifications, no configured values or secrets.
- `include/ou_provider.h`, `src/provider.c`: pure read-only provider contract; accepts a verified driver's freshly received measurement frame, enforces physical ranges and freshness and rejects unverified/stale data with an **unavailable** snapshot, never a stale sample.
- `host/server.c`: POSIX development-only JSON HTTP adapter, bound to loopback, no TLS, no credentials, no browser assets.
- `target/reset.S`, `target/linker.ld`, `target/runtime.c`, `target/compiler_rt.c`: independent ARM7 application startup and linker research. No MMIO, flash, UART, network or physical power operations.
- `tools/verify_native_image.py`: independent ELF32 big-endian ARM entry/vector shape check. Produces no installable BIN.
- `tools/verify_oem_parity.py`: **offline development-only** parser for nine numeric fields from saved OEM XML snapshots. No NMC network access; XML is not used by the target firmware.
- `tests/`: pure C tests and Python HTTP/schema/parity regression tests.

Native provider states are `unavailable`, `simulator`, and `native`. API responses distinguish `"source"` and `"simulated"`. Physical telemetry and event routing fail closed if not available; simulator values must never appear in a native-hardware context. A driver cannot claim operational support without a validated sample.

## What is still blocking on-device testing

No confirmed H910P memory controller/flash geometry, bootloader recovery, native UPS protocol driver, NMC Ethernet stack, device-side authentication/TLS, management writes, signed OTA or rollback. **Do not attempt to flash the linked ELF or raw `.dat`**. These omissions are substantial; `make arm-link-check` is a code generation and linker test only.

See [`docs/native-test-gates.md`](docs/native-test-gates.md), [`docs/resource-budget.md`](docs/resource-budget.md) and [`docs/api-v1.md`](docs/api-v1.md).

## Hardware footprint discipline

The v0.6 native development build now has a **measured size gate**: `make footprint-check` generates `build/resource-report.json` from the ARM ELF sections and raw research binary. Limits in `resource-policy.json` are intentionally **provisional**, not claims about real NMC flash/RAM. The audit refuses deployment qualification. Hardware flash partitions, bootloader protection and recovery have to be measured and proven before a flashable image is produced. This keeps the existing UI and stable v0.5.10 unchanged while we modernize the underlying application.

## 2026-native board support engineering (read-only)

- `include/ou_ns7520_mem.h` and `board/ns7520_mem.c` provide a real NS7520 chip-select register decoder from Digi's documented `0xFFC00000` controller. It accepts *injected read-only register values* through a callback. There is deliberately **no on-card MMIO adapter wired up** yet, so the installed H910P cannot be touched by this research code. A decoded address window is **not** confirmed physical RAM or flash capacity.
- `include/ou_flash_policy.h` and `board/flash_policy.c` validate independently verified flash capacities, erase blocks, and non-overlapping protected bootloader/recovery/application/config/log partitions. No flash writes, erase or update routines are included. Geometry/recovery flags must be positively verified; unknown defaults fail closed.
- `include/ou_ups_link.h` and `board/ups_link.c` provide a bounded, receive-only observer for **already-delimited raw frames**. It records count, length and fingerprints, **not content**, and cannot manufacture a verified UPS measurement or transmit commands. The physical link and protocol framing have not been confirmed.
- `tools/probe_oem_transport.py` performs an offline, SHA-256-allowlisted comparison of Dell 01.16.0001 and 01.18.0005 binary strings. Both reference HID/Report/COPI logic, but this **does not prove a USB, RS-232, UART or any other physical interface**. OEM byte streams are not copied into the repository.

The new board modules are included in the big-endian ARM7 compile/link/footprint audit and in native C regression tests. Running `make clean all test footprint-check` still makes **no device-installable image**. Physical hardware tests remain blocked on a verified recovery mechanism and safe NMC↔UPS data path.

For OEM-backed chip-select register evidence and the status of the native UPS transport work, see [board-and-ups-transport.md](docs/board-and-ups-transport.md). The new C modules require no platform headers or proprietary binaries to compile; nothing in this v0.6 work touches the approved UI or stable branch.

## Crash-safe native configuration storage (no physical flash writes)

`board/config_journal.c` implements an independent 2-slot, 24-byte-header, CRC-checked, NOR-friendly configuration store. It is intentionally **not** connected to any device erase/program routines: it refuses writes unless real geometry, separated config regions and write authorization have been verified. The host NOR emulator tests interrupted writes, corrupted payloads and recovery to the previous slot. See [configuration-journal.md](docs/configuration-journal.md). This is a foundation for later typed JSON configuration, **not** a functional v0.6 settings updater on the H910P yet.

## Native UPS telemetry anti-replay and freshness guard

`src/telemetry_guard.c` and `include/ou_telemetry_guard.h` now extend the
existing validated-sample provider with a **stateful, fail-closed** acceptance
boundary: samples must be verified, in forward sequence, fresh (5-second
budget), have advancing monotonic receive time, and not rewind source uptime.
Rejected, repeated or expired reports immediately hide previously accepted
telemetry rather than presenting a stale measurement as live. Both sequence
and monotonic tick wraps are handled without signed arithmetic. Explicit reset
of the history is permitted **only after an independently verified new physical
source session**; malformed input cannot reset replay protection.

This adds portable C functionality and test coverage to the research ARM7 ELF,
not an H910P transport driver. The future physical driver must verify framing,
checksum, physical source identity and session before setting `verified`.
There is no network access, flash write or power command in this component.
`make test arm-check board-sanitize` now includes its replay/staleness,
overflow, session-restart and failure tests. No native HTTP service presently
uses this guard because no verified H910P UPS transport exists.


## Offline OEM ARM callsite and recovery-image investigation

The new `tools/analyze_oem_arm.py` processes only pinned, CRC-validated
original Dell OEM BIN archives. It performs ARM7 PC-relative literal-pointer
scanning for possible UPS transport and NS7520 register references, verifies
embedded bootHdr/CRC/LZSS2 recovery-image candidates, and compares the
01.16.0001 / 01.18.0005 release evidence in a sanitized JSON report.
It provides concrete targets for hardware-driver reverse engineering but
**cannot prove any physical bus or bootloader recovery behavior**. This
is an analysis utility only; no flashable firmware is generated. See
[ARM OEM forensic methodology](docs/oem-arm-forensics.md) for the two-archive
command and evidence limits.

## Native NS7520 passive board qualification

The research ARM7 target now links board/board_probe.c, a whitelisted
23-register read-only inventory with documented GEN, MAC, serial and memory
controller register addresses. It checks the OEM RAM load address against
a decoded DRAM aperture but never claims physical capacity or boot recovery.
No physical MMIO adapter is present; flash qualification always remains false.
See docs/board-probe.md and its C/ASAN/ARM build tests.

## Guarded native ARM7 reset handoff

The independent research-only reset entry now verifies that the loader did
not enter unprivileged User mode, explicitly enters ARM Supervisor mode with
IRQ/FIQ masked before using the stack, and parks on invalid handoff. The OEM
first vector and reset literal remain unchanged. The ELF image verifier now
checks these exact ARM7 instructions and verifies that the User-mode rejection
branch lands on a self-halt. This does not create an H910P BSP, validate RAM,
or allow native firmware to be installed. See tests/test_native_image.py.

## NET+OS alpha image packaging (offline, not operational)

`make alpha-lab-image` produces a **structurally valid NET+OS bootHdr v2 / flags 9 / LZSS2 / CRC** image from the freestanding big-endian ARM7 application and an accompanying JSON integrity/safety manifest. The original card has previously accepted modified Dell-derived images through the HTTP/1.0 uploader; this preserves the observed container format without containing Dell's application code. This independent image still has no H910P MAC/UPS drivers and is NOT safe to install on the operating card. No deployment command or device access is part of the build. The literal-only encoder is deterministic; all bytes, CRC, header and the decompressed application are checked before output.
