# v0.6 independent firmware — test gates and hardware safety

**Status: ARM7 native core, native JSON services, host-loopback management UI and offline-linked research ELF. NOT flashable.**

The v0.5.10 `main` firmware and existing production `source/` UI are unchanged. The `v06` directory includes freestanding native C firmware foundations, ARM7 research ELF, host-only API simulator and a new loopback management console under `web/`. The browser assets are served **only by the development host server** until a qualified on-card HTTP/static-asset and authentication stack is integrated. The simulator remains read-only.

## What is actually implemented

| Area | Implemented and tested | Does not yet exist |
| --- | --- | --- |
| Independent JSON core | Freestanding C99; typed routes; strict bounded serialization; 302 OEM schema definitions in 19 JSON pages | Native hardware service and persistent config |
| Read-only data provider | `ou_provider_accept` validates version, verified framing flag, reading ranges, monotonic age <=5 sec, and clears state on failure | Actual H910P NMC↔UPS electrical transport driver |
| HTTP/UI | Loopback-only host adapter serves a full 15-destination read-only management console with exact JSON API routes, security headers, and denied mutations | Device-resident UI asset serving, TLS and authenticated sessions |
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
curl --fail --silent --show-error http://127.0.0.1:8760/
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

## v0.6 development console — implemented scope and remaining requirements

The host simulator now serves `/`, `/index.html`, `/app.css` and
`/app.js` through an **explicit static allowlist**, using the same
loopback-only process and same-origin JSON API. The files are never
selected by arbitrary path input, and security headers require same-origin
external scripts/styles. There are no credential uploads or automatic
firmware updates. The UI includes the **15 established management destinations**
with accessible keyboard navigation, 360px mobile behavior, unavailable/
loading/error states, and visible simulator indicators. Implemented sections
use actual JSON values for device identity, UPS status, measurements,
outlet states, events and the complete 302-item configuration schema.
Missing mutation endpoints are intentionally disabled, *not counted as
implemented production management operations*. Read-only frontend
integration is the scope of this host validation pass, not the end state.

**Remaining:** Serve these resources from the native board runtime under an
authenticated HTTPS management service, implement and qualify every required
management write/control feature (including authorization, persistence,
recovery, UPS power operations, scheduling, notifications, SNMP and update
rollback), and test on actual H910P hardware. The existing production UI
identity and requirements remain authoritative; no production UI files or
update/release mechanisms were replaced.

## Event monitoring, statistics, filtering and export

Native-only `GET /api/v1/events/stats` includes retained severity and
event-code totals, the oldest/newest event IDs and boot generation.
Native event pages carry the latest ID as a stable pagination token.
The host development UI checks all pages against that token and
retries once when events are arriving concurrently; it never silently
exports a torn snapshot. All severity/search filters and CSV/JSON exports
operate on the complete 32-record retained window. The System Log
receives the same cross-page treatment, with service-specific code
filtering.

The existing offline validation workflow runs Node source parsing
and a dependency-free DOM/API behavior test in addition to the C,
Python, sanitizer and ARM7 gates. This is **not a substitute for**
automated real-browser visual and keyboard acceptance or on-card
signed firmware, authenticated control, and verified storage.

## Native measurement-history acceptance

The read-only monitoring history is a 64-entry, RAM-only structure. It
accepts only previously validated native samples, aggregates on a
60-second cadence, records AC/low-battery edges immediately, and
annotates gaps after telemetry loss. No physical or simulated UPS is
contacted by the tests. Historical reads remain available with
`live:false` during a physical-link outage, and the simulator
explicitly does not provide genuine historical records.

Native API integration tests assert 8-page retrieval, strict page
validation, zero-record behavior, write-method denial, and rollback
of current telemetry without destroying historical evidence. The
host UI provides an accessible table, a battery/load bar visualization,
CSV/JSON export and coherent cross-page retrieval. Node-based DOM
behavior tests cover full history, interrupted reads and unavailable
sources; the original ARM7, sanitizer and research-only flash gates
remain mandatory. This is not a board-installed web UI.

## Typed configuration model and management draft verification

The C host suite exercises every one of the nine modeled keys,
schema-metadata parity, rejection of hidden/control/credential fields,
noncanonical IPv4, invalid subnet and cross-subnet gateway, missing
static-network dependencies, canonical serialization, duplicate-key
rejection, corrupt decode without state changes and 2-slot NOR power-loss
recovery. Failure of the external authorization gate or flash geometry
must produce **zero writes**. ARM7 big-endian compilation and sanitizers
remain required.

The HTTP suite verifies the new read-only staging metadata, absence of
current values and rejection of POST/PUT/PATCH/DELETE. The Node DOM
suite tests local form validation, blocking invalid exports, exact field
content in the review-only JSON, and retention of unrelated
management navigation. No environment deployment or hardware writes
are performed by these tests.

## Network activation transaction acceptance

The new `ou_config_activation` state machine is a pure, freestanding
ARM7-compatible model for confirmed network-profile changes. It refuses
to start without explicitly bound and separately qualified
authorization, network-apply/restore/probe adapters, a validated
committed baseline and a fully write-qualified independent CONFIG
journal. No unchecked flags from HTTP can invoke the controller.
Every candidate is checked against the nine-field typed configuration
model; stale journal generations, no-op requests and concurrent
operations are rejected.

A new profile is applied provisionally and cannot be persisted until
an authorized reconnect succeeds within its bounded confirmation
window. Outages, hardware errors, operator cancellation and timeouts
restore the old network profile. Interrupted commits preserve the
prior record; a post-marker failure is examined carefully and
repaired when safe. Failure to prove restoration yields a fatal state
requiring independent recovery, not a false success. Simulated NOR,
network, connectivity, authorization and recovery tests are part of
the normal host and sanitized CI suite. The module is cross-compiled
and linked into the non-flashable ARM7 research image.

**Not implemented:** a qualified H910P network adapter, live
authentication, HTTPS session, independent recovery channel, actual
configuration flash partition, and device-resident confirmation UI.
The existing read-only simulator remains read-only, without hidden
power/network operations.

## Authentication core and honest UI qualification

The independent auth engine is compiled and linked into the ARM7
research ELF but has no executable live login path. It requires
an explicitly qualified TLS transport, password-verifier provider
and secure entropy source: all must be present before
`ou_auth_init` can enable session operations. Every login checks
encrypted transport and a bounded role; tokens and CSRF secrets
come from an external CSPRNG and all-zero/duplicate tokens fail.
A four-session limit, monotonic session expiry, role gates,
CSRF/replay rejection, logout, revoke-all, and global lockout
are tested under host, sanitizer, and ARM7 build gates.

The real configuration activation implementation remains inactive;
its new session permission adapter only allows
`OU_AUTH_CONFIG_WRITE` operations with an operator-or-admin
authenticated session and strictly increasing request counter.
Host `GET /api/v1/security/capabilities` and Access Control
UI report no live authentication credentials, TLS, entropy
or write permission; host HTTP writes remain 405.

See `docs/authentication-sessions.md` for the full threat boundary.
Do not interpret offline tests as a real device login.

## Seven-day weekly schedule acceptance

Native `ou_weekly_schedule` preserves the 7×2 on/off OEM schema,
half-hour time granularity, disabled entries, and deterministic
next-occurrence planning. Its exact 64-byte versioned record
is decoded atomically and can only commit to a separately qualified
schedule journal with an explicit authorization callback.
Tests cover weekday/week wrap, boundary/midnight cases, invalid
and same-time entries, noncanonical records, interrupted writes,
invalid storage gates and previous-slot fallback.

The development UI exposes only a local review editor and JSON export.
Host HTTP and Node DOM tests assert seven days, independent on/off
selectors, invalid-time blocking, no live schedule data, no
power-operation routes, and that navigation preserves the local draft.
The ARM7 compile/link and sanitizers remain mandatory. Hardware
RTC/timezone, output-control authorization, persistent execution
and scheduler de-duplication are **not complete**.

## Notification queue and event-driven delivery

`ou_notify_engine` is a freestanding, fixed-memory multi-transport
routing and delivery model. It is integrated with native event
capture and native-only queue statistics, not with unqualified
physical network output. Tests exercise typed event fan-out,
source-event replay suppression, cooldown, backoff, bounded queue
overflow, terminal failure, authorization denial, reset behavior,
and refusal to transmit through unqualified sinks. Host GET/HEAD
tests verify read-only routing capabilities while POST/PUT/PATCH/
DELETE remain 405. The Email/SNMP browser rule editor is tested
for eight-rule limits, duplicate routes and review-only exports.

The engine's sender interface explicitly requires separately
qualified transport, destination store and authenticated transport
security (TLS or SNMPv3 USM). No physical SMTP, webhook, SNMP agent
or trap sender is available. See `docs/notification-routing.md`.
