# v0.6 independent firmware — test gates and hardware safety

**Status: ARM7 native core + JSON services + offline-linked research ELF. NOT flashable.**

The v0.5.10 `main` firmware and the existing `source/` UI are unchanged. The `v06` directory contains only underlying firmware engineering: freestanding C code, a read-only native data-provider boundary, a host-only JSON API simulator, ARM7 reset/linker research, and tests. There are **no HTML/CSS/JS design files** under `v06`.

## What is actually implemented

| Area | Implemented and tested | Does not yet exist |
| --- | --- | --- |
| Independent JSON core | Freestanding C99; typed routes; strict bounded serialization; 302 OEM schema definitions in 19 JSON pages | Native hardware service and persistent config |
| Read-only data provider | `ou_provider_accept` validates version, verified framing flag, reading ranges, monotonic age <=5 sec, and clears state on failure | Actual H910P NMC↔UPS electrical transport driver |
| HTTP | Loopback-only host adapter, exact API routes, security headers, denied mutations and XML paths | H910P Ethernet driver, TCP/IP, TLS and sessions |
| ARM7 startup | Big-endian ELF32 link at OEM header's `0x08004000` destination, reset vector and no unresolved external symbols | Verified board RAM controller, flash/ROM mapping, watchdog, NIC and UART initialization |
| Offline captured-data reference | Private OEM XML numeric data validated into typed reference measurements (no secrets) | Runtime XML parser: deliberately absent |
| Safety | 503 for absent/stale UPS telemetry, 503 for unimplemented native events, power writes always 405 | Safe field-tested power controls, firmware updater and recovery |

## Host-only validation (does NOT access the UPS)

From the repo root:

```bash
cd v06
make clean all test arm-link-check
./build/openh910p-sim --port 8760
```

From a second terminal on that same Linux machine:

```bash
curl --fail --silent --show-error http://127.0.0.1:8760/api/v1/health
curl --fail --silent --show-error http://127.0.0.1:8760/api/v1/ups/status
curl --silent --show-error -X POST http://127.0.0.1:8760/api/v1/ups/outlets
```

Expected: health/status explicitly show `"source":"simulator"`, `"simulated":true`, and read-only power action returns HTTP 405 JSON `READ_ONLY`. Nothing listens on a LAN address and no XML endpoint exists. You can examine all metadata through `GET /api/v1/configuration/fields` and `/api/v1/configuration/fields/0` to `/18`.

To verify the private saved H910P snapshot **offline only**, run from the `v06` directory:

```bash
python3 tools/verify_oem_parity.py /path/to/private/ups.xml
```

This emits only nine numeric UPS measurements, without contact data, network configuration or credentials. It is a development reference check, **not a production XML dependency**.

## ARM target output

`make arm-link-check` creates:

- `build/native-research-NOT-FLASHABLE.elf`: complete independent big-endian ARM7 research ELF, reset vector and JSON core linked.
- `build/native-research-NOT-FLASHABLE.dat`: raw research machine code extracted from ELF, **NOT a flash image**.

It intentionally does **not** generate a `.bin` with Dell's `bootHdr` or a flash updater. The runtime has no verified board initialization, network or UPS transport and deliberately parks after generating an internal diagnostic. It would **not** be a functional management card if uploaded. Never rename or wrap it as firmware for the installed H910P.

## Mandatory gates before a hardware-test BIN

1. Verify **installed** bootloader recovery behavior without assuming code strings guarantee a working rescue path; preferably use a recoverable spare card.
2. Map physical RAM and flash, sectors, loader protection and application allocation; the Dell header's RAM address is not a physical-memory-capacity result.
3. Implement and capture native CPU clock, UART, NIC, watchdog and UPS serial electrical protocol handling; demonstrate known-good measurements without affecting UPS power.
4. Implement and fuzz the actual device-resident HTTP/JSON transport and authentication. The loopback development server is **not** the card's firmware web server.
5. Implement and verify output control and configuration persistence on a test UPS with loads disconnected. Preserve all current management features and the current UI design.
6. Prove signed updates, flash sector boundaries, interrupted installation recovery and successful rollback before live test.

**Hardware test status: BLOCKED, not an implementation-complete release.** This is the maximum tested **offline** firmware substrate, not an excuse to flash an unknown image on a live UPS.

## Freestanding ARM7 division runtime

Calibrating native HID measurements requires general 32-bit unsigned
division. ARM7TDMI has no hardware division instruction, and the research
ELF previously lacked Clang's `__aeabi_uidiv` helper. A bounded
32-iteration restoring unsigned long division helper is now linked without
libc or recursion. Its host regression compares against native C division
for all edge combinations and 10,000 deterministic pseudorandom operands.
The helper's zero-divisor fallback is not an application success condition;
all callers validate divisors before invoking it. ARM7 linking and full
sanitizer tests remain mandatory.
