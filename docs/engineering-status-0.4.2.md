Dell H910P Custom NMC UI — v0.4.2 (2026-11-09)

Modified firmware: H910P_Custom_UI_v0.4.2.bin
SHA256: db0cede9c360b71a36dc68425970b8119005f170b3d9cfb7bda4eba2e7515757
Size: 1507635 bytes
Vendor image: 1517029 bytes
Delta: -9394 bytes
Advertised NMC firmware: 01.18.0005 (unchanged)

Critical fixes from v0.4.1:
- Generated /index.htm loads all 15 asset chunks plus bootstrap, not bootstrap only.
- Corrected tests use the actual generated script sequence, not fake preloads.
- Exact-optimal LZSS2 encoder decreases size below vendor factory image.

Test levels:
18 firmware regression tests passed
4 optimized encoder tests passed
4 native script entry and MIME checks passed
10 dashboard/browser categories passed
8 configuration/control browser categories passed
Embedded JavaScript and authored JS syntax checks passed
HTTP server integration: SKIPPED due to restricted Chromium navigation
Hardware upload, boot and live NMC XML behavior: operator test pending

The previous v0.4.1 NMC firmware upload produced HTTP 303 and an application-level
'ERROR : HTTP transfert failure'. Exact cause has not been proven.

Build locally: python3 build_native.py (requires C++17 g++ and vendor firmware ZIP in originals/)
See README.md and CHANGELOG.md for complete build and verification details.

## Repository provenance

Experimental 0.4.2 source is committed on dev. Public repository deliberately excludes the proprietary Dell firmware image, original vendor archive, generated BIN and non-public NMC configuration. The local build consumes the owner-supplied vendor archive.

Observed v0.4.1 HTTP 303 / `ERROR : HTTP transfert failure` did not establish root cause. Version 0.4.2 corrects a *separate, demonstrated* bootstrap bug and eliminates the image-size overage; hardware upload acceptance remains for operator verification.
