# Change log

## 0.2.0 — 2026-11-08

- Replaced the simple prototype design with the desktop-first Dell Instrument Panel visual system.
- Added 15 navigation destinations aligned to the existing H910P management interface.
- Added SVG icons, radial gauges, animated power flow, refined electrical status, and event-history view.
- Preserved the original native power and configuration handlers; no UPS writes from the new UI.
- Reworked asset packaging into deterministic gzip resources with a modern-browser decompression loader; reduced overwritten legacy Help URLs from 12 to 9.
- Corrected focus handling and navigation state so background telemetry refreshes do not recreate the application shell.
- Added strict executable-byte allowlist, RomPager pointer verification, CRC and compressed asset round-trip validation.
- Added all-section and error-recovery browser tests with mocked read-only NMC responses.
- **Outstanding:** no live-device verification, firmware-image size increased by 12,213 bytes, nine Help pages are repurposed, and management forms remain legacy.
