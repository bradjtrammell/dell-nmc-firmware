# H910P independent native JSON API — implementation contract

The C99 response code has no Dell XML/CGI dependencies. The **only** HTTP listener currently exists in a loopback-only POSIX host simulator. The NMC has no new native API installed.

## Current read-only endpoints

| Method / path | What is returned |
| --- | --- |
| GET `/api/v1/health` | source, simulated flag, uptime, data availability, hardware connection status |
| GET `/api/v1/device` | device/firmware identity and target platform |
| GET `/api/v1/capabilities` | independently gated telemetry, configuration and control capabilities |
| GET `/api/v1/ups/status` | battery, load, AC, runtime (503 if unavailable) |
| GET `/api/v1/ups/measurements` | input/output voltage with `dV` unit (503 if unavailable) |
| GET `/api/v1/ups/outlets` | three outlet summaries; power controls disabled (503 if unavailable) |
| GET `/api/v1/events` | native typed event history, newest-first, independent of UPS telemetry (simulator retains its synthetic events) |
| GET `/api/v1/events/{0..3}` | native event pages, 8 records each, strict path validation |
| GET `/api/v1/configuration/fields` | metadata count 302 and 19 page descriptors |
| GET `/api/v1/configuration/fields/{0..18}` | exact name, type, group, access, restart/hidden flags, unit; no values |

The native service now records its own service-start, telemetry online/offline,
rejected-report, utility failure/restoration and low-battery/recovered
transitions. Events have monotonic sequence IDs and uptime-relative
`monotonicMs` timestamps, severity, stable event codes and safe fixed
messages. The native RAM ring retains 32 entries and reports overwritten
counts, 8 per page. Events remain readable when UPS status returns 503,
and repeated identical state updates do not spam the log. The history is
**volatile**, advertises `persistent:false` and is not yet an implemented
persistent event journal; it is not SNMP/email alert delivery.

HEAD is supported. All mutations return 405, including UPS output commands, configuration changes and firmware upload. Unrecognized paths return 404; XML and Dell CGI paths are absent. Responses use bounded JSON buffers, schema version 1, and an explicit source marker. The server advertises simulator mode and does not accept remote connections.

## Driver-data contract

`ou_provider_accept` checks driver version, a verification bit, frame freshness <=5000 ms, sequence and field ranges. The `verified` bit is **a future driver precondition** requiring actual physical framing, checksum and identity verification; it is not proof such a driver has been implemented. On any rejection, the code clears the old value and returns a 503-capable unavailable snapshot, rather than recycling readings.

No independently verified UPS driver is currently linked. This JSON API **must not be represented as live firmware telemetry** until real drivers exist and have passed hardware acceptance tests.

## OEM mapping support

The optional `tools/verify_oem_parity.py` utility parses a previously captured private `ups.xml` file **offline only**, validates nine numeric UPS measurement and outlet-state fields, enforces ranges, and reports typed readings without network or credential fields. It is not part of the ARM target. Its limited purpose is to establish numeric baseline conversions (for example, voltage in volts to decivolts) before native UPS transport exists.

See [`native-test-gates.md`](native-test-gates.md) for hardware/flash restrictions.
