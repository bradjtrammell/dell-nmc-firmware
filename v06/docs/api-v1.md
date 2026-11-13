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
| GET `/api/v1/ups/history` | read-only validated native measurement history, eight samples per page; retained data available during UPS outage |
| GET `/api/v1/ups/history/{0..7}` | history pages, stable latest-record token and exact decivolt units; no simulator history |
| GET `/api/v1/ups/outlets` | three outlet summaries; power controls disabled (503 if unavailable) |
| GET `/api/v1/events` | native typed event history, newest-first, independent of UPS telemetry (simulator retains its synthetic events) |
| GET `/api/v1/events/{0..3}` | native event pages, 8 records each, strict path validation and consistent `latestEventId` token |
| GET `/api/v1/events/stats` | native retained-history counts by severity and event code, current/oldest IDs and boot-generation snapshot |
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


## Event history archive / recovery (explicit checkpoint)

Native event records include an uptime-relative `monotonicMs` and
`bootGeneration`. The response includes `currentBootGeneration`, so
a timestamp recorded before a restart cannot be confused with the
current boot's uptime. The 32-event in-memory ring continues serving
GET/HEAD even if UPS data is unavailable.

The event archive codec serializes the complete bounded ring using a
versioned endian-stable 440-byte image. The existing NOR-friendly
two-slot journal supports CRC readback, staged writes, and atomic
commit-marker activation. The service now exposes explicit
`ou_native_service_events_restore` and
`ou_native_service_events_checkpoint` calls. Restoring a checkpoint
increments the boot epoch, restores ordered event IDs, and writes a
new service-start event without treating a previous online state as
proof the UPS is connected now.

**There is no configured physical event partition or automatic flash
writing.** Event history is still advertised with `persistent:false`
and `retention:"volatile-until-explicit-checkpoint"`. Checkpointing is
an explicit BSP operation requiring a separately verified EVENT storage
partition, NOR geometry, and write permission; it is never allowed
to reuse configuration, firmware, or bootloader sectors. Future
qualified hardware integration must implement an endurance-aware
checkpoint policy and test power failures.

## Browser event-management integration

The native event response provides `latestEventId` as a consistency
token. The read-only `/api/v1/events/stats` endpoint reports the 32-entry
ring's retained counts, overwritten count, current boot generation,
oldest/newest event IDs, severity totals and counters for each of the
eight typed event codes. Overwritten records are **not included** in
these totals. It returns independent of UPS telemetry availability, so a
link failure does not hide the incident history.

The v0.6 management console reads all retained event pages, rejects
duplicated IDs or inconsistent snapshot tokens, and retries once if
the ring changes mid-read. All visible search, severity filtering,
system-log filtering, pagination and CSV/JSON exports operate on that
**complete, consistent** snapshot. CSV strings are quoted and
formula-prefixed values escaped to avoid spreadsheet execution.
Exported JSON records retain boot-generation and timestamp context and
explicitly disclose source, simulation status, overwritten records and
storage-persistence limitations. No mutation/clear actions are exposed.

The host-only simulator continues offering its labeled sample event list
without a native event-stats endpoint. The development UI derives
temporary counts from those sample records; it never promotes them to
verified hardware events.

`node --test v06/tests/test_ui_events.cjs` is a dependency-free
browser-behavior harness run in the existing CI validation job. It checks
navigation, cross-page filtering, exports, error responses and snapshot
consistency. This verifies JavaScript behavior against a fake DOM/API,
**not** browser rendering, physical hardware, HTTPS, or durable flash writes.

## Native historical monitoring

A fixed 64-sample, volatile, verification-gated history buffer receives only
accepted `ou_telemetry_guard` native snapshots. It samples at a 60-second
interval and immediately captures AC-power transitions, low-battery threshold
crossings, and the first sample following a link loss. The next valid record
after disconnection carries `gapBefore:true` so plots never imply
uninterrupted monitoring. No raw UART frames, credentials, personal data or
unsafe commands are stored. Invalid and replayed reports cannot be recorded.

The native `GET /api/v1/ups/history[/0..7]` response contains
`schemaVersion:1`, `sampleSource:"verified-native-only"`, `live`, a
retention/capacity summary, `latestRecordSequence` (stable during
unsampled device polling) and `samples`. Each sample includes sequence,
uptime-relative monotonic milliseconds, battery and load percentages,
runtime seconds, input/output decivolts, AC state, and gap flag.
No wall-clock time is inferred; these values are uptime-relative.
The resource remains available during a telemetry outage, with
`live:false`. Simulator requests return `503 HISTORY_UNAVAILABLE`
rather than invented native history.

The management UI fetches all retained pages, checks
`latestRecordSequence`, count, and duplicate IDs for a coherent snapshot,
shows accessible rows and battery/load bars with reconnection markers, and
exports exact values as CSV or JSON. If a live sample has not yet
been physically qualified, the UI shows an explicit unavailable
message; it does not plot arbitrary simulator readings.
History is **RAM-only**, does not survive reboot, and is not claimed
as a persistent trend database.

Automated tests cover report verification, 60-second cadence, immediate
power/threshold edges, loss/recovery gap markers, ring overwrite, counter
wrap, offline HTTP read access, all eight pages, CSV/JSON download
behavior and concurrent-page consistency failures. They do not qualify
the physical UPS or enable flash writes.

## Configuration staging — read-only management contract

`GET /api/v1/configuration/staging` returns the nine non-secret
allowlisted field names, labels and types. All of
`configurationWritable`, `storageQualified`,
`authorizationAvailable`, and `activeValuesAvailable` are **false**.
No current configuration, network address, password or secret value is
returned. All HTTP mutations including this route still return 405.

The Network and System management destinations now expose a local-only
draft editor with per-field and cross-field validation. Users can export
an explicitly `applied:false`, `installable:false` review JSON artifact.
Nothing is POSTed to the card or written to storage, and an export is **not**
an installable configuration package. This preserves user work and
validates the form while the future HTTPS session, rollback-safe activation
and independently verified flash remain outstanding.

## Authentication and session capabilities (development integration)

`GET /api/v1/security/capabilities` is a strict read-only statement
of what the independently implemented C authentication core supports.
It returns `sessionEngineCompiled:true`, the four-session limit,
15-minute idle and eight-hour absolute timeouts, five-failure/60-second
lockout policy, and CSRF/replay requirements. It explicitly returns
`loginEnabled:false`, `authenticationAvailable:false`,
`tlsQualified:false`, `entropyQualified:false`,
`credentialStoreQualified:false`, and
`mutationsAuthorized:false`. It provides no password, token, stored
secret, or account list.

The Access Control page fetches this endpoint and shows actual
qualification status instead of an inoperative login screen. All
password- or mutation-related requests remain rejected with 405;
there is no live HTTP authentication endpoint or on-card session.

The new `ou_auth_config_authorize` adapter supports the existing
transactional configuration authorization callback by checking
operator permissions, independent CSRF and strictly increasing
request IDs. External qualified password KDF, hardware entropy,
HTTPS cookie transport, credential persistence and live H910P
integration are **still required**. See
`docs/authentication-sessions.md`.

## Weekly Schedule — native planner and local review

`GET /api/v1/schedule/schema` describes the verified existing
seven-day schedule schema: one ON and OFF time per weekday, disabled
`-1`, otherwise 1,800-second slots between midnight and 23:30.
It returns `weeklyPlannerCompiled:true` and **false** for
`scheduleWritable`, `activeScheduleAvailable`, `rtcQualified`,
`powerControlQualified`, and `persistent`. It contains no
invented active schedule or simulated outlet action.

The Weekly Schedule UI now exposes 14 interactive local selectors,
validates same-day ON/OFF conflicts, and exports a review-only JSON
file explicitly marked `applied:false` and
`installable:false`. There is no `POST`, physical power control
or HTTP schedule execution route. The native planner and dedicated
schedule journal are separately tested and are not a field-qualified
UPS driver. See `docs/weekly-schedule.md`.

## Native event-notification routing (read-only control plane)

The native service now forwards its validated typed events into
a fixed-memory delivery queue (24 jobs, eight configurable rule slots)
for email, webhook and SNMP trap destinations, using event ID/boot
generation de-duplication, per-rule cooldown and retry/backoff. The
engine only invokes an externally qualified authenticated outbound
sender; none is installed on the real H910P.

`GET /api/v1/notifications/policy` returns exact supported event
codes, transports and capacity limits, with `deliveryAvailable:false`,
`rulesWritable:false`, `transportQualified:false` and
`destinationsProvisioned:false`. The native-only
`GET /api/v1/notifications/status` returns queue and delivery
counters; the host simulator returns 503 rather than inventing
delivery. All HTTP methods other than GET/HEAD remain denied.

The Email Notification and SNMP management pages expose a local
rule review editor, eight-rule ceiling, duplicate detection,
cooldown limits and a JSON export marked
`applied:false`/`installable:false`. This is not actual alert
sending or recipient provisioning. See
`docs/notification-routing.md`.

## Custom GitHub release-check and native image verification

`GET /api/v1/firmware/update-policy` pins
`releaseProvider:"github"` and
`repository:"bradjtrammell/dell-nmc-firmware"`. It reports the
H910P target and running independent firmware version. The native
SHA-256 plus signed-manifest verification implementation is compiled,
but real signing keys, HTTPS device transport, flash geometry,
recovery and installation/rollback all report **false**.
All HTTP write/update/upload operations still return 405.

The Firmware Upload page can now request the public project's GitHub
Releases API directly through an explicit host Content Security
Policy allowlist for `https://api.github.com`; no token or
credentials are sent. It displays published tags, channel, date,
asset names, and an explicit unverified-for-installation status,
including zero releases and errors. The current API returned no
published releases. Nothing can be installed, updated or downgraded
from the browser. See `docs/github-updater.md`.

## Bounded read-only SNMP UPS-MIB agent (no network listener yet)

`GET /api/v1/snmp/capabilities` lists seven RFC1628 UPS-MIB OIDs for independently verified native readings and the compiled SNMPv2c GET/GETNEXT BER codec. Listener, ACL, community provisioning and SNMPv3 authPriv are explicitly unavailable. `ou_native_service_snmp` passes validated telemetry to a bounded datagram parser, which refuses SET and drops unauthorized requests. No UDP/161 adapter or power action is enabled. See `docs/snmp-monitoring.md`.

## Shutdown application control plane

`GET /api/v1/agents/capabilities` declares a native 12-client, 10–300-second heartbeat lease and 30–1,800-second warning coordination model. It denies live registration, mTLS, verified shutdown intent and physical power operations. `GET /api/v1/agents/status` provides native registry counts and active warning status; the loopback simulator returns 503 rather than inventing registered applications. Both the Notified Applications and Shutdown Parameters pages have local review-only editors. See `docs/shutdown-agents.md`.

## Independent time sync and UTC status

`GET /api/v1/time/capabilities` describes the compiled NTPv4 wire validator, maximum 5-second response time and 1-hour maximum sample age; live network, NTS, RTC, source, timezone and DST qualifications remain false. `GET /api/v1/time/status` returns UTC Unix seconds, millisecond precision, RTT and estimated uncertainty only after a verified native time sample, otherwise 503 `TIME_NOT_SYNCHRONIZED`. The Time UI prepares a review-only server/timezone/DST configuration with no actual set-time operation. See `docs/time-sync.md`.
