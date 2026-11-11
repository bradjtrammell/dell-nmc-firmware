# Dell H910P 0.5.11-beta.3 — development engineering checkpoint

Branch: dev. Parent release baseline: v0.5.10 on main. This checkpoint is not promoted to main.

## Implemented

- XML response validation rejects HTML and malformed XML after one bounded retry. No configuration or power write is retried.
- System and Environment configuration reads are combined without suppressing valid System settings when Environment is unavailable.
- Power actions require a live card descriptor, successful live configuration read, observed outlet state and explicit typed confirmation.
- The four native email recipient slots remain 0–3. Monita, Discord and ntfy destinations are independent of these slots.
- Each relay destination can select power-loss/restoration, battery-low/recovered and connection-loss/restoration alerts.
- The relay applies 20% low-battery trigger and 25% recovery hysteresis, preserves old destination defaults, and has no arbitrary four-slot or thousand-destination cap.
- Release/Beta firmware discovery remains on dev. The release-channel selector does not make flashing transactional or independently verify release signatures.

## Local verification

- Final BIN: 1,516,994 bytes, below original Dell 1,517,029 bytes.
- SHA-256: d3350fe46cc9022f92c8d3a65af747c488dfe7d2d4071d065e1f54ed346f7fa6.
- Browser navigation, configuration, notification destination manager, native resource structure, and Python relay event tests passed.
- A full earlier compressor suite timed out. Local HTTP browser navigation was blocked, so actual session refresh on the NMC remains unverified.

## Not represented as complete

Native on-card HTTPS webhook delivery, a native server-side session, complete OEM parity, live UPS power-control testing, full hardware recovery/rollback and a verified automatic GitHub installer remain unresolved. Do not use this beta on a production-critical UPS. Before flashing, confirm the exact hash, a working recovery procedure and the OEM FLASH-write success message.
