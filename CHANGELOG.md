# Change log

## 0.5.0 — 2026-11-09 — next-generation native management surfaces

- Added a bundled white, alpha-transparent Dell brand mark in the upper-right header, derived from the owner's supplied artwork; no CDN.
- Replaced raw editable key/value grids with semantic setting groups, automatic descriptor discovery, local search, explanatory copy, and modal editor/confirmation flow.
- Enforced two-step submission, existing card input constraints, independent get_object XML readback, masked credentials and honest unverified-secret statuses.
- Added filtered/severity-classified searchable event history, native CSV export and clearer loading/error states.
- Added email notification management surfacing and Monita/ntfy/Discord integration status panels, explicitly not claiming direct push delivery.
- Added firmware maintenance view with separate UI/base versions and current manual HTTP/1.0 POST process; native GitHub asset updater remains deferred pending Flash0 verification.
- Repurposed original `html/synoptic/dhtml.js` and normalized unused prior Help chunks, reducing firmware to 1,512,695 bytes (4,334 under OEM).
- Built exact same BIN using either OEM firmware or SHA-256-verified v0.4.2, with byte-for-byte comparison.
- Updated browser tests for automatic discovery, modal workflow, invalid value rejection, sensitive field masking, no-write cancellation, event filtering and firmwares states.
- This is an offline-tested candidate, NOT a live hardware validated v0.5.0 release. The original synoptic frontend is no longer functional, and full legacy UI parity is not claimed.


## 0.4.2 — 2026-11-09 — entry loading and image-size remediation

- **Fixed real startup failure in v0.4.1:** OEM `/index.htm` now loads all 15 content chunks followed by the bootstrap script; no test harness preloading or missing assets.
- Fixed browser tests to derive the script order from the actual packed `/index.htm` instead of inserting a synthetic preload.
- Added a four-test native script/MIME/resource linkage suite that reads the rebuilt BIN.
- Added an optimal C++17 LZSS2 compressor with eight-state exact flag-byte accounting, producing the same decompressed ARM application as the original greedy encoder.
- Reduced the custom BIN to **1,507,635 bytes**, **9,394 bytes smaller than factory**; no OEM graphics or executable code were removed.
- Added deterministic/edge-case codec tests; original Python firmware, browser, and configuration suites remain passing.
- Did not alter the advertised Dell firmware version. Previous HTTP-transfer failure remains under investigation until device retest.


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

## 0.2.1 (development)
- Preserve user focus and scroll during automatic telemetry refresh; avoid unnecessary view replacements.
- Parse XML values supplied as text, VALUE children, or attributes; reject nameless XML objects.
- Add live read-only card identity retrieval from /product.xml, filtering credential-like fields.
- Add a tenth browser test covering identity retrieval and sensitive-value suppression.
- Add a separate actual-HTTP loading test that transparently SKIPs under restricted Chromium loopback policy.
- Compact RomPager entry HTML to accommodate the additional assets.

## 0.3.0 (development)
- Implement Dell documented configuration_descriptor.xml reader and schema-driven settings forms.
- Confirm and validate individual RW changes, use multipart Configuration.xml to Forms/set_obj_2, verify readback through get_object.xml.
- Exclude hidden/undocumented power commands from generic editor.
- Add six simulated-browser regression categories covering descriptor parsing, secret handling, invalid input, XML upload and readback.
- Rewrite firmware validation report to distinguish new RW operations from legacy-only power controls.

## 0.4.0 (native UI development)
- Replace multi-script index with a single bootstrap resource that loads chunk resources sequentially, eliminating the index page cap.
- Add five static Help resource slots without changing NMC executable code or RomPager callback tables.
- Add dedicated native output power on, restart, and power off actions based strictly on advertised Control.Outlet[n] commands.
- Require exact typed confirmations for power commands and explicitly distinguish submission from confirmed outlet state.
- Add canceled-command and confirmed-command browser tests; 8 configuration test categories now pass.

## 0.4.1 (desktop polish + navigation correctness)
- Fix selected sidebar navigation state after switching away from configuration pages.
- Show four UPS live instruments above the protected output controls.
- Extend browser tests to assert selected navigation and capture network/power-control desktop screenshots.
