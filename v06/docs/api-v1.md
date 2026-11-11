# Native JSON API v1 — implemented host simulator contract

**Status:** Portable C response engine, tested on Linux and compiled as big-endian ARM7 objects. This is **not** a live physical firmware API. Every response contains `"simulated": true` and `"source": "simulator"` until a reviewed native board driver is attached.

## Implemented endpoints

| Route | Payload / purpose | Hardware interaction |
| --- | --- | --- |
| `GET /api/v1/health` | `status`, `hardware`, `uptimeSeconds` | None |
| `GET /api/v1/device` | Firmware identity and target platform | None |
| `GET /api/v1/capabilities` | Feature flags and OEM migration totals | None |
| `GET /api/v1/ups/status` | `powerSource`, `batteryPercent`, `estimatedRuntimeSeconds`, `loadPercent` | None (test fixture) |
| `GET /api/v1/ups/measurements` | Input/output voltage objects in **decivolts**, `unit: "dV"` | None (test fixture) |
| `GET /api/v1/ups/outlets` | Three output descriptions with `controlEnabled: false` | None (test fixture) |
| `GET /api/v1/events` | Explicit development simulator events | None |
| `GET /api/v1/configuration/fields` | `total: 302`, `pageSize: 16`, `pages: 19`, `configurationWritable: false` | None |
| `GET /api/v1/configuration/fields/{page}` | 0–18; up to 16 metadata-only records with `name`, `group`, `access`, `type`, `unit`, `requiresRestart`, `hiddenInOEM` | None |

**HEAD** is supported for each route and returns the exact GET content length without a body. Unrecognized paths return `404`, including Dell's `.xml` and CGI routes. Methods other than GET/HEAD return `405`, including power-control and configuration-write attempts. The host HTTP adapter never writes to a UPS or its storage.

Every response uses `Content-Type: application/json; charset=utf-8`, with schema version, resource name, source, simulated flag and sequence. The C serializer uses bounded buffers and refuses to emit truncated JSON. Unknown resource errors use a structured `code` property.

## Configuration schema

The **exact 302 read/write/read-only/write-only field definitions** derive from the owner's sanitized legacy OEM contract. The generated `src/schema.c` contains only validated metadata: no defaults, hostnames, serial numbers, addresses, passwords, communities, or actual live configuration. This is an intentional migration inventory; it is **not** a configuration store and cannot be used to write settings.

The C engine exports **all 302 records** in 19 fixed-size JSON pages. An HTML search interface loads the metadata once and filters client-side without making any NMC requests. No XML parser or external network service participates in the simulator.

Regenerate from the repository root after validating an updated sanitized contract:

```sh
python3 v06/tools/generate_schema.py docs/v06-oem-capability-inventory.json --output v06/src/schema.c
```

Then run `make test` and `make arm-check` from `v06`. Do not generate schema from unsanitized XML or device values.

## Security boundaries

The host-only server binds `127.0.0.1` with no route to the live H910P. It serves only exact whitelisted static files, excludes CORS credentials, adds CSP/no-store/nosniff security headers, rejects oversized request headers, rejects ambiguous body framing, and drains bounded write bodies before responding. **No authentication or TLS is present**, therefore this simulator is limited to a local development machine.

The eventual native device firmware must introduce secure sessions, CSRF controls for allowed writes, TLS and cryptographically signed releases before connecting any real UPS actions. Treat this API implementation as a proven serializer/contract, not a finished security subsystem.

## Compatibility and future hardware provider

`ou_dispatch()` is freestanding and takes an explicit `ou_snapshot` as an argument. The current host adapter supplies `ou_simulated_snapshot()` and advertises no physical hardware connection. After the NS7520 board-support work is verified, a real provider must populate that structure from validated measurements and change capability flags **only after hardware acceptance**. The serializer and field-schema engine can be reused without dragging Dell's RomPager/ThreadX/XML services into the new firmware.
