# ADR-0002 — v0.6.x independent H910P firmware replacement

**Status:** Accepted architectural target (November 11, 2026)
**Decision owner:** Project maintainer
**Scope:** All v0.6.x development on `dev`. v0.5.10 on `main` is the locked stable release.

## Decision

v0.6.x is **not** another web-interface overlay onto Dell 01.18.0005. It is the next major engineering generation of this project: a standalone, maintainable H910P firmware application and management stack owned by this project, replacing Dell's application firmware, original RomPager UI and XML CGI management services.

No new functionality is to depend on OEM `/get_object.xml`, `/configuration_descriptor.xml`, `/product.xml`, `/Forms/set_obj_2`, or Dell UI asset slots after the native implementation reaches parity. These can exist only in a clearly separated legacy compatibility/test harness while transitioning. v0.5.10 is retained unchanged as the current stable rollback/recovery reference.

“Independent firmware” does **not** imply an independently replaceable mask-ROM boot stage or that the exact H910P bootloader can be rewritten. The required deliverable is a complete independent NMC application image and native services; any immutable SoC ROM or necessary retained bootloader component must be identified and documented. If the card's boot architecture cannot safely support this, the project must say so rather than call an OEM patch fully custom firmware.

## Release structure

| Branch | Version | Product |
| --- | --- | --- |
| `main` | v0.5.10 | Stable Dell-derived embedded UI firmware, no feature changes until promotion |
| `dev` | v0.6.x development | Independent firmware implementation and beta qualification |

A newly compiled v0.6 binary is required before assigning a v0.6 runtime version. Existing `source/` and `build_native.py` remain historical v0.5 compatibility assets until explicit retirement; changing the displayed version does not constitute a new firmware.

No additional active Git branches. Work is contained on `dev`, with `main` promoted only after independently reviewed release qualification.

## Native stack boundaries

1. **Boot and board support:** exact processor architecture/stepping, executable load address, interrupts, clocks, RAM/flash geometry, persistent configuration partitions, watchdog, NIC, serial UART and bootloader entry/recovery behavior. Do not guess processor/RTOS, flash layout or erase boundaries.
2. **UPS hardware protocol:** reverse-map read-only telemetry first; then explicit output control, alarms, battery tests, timed shutdown and outlet sequencing with device-side validation. Reject unsupported operations. No incidental power-affecting writes while probing.
3. **Native HTTP service and API:** bounded parsing, typed JSON v1, stable error schema, explicit schema version and capability advertisement, CORS disabled by default, CSRF-safe session behavior, request IDs, strict authorization and rate limits. No legacy XML dependency.
4. **Web UI:** directly served, fully offline-capable, preserves the approved desktop-first product identity; accessible tables, task-specific dialogs, responsive design and real status of asynchronous operations. Only use genuine device-advertised capabilities.
5. **Persistent configuration:** versioned, validated, atomic if supported by physical flash; checksums, write-budget aware storage and migration or reset strategies. Secrets never exposed by read APIs and never stored in browser session storage.
6. **Network and security:** DHCP/static IPv4, supported IPv6, DNS/NTP, HTTPS with secure credential handling, local user/session policy, SNMP v1/v3 where required, notification egress, and isolation on a trusted management network by default.
7. **Notifications:** native SMTP configuration with four supported email slots 0–3 including default email; one table for configured channels; any number of independent Monita, Discord and ntfy destinations within documented memory/storage capacity (no artificial four-slot limit). Native background delivery, retries, event subscriptions and independent test actions. No external relay required for the final v0.6 feature.
8. **Updates:** firmware-level verification of signed release manifests and image signatures, approved main/dev channel selection, compatibility check, inactive staging/readback when hardware permits, boot health and rollback or a documented equivalent physical recovery procedure. Never advertise automatic rollback without a proven mechanism.
9. **Diagnostics:** structured logs, bounded history, export, errors with durable cause and firmware identity; console access is an independent rescue/control plane if proven by hardware.

## Native JSON API v1

JSON is an HTTP response format, not a periodically rewritten disk file. Live measurements are generated from in-memory state; persistent configuration is committed only after validated changes.

Required endpoint groups and representative methods, to implement only after the underlying native handlers exist:

- `GET /api/v1/capabilities`, `GET /api/v1/device`, `GET /api/v1/health`
- `GET /api/v1/ups/status`, `GET /api/v1/ups/measurements`, `GET /api/v1/ups/outlets`
- `POST /api/v1/ups/outlets/{id}/actions`, `GET/PUT /api/v1/ups/schedules`, `GET/PUT /api/v1/ups/shutdown`
- `GET /api/v1/events`, `GET /api/v1/events/export`, `POST /api/v1/events/clear`
- `GET/POST/PATCH/DELETE /api/v1/notifications/destinations`, `POST /api/v1/notifications/destinations/{id}/test`, `GET/PUT /api/v1/notifications/smtp`
- `GET/PUT /api/v1/network`, `GET/PUT /api/v1/system`, `GET/PUT /api/v1/snmp`, `GET/PUT /api/v1/time`
- `GET/PUT /api/v1/shutdown-clients`, `GET/PUT /api/v1/access`, `POST /api/v1/session`, `DELETE /api/v1/session`
- `GET /api/v1/updates`, `POST /api/v1/updates/verify`, `POST /api/v1/updates/install`, `GET /api/v1/updates/jobs/{id}`

These names are a **target API contract, not implemented H910P endpoints**. Never refer to them as live until executable hardware tests pass.

Expected response properties: finite payloads, schemaVersion, UTC timestamps and monotonic sequence numbers where relevant, explicit unit labels, null for unavailable readings, enum states rather than magic XML strings; consistent `application/json` errors. No power-changing action auto-retries. Idempotency protection for any deliberately retryable operations.

## Hardware research gates — before any flash

**Gate H0, read-only hardware identity.** Identify processor ISA and flash using verified markings, photographs or trusted datasheets and the known OEM firmware in offline analysis. Record voltage, flash type, chip select, reset and boot modes. The confirmed Settings port on `dockeradm` runs 9600 8N1 through an FTDI FT232R at `/dev/ttyUSB0`; it provides a maintenance menu, **not an established shell or bootloader console**. Use only passive serial observation and non-destructive menu options. Do not write settings during discovery.

**Gate H1, independent boot feasibility.** Recover reset vectors, relocation, memory map, original image validation rules and firmware uploader safety conditions. Establish an off-device emulator/simulator or development board where feasible; never assume a legacy NET+OS image packer can build a new firmware.

**Gate H2, read-only UPS communication.** Verify protocol framing/checksum, model discovery, event notifications and safe read operations on a controlled test unit. Record actual hardware traces without committing credentials, serial numbers, or proprietary Dell binary material.

**Gate H3, native boot on sacrificial/recoverable hardware.** Independently prove memory initialization, timer, watchdog, NIC/serial, a persistent recovery path and clean reboot. Hard-stop if a failed flash cannot reliably be recovered with available tools.

**Gate H4, functional parity.** Verify all original 24 configuration groups and 302 OEM items against dedicated, supported workflows; explicitly record unsupported model capabilities. Test output control only with UPS outputs disconnected from production loads.

**Gate H5, release qualification.** Cold boots, repeated power loss and interrupted-write recovery, long soak with network stress, session/login security, malformed JSON rejection, device event and notification egress, persistent settings, branch updates, and signed image validation. Pass only measured acceptance tests.

## Milestones

- **v0.6.0-dev:** board/boot/flash map and API schema with a runnable host-side hardware simulator. Not flashable; make no unsupported runtime claims.
- **v0.6.0-alpha:** independently bootable candidate with read-only telemetry, native HTTP/JSON, native UI and recovery validation.
- **v0.6.0-beta:** full operation and configuration parity, notification delivery, security and safe update path on recoverable test hardware.
- **v0.6.0 stable:** hardware-qualified image with documented recovery and no critical operational regression.

The first true flashable release name is determined by test evidence, not the roadmap calendar.

## Explicitly out of scope for v0.5.x

- No retrofitting native v0.6 API claims onto Dell's existing XML endpoints.
- No force-flashing an over-size image or bypassing OEM integrity checks.
- No speculative serial boot commands or destructive reset attempts on the connected UPS.
- No treating source commits, release discovery or a successful HTTP redirect as firmware installation.
- No automatic `main` promotion while hardware acceptance is incomplete.

## Decision rationale

v0.5.x reached the ceiling of an OEM resource-patcher: tiny fixed RomPager asset slots, a factory-sized binary, unreliable legacy XML handlers, native HTTP Basic challenges and limited extensibility. A genuinely maintainable firmware requires owning the executable, device service layer, persistence and API—not merely converting XML response strings into JSON in the browser.

Hardware discovery may reveal practical or legal limits that require changing the implementation approach. Those findings must be recorded before attempting a native firmware flash.
