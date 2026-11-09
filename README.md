# Dell H910P NMC — Instrument Panel

Desktop-first, frame-free custom web interface **embedded inside the original Dell NMC application**, targeting H910P A06 / vendor firmware **01.18.0005**. It serves from the NMC's own address; no external application or Docker deployment.

## v0.4.2 — Native upload-test build

**The old v0.4.1 build should not be used again.** We found that its generated `/index.htm` referenced the bootstrap but did not load the 15 JavaScript chunks containing the new UI. The old browser tests supplied those chunks manually and masked the defect. Version 0.4.2 corrects the native page and the tests.

An optimal, byte-exact LZSS2 encoder also reduces the modified firmware image to **1,507,635 bytes**, which is **9,394 bytes smaller** than Dell's original **1,517,029-byte** firmware. No OEM images, UPS controller instructions, callbacks, or static resource pointers were removed to achieve the reduction. The only application-data edits are original `/index.htm` and 16 repurposed legacy Help resources, whose MIME type is changed to JavaScript.

This is an experimental software candidate, not evidence of successful device upload. Dell's native uploader previously returned `ERROR : HTTP transfert failure` after a `303 See Other` on the larger v0.4.1 image. **The actual cause of that upload failure remains unproven.** A new 0.4.2 upload tests whether reduced size matters while also fixing the startup bug.

The firmware's internally advertised Dell version remains **01.18.0005**. The separate `0.4.2` number tracks this custom UI source and BIN; no downgrade field is introduced.

## UI design

Personality: precise, assured, industrial — a professional power-distribution control console. A light `#f2f6f7` content canvas, white instruments, Dell blue `#007db8`, green `#087a53`, charcoal `#18272f` side navigation, warm amber `#a86814`, red `#b93e30`. Tight, angular 4–5 px radii; Segoe UI/Arial and tabular numeric readouts; dense management navigation; SVG icons and animated power-flow paths. Motion lasts roughly 180–220 ms and respects `prefers-reduced-motion`. Desktop is the priority; a mobile fallback exists.

## Existing functionality

- 15 management destinations derived from the owner's frame-by-frame recording of the original NMC UI.
- Live read-only UPS load, charge, runtime, voltage and output telemetry via existing `get_object.xml` XML endpoints; local trend chart, events, refresh/retry states.
- Native configuration editor reads the original NMC `configuration_descriptor.xml`, displays descriptor-advertised RW fields, validates a single change, requests confirmation, submits `Configuration.xml` to `/Forms/set_obj_2`, and requests an independent readback before displaying success.
- Native outlet command panel uses only the original descriptor's `Control.Outlet[n]` objects, with explicit typed confirmation. Command acceptance is not treated as observed power-state completion.
- Product identification filters password- and community-like values.
- Existing Dell firmware-management and incomplete native workflows remain accessible through their original URLs. Weekly scheduling and some vendor-specific configuration operations are **not** a completed native replacement.

## Build

**Inputs:** Python 3.11+, C++17 compiler (`g++`), owner-provided Dell vendor ZIP in `originals/NMC_Firmware_01_18_0005_X2JYN.zip`. Python libraries for firmware building are standard-library only. No network or NMC connection is needed to build. Build on Linux with:

```bash
python3 build_native.py
python3 tools/test_validation.py
python3 tools/test_optimal_encoder.py
python3 test_native_entry.py
```

Output: `build/H910P_01.18.0005_INSTRUMENT_PANEL_EXPERIMENTAL.bin` and `build/VALIDATION.json`. Firmware build fails if the optimized BIN exceeds the OEM BIN, has invalid header/CRC, contains unexpected modified executable bytes, or fails a full decompression round trip.

For Playwright Chromium browser tests (Chromium expected at `/usr/bin/chromium`):

```bash
python3 test_browser.py
python3 test_config_browser.py
python3 test_http_integration.py
```

The HTTP integration test **skips** in environments where Chromium refuses loopback navigation. Simulated browser and XML tests do not prove NMC runtime compatibility.

## What offline validation establishes

- Both vendor images decode and validate.
- Exact startup header, image format, RAM and flash address fields, RomPager pointers and dynamic callbacks are preserved.
- The original application length is unchanged, with changed bytes limited to allowlisted UI resources and MIME enum fields.
- The modified BIN CRC validates and its decompression equals the precise patched application.
- The NMC entry page contains all 16 native script references **in the correct order**, and their descriptor MIME types are JavaScript.
- Browser tests load precisely the embedded script contents in that order and simulate the Dell M2M endpoints, including error and control confirmation states.

**Not yet established:** whether the H910P accepts the upload, boots and serves the full UI, implements the simulated XML forms exactly, or supports automatic recovery.

## Project files

- `build_native.py` — immutable-address UI patching, browser entry, image rebuild.
- `tools/lzss_opt.cpp` — optimal byte-cost LZSS2 parser/encoder. Produces a lossless stream accepted by the Python decoder.
- `tools/h910p_rebuild.py` — independent reference decoder, checksum and header parser.
- `tools/rompager_map.py` — original NMC resource descriptor mapping.
- `source/` — complete current UI CSS, SVG/icon utilities, navigation and JS.
- `test_native_entry.py`, `test_browser.py`, `test_config_browser.py`, `tools/test_optimal_encoder.py`, `tools/test_validation.py` — validation.
- `originals/`, `build/` — local/private, excluded from Git. Do not redistribute vendor firmware or user credentials in this public repo.

Official Dell firmware: https://www.dell.com/support/home/en-us/drivers/driversdetails?driverid=x2jyn

## Live device upload issue (v0.4.1)

The browser submitted `POST /Forms/app_upload_1` with 1,533,873 request bytes (BIN + multipart overhead), and the NMC responded `303 See Other` redirecting to `/app_upload.htm`. The page displayed `ERROR : HTTP transfert failure`. That 303 establishes only an application response, not image acceptance. Neither binary size nor browser MIME encoding is confirmed as the root cause. Version 0.4.2 is smaller than Dell's original image, eliminating the demonstrated size overage but not establishing whether the transport error is resolved.
