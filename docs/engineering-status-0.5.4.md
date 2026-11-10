# H910P v0.5.4 — firmware build verification

## Deliverable

- Image: `H910P_Custom_UI_v0.5.4.bin`
- Bytes: 1516350
- SHA-256: `c1afea30020234844b5db04b46185cad18c33abfe051dfc81184deb1be42a6be`
- Base image: Dell 01.18.0005, factory image length 1,517,029 bytes
- Flash size delta: **-679 bytes** versus factory
- Branch target for maintained source: `dev`

## Implemented

- Carried forward v0.5.3 runtime management UI and existing Dell handlers.
- Added same-origin safe XML diagnostic with status, MIME, length, root, 24/302
  group/item count where supplied, and parse classification. Diagnostic requests
  use GET and do not send settings writes or emit secret XML content.
- Full-height sign-in background style stabilization.
- Minified only comment and indentation whitespace for ROM script transport.
- Packed 48,160 bytes of base64 UI payload into known RomPager resource slots.
- Verified NET+OS checksum and embedded UI round-trip.

## Verified here

- Firmware unittest suite: **22 passed**.
- Native entry tests: **4 passed**.
- Optimal compressor: **4 passed**.
- Browser UI: **13 PASS**, no uncaught JS errors.
- Configuration and updater browser suite: **16 PASS**, no uncaught JS errors.
- Bootstrap interception: PASS, 20 sequential resource requests.
- Diagnostics browser: **3 PASS**, including non-disclosure and no writes.
- Exact firmware bytes: **1,516,350**, binary SHA above.
- Local HTTP browser navigation: **SKIPPED**, browser sandbox restricted
  loopback; not a live card acceptance test.

## Device-level gate

Original Dell upload handler remains in original callback data. Physical
firmware flash/write/boot, live descriptor responses, actual email delivery,
UPS power commands, and automatic updater rollback have NOT been verified on
the physical device. The GitHub API's `/releases/latest` was 404 when checked.
Do not infer that the browser firmware uploader can negotiate HTTP/1.0.

See `docs/firmware-deployment-http-post.md` for the actual known-method
command and hardware success check.
