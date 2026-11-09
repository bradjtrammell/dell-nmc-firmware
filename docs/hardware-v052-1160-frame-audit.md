# Dell H910P v0.5.2 — exhaustive video audit

**Input:** `Screencast from 2026-11-09 13-43-53.webm`
**Duration:** 69.714 seconds, **resolution:** 1906×877, **decoded frames:** 1,160 / 1,160.
**Method:** sequential full decode (no skipped frames), frame-by-frame per-image pixel comparison and page-state classification, 49 ordered contact sheets (24 frames/sheet), full-resolution inspection of all observed UI destinations and key error states. Frame-level index covers frames 0–1159. Browser animations and cursors may cause consecutive frames with the same page label; they were still indexed.

**Scope:** visual behavior and matching v0.5.2 source code. No live NMC request/response capture or firmware upload was performed.

## Complete screen visitation timeline

| Frame range | Recording time | Screen |
|---|---|---|
| 0000–0028 | 0.00–5.63s | Login/Overlay/Unknown |
| 0029–0043 | 5.68–6.15s | Overview |
| 0044–0135 | 6.18–9.24s | Login/Overlay/Unknown |
| 0136–0230 | 9.28–12.43s | Overview |
| 0231–0258 | 12.46–16.34s | Login/Overlay/Unknown |
| 0259–0264 | 16.39–16.56s | Overview |
| 0265–0297 | 16.60–17.67s | Login/Overlay/Unknown |
| 0298–0332 | 17.70–18.86s | Overview |
| 0333–0378 | 18.89–21.34s | Events |
| 0379–0600 | 21.43–28.89s | Overview |
| 0601–0689 | 28.92–31.91s | Measurements |
| 0690–0703 | 31.94–32.94s | Outputs |
| 0704–0719 | 33.48–34.90s | Events |
| 0720–0737 | 35.13–37.86s | Ups Control |
| 0738–0800 | 38.63–43.87s | Weekly Schedule |
| 0801–0826 | 43.90–45.87s | Shutdown Parameters |
| 0827–0896 | 45.96–51.04s | Notification Center |
| 0897–0942 | 51.18–53.57s | Network |
| 0943–0975 | 53.71–55.45s | System |
| 0976–1013 | 55.73–57.64s | Shutdown Clients |
| 1014–1034 | 57.85–58.88s | Access Control |
| 1035–1051 | 59.80–60.89s | Snmp |
| 1052–1159 | 61.04–69.71s | Firmware Updates |

**Note:** `login/overlay/unknown` occurs when the recording shows the sign-in view or third-party browser overlays obscuring the UI, and does not indicate unclassified missing video frames.

## Page-by-page results

| Route | Frames present | Observed result |
|---|---:|---|
| Sign in / browser overlay | 182 | Login/sign-out/relogin; browser password-manager popups; successful dashboard sign-in observed |
| Overview | 373 | Live 54–57% load, 100% battery, 120 V, ~1,011–1,022 W; power path displayed |
| Measurements | 89 | Input/output readings present; temperature unavailable, battery trend empty |
| Output groups | 14 | Three output groups shown; group 1 UNREPORTED, groups 2 and 3 ON |
| Event history | 62 | 0 recent events and No events returned; cannot verify completeness of event log |
| UPS control | 18 | Control capability not loaded; no native power-action controls available |
| Weekly schedule | 63 | Invalid configuration descriptor, 0 editable settings |
| Shutdown parameters | 26 | Invalid configuration descriptor, 0 editable settings |
| Notification center | 70 | Invalid configuration descriptor; no email recipient manager visible |
| Network | 46 | Invalid configuration descriptor; zero editable settings |
| System | 33 | No readable identification fields in product XML; UPS XML telemetry still returns 420 objects |
| Shutdown clients | 38 | Invalid configuration descriptor; zero editable settings |
| Access control | 21 | Invalid configuration descriptor; zero editable settings |
| SNMP | 17 | Invalid configuration descriptor; 0 editable settings |
| Firmware / updater | 108 | v0.5.2 shown; update UI explicitly says automatic updates unavailable and GitHub button opens releases only |

## Defects and actionable engineering evidence

### 1. P0: Configuration parser / backend adapter
**Time:** 38.6–61s. **Observed:** Weekly schedule, shutdown parameters, notification center, network, clients, access control, SNMP and time/NTP routes depend on /configuration_descriptor.xml; several visibly show Invalid configuration descriptor and zero settings.
**Code/evidence:** Confirmed on screen. The original OEM application embeds a well-formed SYSTEM_CONFIG descriptor: 24 groups, 302 items. The current UI source throws Invalid configuration descriptor specifically on DOMParser parsererror. The exact HTTP response bytes from the running card are not recorded in this video; cannot yet name the transport/encoding root cause.

### 2. P0: Device settings coverage
**Time:** 35–61s. **Observed:** UPS control cannot load capability, and many management pages show zero settings or only advanced OEM links.
**Code/evidence:** Confirmed. Source ui-config.js only discovers fields after shared descriptor fetch succeeds. It also filters fields by selected groups and RW access.

### 3. P0: GitHub updater not implemented
**Time:** 61–69.7s. **Observed:** Firmware page says "Automatic updates are being developed" and the GitHub button only views releases; no check/install or rollback.
**Code/evidence:** Confirmed by screen and source. Must not describe it as operational. UI-only updates require proved writable+servable Flash0 asset store and transactional activation; firmware POST is currently separate.

### 4. P1: Card identification XML incompatible
**Time:** 53.7–55.45s. **Observed:** System → Management card displays "No readable identification fields in product XML".
**Code/evidence:** Confirmed on screen; source expects leaf nodes with values from /product.xml. Its actual response schema remains unrecorded.

### 5. P1: Power outlet state incomplete
**Time:** 31.94–32.94s. **Observed:** Output group 1 reports UNREPORTED, while groups 2 and 3 report ON; separate UPS control page cannot activate controls.
**Code/evidence:** Confirmed. Investigate mapping between UPS.OutletSystem fields and firmware-declared outlet objects. Do not enable switching actions based on guessed field names.

### 6. P1: Event history interpretation
**Time:** 18.89–21.34s and 33.48–34.90s. **Observed:** The page displays 0 recent events and "No events returned".
**Code/evidence:** Confirmed visual state; not proof the actual log has no events. Source pulls /logevent.csv and slices lines; raw log payload and encoding need validation.

### 7. P2: Measurement history / missing temperature
**Time:** 28.92–31.91s. **Observed:** Measurements show real volts, load, power and battery, but temperature is -- and battery trend says it will start with a later reading.
**Code/evidence:** Confirmed. A missing sensor should be marked unsupported rather than falsely read as zero. Trend persistence and data continuity can be improved.

### 8. P2: Sign-in and browser overlays
**Time:** 0–17.70s. **Observed:** Recording includes sign-in, Lock/return, and third-party browser password-manager warnings / save prompts over the UI.
**Code/evidence:** Login works in this recording. Chrome/LastPass overlays are browser UI, not H910P UI defects; hide them when conducting visual acceptance tests.

### 9. P2: UI routes / default recovery
**Time:** 18–69.7s. **Observed:** The 14 distinct authenticated page destinations in the video navigate, but feature content is not at parity and many links still lead to old Dell pages.
**Code/evidence:** Navigation is demonstrated; functionality is not. Legacy fallback links may be broken as resources were repurposed. **Time & NTP appears in the sidebar but is not actually opened in the video; no claim about that page’s behavior is supported.**

## Source-level check: OEM descriptor

The exact original Dell firmware ZIP contains a literal XML document starting with `<?xml version='1.0' encoding='UTF-8'?>` and a `SYSTEM_CONFIG` root. An independent offline XML parser accepts its 35,146 bytes and enumerates **24 GROUP nodes and 302 ITEM nodes**. `ui-config.js` calls `DOMParser().parseFromString(response, 'application/xml')` on the live `/configuration_descriptor.xml` response and throws `Invalid configuration descriptor` when a `parsererror` element exists. The video does **not** contain the raw response; possibilities such as truncation, non-XML gateway error or incorrect encoding remain unverified.

Other verified original firmware endpoints include `/get_object.xml`, `/product.xml` and `/config.xml`. The first is returning live telemetry in this recording. `/configuration_descriptor.xml` exists as a native CGI endpoint string even though it is not one of the 159 normal RomPager static resource descriptors.

## Required acceptance gates before calling v0.5.x production-ready

- Capture the real descriptor endpoint response **through the app itself** in a safe limited diagnostic, noting HTTP status, content type, response length, XML parse location and sanitized prefix; do not infer behavior from browser mocks.
- Reconcile the parsed OEM schema with the 302 firmware-declared fields; explicitly map each current navigation page to fields and writable controls; test against exact device responses.
- Validate `product.xml` response shape; show device identification in System.
- Verify complete NMC event log retrieval and parsing; distinguish truly empty from failed/unsupported.
- Verify each outlet status read and protect power controls with capability checks, typed confirmations and actual state readback.
- Implement four firmware-declared email recipient slots using working backend data and readback; Monita/ntfy via proven SMTP routing where supported, not pretend-native HTTP.
- Implement a real signed, persistent GitHub UI-asset installer only after Flash0 storage/serving and rollback are demonstrated. Until then label it unavailable. Firmware POST updates remain separate and confirmed via NMC success message.
- Test authenticated navigation, all 15 management destinations, read/write and interrupted-update recovery on a real card, not just against simulator fixtures.

## Files in accompanying archive

- `frame_index_all_1160.csv`: one record for **every** decoded frame, containing timestamp, UI classification and visual-change metrics.
- `contact_sheets/`: all 49 chronological contact sheets, each with 24 sequential labeled frames except the final sheet (8 frames). None are sampled.
- `keyframes/`: 72 full-resolution frames selected for close reading, *in addition to* the complete contact sheets.
- `scene_endpoints/`: full-resolution captures at the end of every classified screen-state segment.
- `index.html`: offline frame-by-frame viewer with a slider and keyboard navigation; loads every frame cell from the complete contact sheets.
- `per-frame.json`: per-frame timestamp and pixel-change index.

## Verification

**Executed:** full sequential decode; all 1,160 frames indexed; ordered frame coverage asserted; 23 continuous segments with no index gaps; OEM descriptor parsed using independent XML library; firmware source inspected. **Not executed:** physical NMC integrations or repairs. No new firmware BIN is claimed in this audit.