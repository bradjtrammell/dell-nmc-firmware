# H910P native UPS shutdown thresholds and three-outlet policy

The independent native firmware now contains a **validated UPS/three-outlet shutdown policy model**, a bounded read-only threshold assessment, a dedicated transactional journal record, a native advisory API and the matching full Shutdown Parameters review editor. This work follows the exact types/ranges in the existing 302-field OEM capability inventory; it does not replace or remove any original management options.

## Main UPS settings

The primary UPS policy validates the original `Shutdown.Outlet[1]` group:

| Setting | Supported values |
|---|---|
| Shutdown duration | 120–9,999 seconds |
| Timed shutdown selected | Boolean |
| Main shutdown timer | 0–5,999,940 seconds, 60-second steps |
| Remaining runtime limit | 0–99,999 seconds |
| Battery-capacity shutdown threshold | 0–100% |
| Battery charge required before restart | 0–100% |
| Forced reboot | Boolean; **never executed** by the assessment engine |

The model does not assume those values are actually configured on the live card. In its independent advisory calculations, zero for a threshold means **no threshold trigger configured**; this interpretation does not authorize or alter physical UPS semantics until verified hardware integration establishes the exact original behavior.

## Managed outlets 2 and 3

Each outlet independently validates duration (120–9,999 seconds), shutdown timer (0–99,999 seconds), startup timer (0–65,535 seconds) and remaining battery-capacity threshold (0–100%). Outlet names and direct power-control commands remain represented by the separate original schema; no new control write is enabled here.

## Read-only verified-data assessment

`ou_shutdown_assess` only accepts a previously verified, non-stale native telemetry snapshot with valid battery and load values. It independently identifies utility loss, low runtime, low battery charge, per-outlet low-charge thresholds and restart-capacity readiness. It recommends a **shutdown-agent warning**, not UPS shutdown, when a configured low-runtime or low-charge threshold is reached. The output's `power_commands_enabled` flag is always false and no physical command is sent. A simulated, stale or unqualified reading cannot produce an advisory.

## Canonical policy journal and safe persistence

`src/shutdown_policy.c` and `include/ou_shutdown_policy.h` implement the versioned, fixed 64-byte `O6SD` record. The decoder rejects unknown version, reserved bytes, invalid bounds and inconsistent data without partially overwriting a previously loaded policy. Checkpoint requires an independent positive authorization callback and the existing journal's separately verified geometry, dedicated partition and writes-enabled gate. A CRC-protected dual-slot journal preserves the last valid record on interrupted writes. The dedicated SHUTDOWN partition must not overlap the bootloader, configuration, event, schedule or firmware regions; no physical partition has been allocated.

## Native API and management UI

`GET /api/v1/shutdown/schema` describes the exact original ranges and reports write/active-policy/output-control availability as false. `GET /api/v1/shutdown/advisory` provides the trusted policy's assessment flags only after **both** the device-side policy and telemetry have been qualified; otherwise 503 is returned. It never reports a physical shutdown as executed.

The Shutdown Parameters page now includes every primary and secondary-outlet field covered by this model, with accessible labels, cross-field validation, disabled/invalid states and a review-only JSON export marked `applied:false`, `installable:false`, and `physicalPowerControl:false`. The existing shutdown-agent coordination and warning controls remain on the same page; **no management feature was removed**.

## Verification

C tests cover bounds, 60-second granularity, separate outlet timers, power-command refusal, utility/battery/runtime thresholds, verified-versus-simulated input, canonical record integrity and interrupted NOR writes. Native service, HTTP and browser tests check the policy capability endpoint, authenticated binding boundary, verified-only advisory output, offline withdrawal, all three outlet editing groups and review-only export. ARM7 freestanding build and AddressSanitizer/UBSan remain mandatory gates.

**Remaining:** authenticated live policy editing and safe apply, physical UPS output protocol, protected persistence partition, verified event-to-agent-warning policy, watchdog/recovery controls, schedule execution, and device/hardware acceptance. None is excluded from project scope or claimed complete by this work.
