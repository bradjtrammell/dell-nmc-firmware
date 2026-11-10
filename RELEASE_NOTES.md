# Dell H910P v0.5.10 — Release Baseline

**Firmware:** `H910P_Custom_UI_v0.5.10.bin`  
**Base Dell firmware:** 01.18.0005  
**Size:** 1,517,026 bytes  
**SHA-256:** `e3adea74aad966c15e87f9fdb7407bb9d9d810e20158da54bad639466f70b8bb`

v0.5.10 is the project release baseline. The custom interface runs on H910P hardware, but remains experimental and is not recommended for production UPS deployments. The changes below passed offline firmware and browser tests; live H910P acceptance of all management workflows is still ongoing.

## Changes

- Restores the administrator's same-tab sign-in after a refresh by revalidating the card's existing HTTP Basic credentials.
- Serializes NMC XML operations, retries eligible transient reads once, and retains the last valid monitoring sample through a single polling interruption.
- Preserves the 15 management destinations, four Dell email-recipient slots (0–3), additional Monita/Discord/ntfy destinations, separate SMTP modal, and optional webhook relay.
- Maintains the original firmware size limit and validates the embedded browser assets and checksum.

## Security and operational notes

Tab-scoped credential storage is not a secure server-side session. HTTP Basic credentials are exposed over unencrypted HTTP and should only be used on a trusted management segment or VPN.

A FLASH-write success response is required after manual installation, and device behavior must be checked after reboot. Avoid repeated retries after an HTTP 401 upload rejection. Automatic update installation/rollback, native HTTPS webhook delivery, and full on-hardware control parity remain under development.

The corresponding source code and tests are in the `main` branch. Future work proceeds on `dev` until promoted.
