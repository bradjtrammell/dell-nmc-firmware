# Dell H910P NMC — Instrument Panel

A desktop-first, offline-capable web management interface **embedded directly into Dell H910P NMC firmware** (Dell base `01.18.0005`). No CDN, external management server, cloud service, or Docker is needed to browse the UI. All new frontend code is here in source form; the vendor NET+OS firmware remains proprietary.

## v0.5.1 — Notification manager, Dell branding and authenticated sign-in

**Hardware status:** v0.5.0 was reported installed by the owner; v0.5.1 is built and browser-tested **offline only**, and must not be represented as tested on a real H910P until installation and read-only verification are reported.

This update builds directly on the embedded Dell 01.18.0005 application and retains its original executable instructions and RomPager callback pointers. No CDN or separate server is required to open the management interface.

- Corrected the **white transparent Dell logo to the upper-left** of the management sidebar.
- Added a centered username/password screen. The entered credentials are verified by a **real, protected Dell Basic-auth endpoint** (`GET /app_upload.htm`) before opening the management UI. Credentials are kept only in browser memory; **Lock** clears the current UI state and requires a fresh sign-in. This is a browser-side sign-in experience using existing Dell HTTP authentication, **not** a new server authorization layer or a guarantee that the underlying plain-HTTP transport is encrypted. Continue to restrict device network access.
- Refined the dashboard with quick access to events, protected output status, notifications, maintenance, and a live feed of recorded UPS events alongside gauges and power flow.
- Replaced raw `System.Email[n]` key/value rows with **four real recipient slots**. The exact OEM descriptor advertises `Email.Email[0]` through `Email.Email[3]`, each with `System.Email[n].Recipient`, `System.Email[n].Selected`, and per-recipient event selections. The new modal editor stages individual user-approved changes and independently reads each back from the original NMC API before reporting success. A partial failure is reported instead of being silently treated as success.
- Showed outgoing SMTP server and sender settings separately from notification recipients. The four-slot cap is derived from the actual descriptor, not hard-coded fake recipients.
- Replaced “not installed” service gates with **setup guidance** for ntfy inbound SMTP, Monita inbound email, and Discord SMTP-to-webhook bridges. Users can assign available email slots to these receiving addresses. **No direct native ntfy, Monita, Discord, webhook, or push transport has been added.** The remote service/gateway must already be configured to accept mail.
- Rewrote technical release/update copy for device owners. UI-only auto-update from GitHub is **not implemented**, pending measured Flash0 storage, static file serving, authenticated package verification, and recoverable installation.
- Reclaimed 47,762 bytes of static presentation strings from redundant original synoptic and legacy help pages, preserving callback pointers and executable bytes. The stock Dell synoptic UI and selected stock Help pages are **not functional** in this image. The native firmware-upload handler remains intact; the new UI still does **not** implement every legacy control workflow.

**v0.5.1 BIN:** 1,515,874 bytes, 1,155 bytes under the OEM 1,517,029-byte image.

**SHA-256:** `3dbe3c27952cb5b56bdc956c05d6e9b4b64c20125afe196e7baff451f536c2bb`

Validated offline: exact binary decompression and CRC, embedded asset reconstruction, unchanged callback pointers and non-allowlisted executable bytes; 18 firmware regressions, 4 compressor regressions, 4 native-entry checks, the 15-screen browser suite, sign-in/lock checks, and notification/form workflow browser checks with simulated Dell device responses. Rebuilt from both verified OEM ZIP and verified v0.4.2 BIN with **byte-identical outputs**.

**Not yet hardware verified:** sign-in behavior on the physical card, live configuration XML compatibility, recipient delivery, sequential changes/partial-failure behavior, full UI feature parity and v0.5.1 image acceptance/boot. The build is experimental and should not be distributed as a fully qualified production replacement.

The [manual HTTP/1.0 POST deployment method](docs/firmware-deployment-http-post.md) remains the recommended upload method. It must report success on the card’s status page before treating a flash as complete.

---

## v0.5.0 — Operational UI expansion (firmware candidate)

The original H910P was successfully flashed with **custom UI v0.4.2** via HTTP/1.0 multipart POST on November 9, 2026. The card rebooted and served `/index.htm` directly. **v0.5.0 has passed offline firmware and browser tests but has NOT been tested on real hardware.** Keep the proven v0.4.2 BIN as a recovery reference.

### New in v0.5.0

- Reworked the native configuration pages into categorized, searchable management sections with descriptive titles, live read-only values, clear supported-versus-unavailable states, and keyboard-friendly modal editors.
- Clicking **Edit** opens a dialog, validates on review, then requires a second explicit **Apply to NMC** action. Upload uses the existing same-origin `Configuration.xml` multipart endpoint and checks device readback before declaring success. Secrets remain masked; credential writes are reported as submitted rather than device-readback verified.
- Load configuration automatically when entering device management sections rather than leaving pages empty until a manual fetch.
- Added searchable event records, severity filters, refresh control and CSV export; severity is inferred from message text and is not a device-provided classification.
- Notification page provides existing SMTP settings and clearly labels ntfy, Monita and Discord delivery *as not installed*, with their potential event-ingest paths. Direct webhooks, new native event handlers and SMTP mail sending tests are NOT implemented.
- Firmware page shows the installed **UI version 0.5.0** distinct from Dell base `01.18.0005`, the proven manual HTTP/1.0 POST update path, and an honest GitHub/UI-only updater status (**not yet implemented**).
- User-supplied Dell logo was converted to white with transparency and vectorized; its SVG is packaged into the local UI and displayed in the upper-right. No external assets, fonts or dependencies.
- Increased available UI storage by repurposing original `/html/synoptic/dhtml.js` and the already-overwritten Help-resource slots. The old Dell *synoptic/diagram frontend* will no longer work; original UPS backend and firmware upload handler remain in place. This is part of the approved single-UI transition; **do not use v0.5.0 if full legacy Dell interface fallback is required**.
- The 1,512,695-byte BIN is 4,334 bytes smaller than the original Dell image (1,517,029 bytes). ARM executable, boot header, resource descriptors, and native callbacks are unchanged; only allowlisted static UI bytes and MIME metadata are patched.
- Deterministic builds from either the owner's original Dell ZIP **or an exact checksum-matching custom v0.4.2 BIN** produce byte-identical v0.5.0 firmware.

### Not yet complete

- The physical card has NOT run v0.5.0. All 15 routes render in browser mocks, but availability of actual editable NMC settings depends on the live configuration descriptor; many specialized workflows (weekly schedule, shutdown clients, event recipient selection, SNMP traps, user management) still require hardware API discovery and verification. No false claim of complete parity.
- The independent Flash0 UI asset store, GitHub-only UI updater, automatic rollback, custom notification dispatcher, HTTP webhooks and direct ntfy/Monita/Discord delivery are **not yet implemented**.
- Native power control remains based on the descriptor's observed `Control.Outlet[n]` commands with exact typed confirmation and no end-to-end device-power test. Treat power-command submission as unconfirmed until output state is observed on hardware.
- Firmware restoration through `/Forms/app_upload_1` requires the NMC still boots, is reachable, and serves the native uploader. This is not a guarantee of recovery from an unbootable image.

## Build from the exact proven v0.4.2 BIN

The **simplest local build**, requiring no Dell website and no OEM ZIP, is:

```bash
python3 build_native.py --from-v042 "$HOME/Downloads/H910P_Custom_UI_v0.4.2.bin"
```

The builder refuses any v0.4.2 BIN whose SHA-256 does not equal:

`db0cede9c360b71a36dc68425970b8119005f170b3d9cfb7bda4eba2e7515757`

Alternatively, put the original `NMC_Firmware_01_18_0005_X2JYN.zip` inside `originals/` and run `python3 build_native.py`.

**Toolchain:** Python 3.11+ standard library, C++17 compiler `g++` (for the repository's lossless optimal LZSS2 compressor). No network access needed once the sources and baseline image are present.

Output: `build/H910P_01.18.0005_INSTRUMENT_PANEL_EXPERIMENTAL.bin`

Expected **v0.5.0** SHA-256:

`b9d3a17c8d26267d6d9e4c678097c63911d73c219ae8d250e972e31465eca77b`

Exact size: **1,512,695 bytes**.

We reproduced identical output bytes using both input methods. The firmware build checks the CRC, decompression round trip, metadata pointers, allowable binary changes, embedded UI reconstruction, and factory BIN-size ceiling.

## Tests

```bash
python3 tools/test_validation.py          # Needs the two OEM ZIP test fixtures
python3 tools/test_optimal_encoder.py
python3 test_native_entry.py
python3 test_browser.py                   # Needs Playwright + Chromium
python3 test_config_browser.py
```

The 18 firmware tests, 4 compressor tests, 4 native-entry tests and two browser suites passed on the authoring machine using mocked UPS/NMC responses; no real-hardware v0.5.0 claim is implied. Run `node --check source/*.js` individually for JS syntax checks.

## Firmware deployment

The **proven device upload method** is a single HTTP/1.0 multipart POST (not a browser HTTP/1.1 upload), and positive firmware-write confirmation on `/app_upload.htm` after the redirect. Do not follow the 302/303 automatically; those statuses do not establish flash success. See [`docs/firmware-deployment-http-post.md`](docs/firmware-deployment-http-post.md). The GitHub updater is tracked separately in ADR-0001 and remains unavailable.

## Design identity

Precise, industrial, assured — a control-room instrument panel with a light `#f2f6f7` content canvas, Dell blue `#007db8`, charcoal `#18272f` sidebar, green `#087a53` system statuses, angular 4–5 px corners and Segoe UI/Arial. Sparse borders and strong typography maintain hierarchy, with short animations and reduced-motion support. Every screen must work offline at 360px and larger. No framework runtime/CDN is loaded; Tailwind is compiled ahead of time.

## Licensing and scope

`originals/`, `build/`, binary firmware and proprietary Dell components are excluded from the public Git repository. Users supply their own lawful firmware image for local patching. Dell and the Dell logo are trademarks of Dell Inc.; inclusion of owner-supplied artwork does not imply Dell endorsement. Code in this repository remains distinct from Dell's proprietary NET+OS application.
