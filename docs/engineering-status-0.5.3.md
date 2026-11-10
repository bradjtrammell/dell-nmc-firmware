# H910P v0.5.3 — audit remediation, configuration recovery, and maintenance UI

**Date:** 2026-11-09. **Branch:** `dev`. **Status:** experimental firmware candidate, tested offline only; do not claim that it has booted on the physical H910P.

## Scope addressed in this engineering pass

- Parsed the OEM's built-in `SYSTEM_CONFIG` XML from the same vendor or verified v0.4.2 firmware image used by the build. Generated a **field-only**, non-secret catalog covering its RW configuration capabilities, instead of embedding a generic settings demo or inventing fields. The original schema has 24 GROUPs and 302 ITEMs; the packaged catalog includes only supported writable items. No passwords, defaults, or secret values are embedded.
- Replaced the brittle `configuration_descriptor.xml` all-or-nothing path with a recoverable adapter. When the endpoint returns invalid XML, an HTML error, or fails, the UI loads the original firmware field catalog, labels it as a fallback, independently fetches live `System.*` values, and **blocks edits if live configuration values are unavailable**. The fallback is a display/catalog recovery, not proof of available live writes.
- Populated the System page with editable device-profile settings (contact/location/etc.) in addition to product identity and diagnostics. Reused modal review, validation, OEM `Configuration.xml` POST and readback. Product identification now displays a minimally useful device/address/firmware fallback when `/product.xml` cannot provide recognizable leaf values.
- Kept 15 navigation destinations and existing power-control capability gating. The catalog never enables power commands by itself; power controls require a readable live descriptor and typed confirmation.
- Added **real browser-initiated GitHub latest-release checks** (`api.github.com`, user-initiated only), matched published BIN assets, and presented download links. Added a **local BIN upload UI** that checks the NET+OS header, payload length and NET+OS CRC before permitting a same-origin upload to the original Dell firmware handler. Redirect results are explicitly labeled *unconfirmed* until the NMC reports successful FLASH write. Credentials and BIN files are not sent to GitHub.
- Reworked the ROM resource bootstrap to fetch JavaScript chunks sequentially at startup with just one script reference in `/index.htm`. Added extra reclaimed slots from obsolete Dell synoptic graphics, menu, and legacy stylesheet. This destroys additional old Dell presentation resources; **the original firmware's executable code, dynamic callbacks, `/app_upload.htm` handler and firmware upload status remain unmodified**.
- Extended regression tests to simulate the real malformed-descriptor failure seen in the 1,160-frame recording, block edits without device values, check GitHub release discovery, inspect actual BIN CRC before browser form POST, and test the ROM bootstrap execution order without network access.

## Critical distinction: updater status

This pass **does not implement ADR-0001's separate on-card Flash0 UI-asset store or transactional asset writes**. No firmware-side API exists yet to persist arbitrary UI bundles independently of a firmware reflash. A Chromium browser cannot force HTTP/1.0 for POST; the original card previously rejected browser uploads and succeeded with HTTP/1.0 `curl`. The new browser firmware uploader is a **genuine UI control** with validation and handling but its success is **not verified on hardware**. GitHub release lookup works in mock tests; it will not offer a BIN until the public GitHub repository publishes a compatible firmware release asset. Do not label this automatic no-reflash OTA as complete.

## Test and recovery boundaries

- Required before upgrading actual hardware: confirm `/app_upload.htm` remains accessible, retain working v0.5.2 BIN and factory recovery archive, perform a controlled v0.5.3 POST and validate a genuine FLASH success message plus login and 15 screens.
- Full native boot, HTTP upload behavior, live `System.*` response, real SMTP delivery, power switching, persistent storage, updater rollback, and actual GitHub release publication remain **unverified**.
- The old Dell menu, visual stylesheet, old default pages, and synoptic UI have been intentionally retired to meet the image-size ceiling. `/app_upload.htm` static and dynamic parts remain unchanged. This preserves the upload mechanism, not a second complete legacy UI.

## Exact final build

Firmware BIN: **1,516,340 bytes**, SHA-256 `3a5d34ddf7fa9699b044e4e4d9f96562f1b80e1167db35f0ce352d0013bfe422`. Rebuilt from factory ZIP and known-good v0.4.2 with byte-identical output.
