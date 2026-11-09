# H910P native UI 0.5.0 — engineering verification

**2026-11-09** | GitHub `dev` | **Firmware candidate, not flashed on hardware yet**.

## Confirmed, inherited from v0.4.2

Physical Dell H910P accepted full firmware via direct HTTP/1.0 multipart POST, reported `SUCCESS : Firmware was successfully written in FLASH`, rebooted, and served the new dashboard at `/index.htm`. v0.4.2 is the last hardware-proven baseline.

## Newly implemented in v0.5.0

1. Functional management route surfaces: automatic NMC configuration descriptor loading for network, email, schedule, shutdown, clients, access, SNMP, and clock settings; semantic categorization and local instant search; secret masking; modal editor and explicit separate review/commit steps; readback confirmation for non-secret values.
2. Searchable event history with severity heuristic, refresh, CSV download.
3. Notification overview: SMTP is the only built-in transport; ntfy/Monita/Discord are visible as planned integrations, not functional senders.
4. Firmware maintenance status view that distinguishes custom UI from original firmware and labels GitHub asset updater as unavailable.
5. Owner-supplied exact Dell logo traced to a local self-contained transparent white vector shown in the upper-right.
6. Additional app bytes packed by repurposing OEM `/html/synoptic/dhtml.js` and v0.4.2 Help slots. **OEM synoptic UI loses its JavaScript and is no longer usable as a fallback**. OEM firmware-upload handler and original management backend remain unchanged; no executable code/data pointer changes.
7. Deterministic builder accepts either Dell's original ZIP or **verified v0.4.2 BIN** as baseline, no Dell CDN access required.

## Exact firmware candidate

- v0.5.0 BIN size: **1,512,695 bytes**; original OEM 1,517,029; saved **4,334 bytes**.
- SHA-256: `b9d3a17c8d26267d6d9e4c678097c63911d73c219ae8d250e972e31465eca77b`.
- Original header, callback dispatch/pointers, embedded application length and CRC validated offline; exact same BIN from both source baselines.
- Native script requests: 8 total: 7 data chunks followed by one bootstrap. The packed RomPager entry page contains all script tags in order, matching source-generated assets.
- Firmware test categories: 18 Python tests passed; 4 LZSS tests passed; 4 native-entry tests passed; browser tests passed all navigation, telemetry, configuration, modal confirmation, simulated power, masked secret, event filter, error and retry checks. JS syntax checks passed.

## Unverified and incomplete

- **Real hardware:** v0.5.0 boot, script serving, legacy handler coexistence, live XML schema, write operations and power controls.
- **Full feature parity:** scheduled UPS actions, shutdown agent management, event subscription filters, notification delivery and SNMP trap handling remain controlled by OEM APIs and may require further reverse engineering; pages display only actual NMC-exposed controls and do not invent settings.
- **Updater:** persistent FLASH0 static asset store, signature validation, GitHub transport, atomic switching, rollback; none implemented.
- **Firmware recovery:** original POST uploader is present; ability to recover when the modern UI is corrupted but card boots is not specifically demonstrated for v0.5.0. Network flash cannot recover from a firmware image that never boots.

## Next live hardware validation

Upload only after confirming the exact candidate SHA-256, using the known HTTP/1.0 multipart POST shape. Check explicit FLASH success on `/app_upload.htm`, wait until reboot completes, and verify `/index.htm`, dashboard live data, navigation, original firmware upload page, and read-only config descriptors. Avoid power-changing tests until protocol mapping is verified.
