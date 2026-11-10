# Dell H910P v0.5.10 — locked experimental release baseline

**Status:** User-designated baseline (November 10, 2026); experimental, offline tested, not production-qualified. Earlier custom firmware has operated on a physical H910P. End-to-end hardware acceptance of the v0.5.10 session and XML changes is not independently verified.

## Fixed

1. **Refresh previously logged the administrator out:** after a successful formal login, the app now stores a tab-scoped Basic credential reference and revalidates it on refresh. It does not use a persistent cross-tab or server session; Lock clears the credential immediately.
2. **Periodic malformed XML:** reads to the NMC no longer overlap, XML endpoints receive one bounded read-only retry on malformed content/timeouts, and the dashboard preserves the last verified reading through one failed polling cycle. Sustained faults are shown rather than suppressed.
3. **Build size:** optimized non-runtime JavaScript formatting, removed unused functions, and preserved the same Dell SVG as an encoded data URI. The complete firmware remains below the vendor factory BIN size.

## Preserved

All 15 navigation destinations, original management callbacks, the notification table, email slots 0-3, unlimited additional Monita/Discord/ntfy destinations through the optional relay, SMTP modal and existing signed checksums.

## Known limitations

- The device's HTTP Basic scheme requires a reversible credential to be held in sessionStorage for tab refresh continuity. This is not safe over an untrusted network or a replacement for native HTTPS and device server-side sessions.
- Real H910P CGI/XML behavior and the one-refresh restoration have not been observed after installing this image. The tests operate against mocks and final BIN bytes.
- Local Chromium navigation is blocked in the build environment, so an actual reload through a local HTTP mock could not run; the state machine was tested in Node and browser login was tested through simulated fetch.
- Automatic GitHub firmware installation, rollback and native on-card webhook delivery remain incomplete. Firmware upload HTTP 401 occurred in earlier releases; a successful write is not guaranteed.

## Deployment

Use the separately provided SHA-256-pinned deployment block. Always confirm `SUCCESS : Firmware was successfully written in FLASH` before browsing `/default.htm`. Never interpret HTTP 302 alone as an installed firmware update. Avoid repeated 401 upload attempts.
## Release asset integrity

- Firmware file: `H910P_Custom_UI_v0.5.10.bin`
- Size: **1,517,026 bytes**
- SHA-256: `e3adea74aad966c15e87f9fdb7407bb9d9d810e20158da54bad639466f70b8bb`
- Base firmware: Dell 01.18.0005

The release asset must be the actual BIN, not a browser demo or source ZIP.
