# H910P v0.6 — native firmware engineering (no UI redesign)

**Status:** Offline native firmware development. **NOT boot-tested and NOT flashable.**

Stable **v0.5.10 on `main`** and the existing custom management UI under top-level `source/` remain unchanged. This directory contains the ARM7 C firmware core, the native JSON API, validated 302-field configuration metadata, a read-only device-data provider interface, a loopback-only host server for test, and a standalone research-only ARM ELF linker target. No user interface files are part of this directory.

The v0.6 application replaces Dell's application/management stack *only when* verified native board drivers, safe startup and recovery are complete. Retaining the original bootloader is permitted if the installed H910P bootloader is proven compatible.

## What can be tested now

Run from the repository root:

```bash
cd v06
make clean all test arm-link-check
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

See [`docs/native-test-gates.md`](docs/native-test-gates.md) and [`docs/api-v1.md`](docs/api-v1.md).
