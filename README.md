# Dell H910P NMC — Instrument Panel

A desktop-first embedded UI experiment targeting Dell H910P Network Management Card firmware **01.18.0005**. The interface is hosted **on the NMC**, not a separate server or container, and provides monitoring via Dell's documented XML protocol.

> **NOT FLASH-APPROVED.** The experimental BIN has not been tested on hardware. It increases the vendor image size, and the original NMC Help pages used for asset storage are no longer available. Do not flash the production UPS card until a spare-card boot/recovery test succeeds and full functionality is proven.

## Design identity

- **Personality:** precise, assured, industrial; inspired by a professional power-distribution control-room console.
- **Palette:** light content `#f2f6f7`, white `#ffffff`, Dell blue `#007db8`, functional green `#087a53`, charcoal sidebar `#18272f`, warning `#a86814`, danger `#b93e30`.
- **Typography:** Segoe UI / Arial; numerical readouts use tabular numerals.
- **Layout:** dense desktop console with persistent grouped navigation, operational status bar, power-flow schematic, and instrument gauges; 850px tablet collapse and 520px narrow viewport refinement.
- **Navigation:** frame-free single-page interface with URL fragments; existing Dell management remains accessible.
- **Geometry:** rectilinear precision, 4–5px corners, sharp dividers, minimal elevation.
- **Iconography:** local inline stroke-based SVG icons; no external CDN or images.
- **Motion:** 180–220ms navigation and gauge transitions, flowing electrical-path indicators; reduced-motion preference honored.
- **Signature:** animated live power path, 2 radial load/battery instruments, and four primary power metrics.

## Implemented

- Modern dashboard, read-only measurements, output group status, and event CSV viewer.
- 15 navigable management destinations based on the user's current Dell UI recording.
- Browser-local trend samples, status indicators, recoverable polling failures, and explicit unreported values.
- Fixed 15-second polling interval with in-flight request suppression, 8-second timeout, and pause when the browser tab is hidden.
- Tailwind CSS v4.1.10 precompiled utility subset plus semantic CSS and vanilla JavaScript compressed into nine existing RomPager static resource slots; `DecompressionStream('gzip')` requires a current browser.
- Original `/default.htm` UI and Dell firmware management/control handlers are not modified; management links open those native pages and still use the old frame-based UI.
- Offline firmware safety invariants: intact compiled callback tables, unchanged ROM object pointers, original application memory length, valid image checksum, exact asset round trip.

## Known blockers — **do not deploy to production**

1. The NMC has not been shown to accept, boot, or serve the modified firmware image. No hardware rollback or bootloader recovery has been verified.
2. The rebuilt image is **12,213 bytes larger** than Dell's original; available flash headroom is unknown.
3. The nine original Help URLs used for compressed UI assets are overwritten. Legacy Help navigation is therefore degraded.
4. Modern UI is read-only for power and configuration actions. Full native control workflows are **not** yet implemented. The legacy interface remains necessary.
5. Live XML response compatibility and authentication against the actual card are not yet verified; tests use simulated protocol data.
6. Browser decompression must be tested in the expected host/browser environment and with the card's actual HTTP security headers.

## Local offline build and tests

Python 3.11+, standard library, and the official Dell 01.18.0005 firmware ZIP are needed for the firmware build. **Do not commit Dell firmware or any device credentials.**

Copy the original Dell ZIP into `originals/NMC_Firmware_01_18_0005_X2JYN.zip`, then:

```bash
python3 build_native.py
```

This generates `build/H910P_01.18.0005_INSTRUMENT_PANEL_EXPERIMENTAL.bin` and `build/VALIDATION.json`. The build never contacts or flashes the NMC. The checked-in `source/tailwind-compiled.css` is produced with Tailwind CSS v4.1.10 using `source/tailwind-build.cjs` at design/build time; firmware builds do not require Node or external assets.

For regression tests, also supply the unmodified Dell `01.16.0001` ZIP at `originals/NMC_Firmware_01_16_0001_YJ6JM.zip`:

```bash
python3 -m unittest discover -s tools -p 'test_*.py' -v
```

Browser QA needs Python Playwright plus an installed Chromium at `/usr/bin/chromium`:

```bash
python3 test_browser.py
```

The test reconstructs **the bytes embedded in the modified firmware**, injects simulated Dell XML and CSV responses, verifies all 15 destinations, and checks the absence of power/configuration writes. It does **not** establish hardware compatibility.

## Repository layout

- `source/` — human-readable custom UI modules and design styles.
- `build_native.py` — offline resource packing, checksum/round-trip rebuilding, strict allowlist verification.
- `tools/h910p_rebuild.py` — NET+OS image parser and LZSS2 encoder/decoder.
- `tools/rompager_map.py` — embedded RomPager resource inspection.
- `tools/test_validation.py` — firmware regression tests.
- `tools/ui_patch_proof.py` — isolated one-byte CSS patch proof.
- `test_browser.py` — embedded-resource browser tests using simulated UPS data.
- `originals/` — **local only; excluded from Git**.
- `build/` — generated BIN and QA outputs; excluded from Git.

## Hardware and source reference

Target: Dell H910P rev A06, original firmware 01.18.0005, user-reported UPS Rack 1920W / 1500W LV. The web UI baseline was captured in the November 8, 2026 screencast. Dell M2M protocol documents `POST /get_object.xml` with `objectName=UPS.*`; the endpoint must still be validated against the user's live card.

- Firmware: <https://www.dell.com/support/home/en-us/drivers/driversdetails?driverid=x2jyn>
- M2M protocol: <https://dl.dell.com/manuals/all-products/esuprt_ser_stor_net/esuprt_rack_infrastructure/dell-line-interactive-rack-ups-1920r_user%27s%20guide13_en-us.pdf>

The device remains on factory firmware unless the operator explicitly performs a hardware flash. This repository includes no automatic device update mechanism.
