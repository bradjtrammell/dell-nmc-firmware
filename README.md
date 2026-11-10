# Dell H910P — Embedded UPS Management Interface

The Dell H910P project modernizes the original Dell UPS Network Management Card's web interface **directly in its firmware**, without running a separate web-server container or relying on the internet for device monitoring. It provides a live UPS dashboard, 15 management destinations, protected settings editing, alert management, and firmware-update controls.

**Current release baseline: v0.5.10 (November 10, 2026).** This is the project’s user-designated release baseline. The custom interface has operated on real H910P hardware in earlier releases, and development continues. **It is not recommended for production-critical UPS installations.** The v0.5.10 session/XML corrections passed offline tests; complete v0.5.10 hardware acceptance has not been independently documented.

## What the project provides

- Direct on-card management at `http://NMC-IP/default.htm`, with embedded JavaScript and CSS.
- Power measurements, battery, load, outlet groups, event log, and live graphical dashboard.
- Configurable UPS controls, notification delivery, scheduling, network settings, SNMP, access, time/NTP, and firmware management.
- A unified table of configured notification destinations: at most four **native email recipients** (Dell slots 0–3), with independent Monita, Discord, and ntfy destinations not subject to that limit.
- A separate SMTP settings modal, provider-specific destination forms, and optional notification relay for HTTPS webhook delivery. The H910P's four email slots are not interchangeable webhook slots.
- Use of Dell's existing backend endpoints for supported reads/writes; an external relay is required for Monita/Discord/ntfy delivery until a native transport is proven.

## Why it exists

The original interface is frame-oriented and difficult to use on modern desktops. This project aims to bring the card's useful UPS management features into a coherent, directly-hosted administrator interface, keeping the electrical control plane local rather than creating another always-on management dependency.

## Hardware and version

- Target: Dell H910P rev A06, Dell 01.18.0005 firmware.
- Known example: Dell UPS Rack 1920W / 1500W LV.
- **Locked release baseline: v0.5.10**, including session continuity and XML reliability changes.
- Built image: `H910P_Custom_UI_v0.5.10.bin` (1,517,026 bytes).
- SHA-256: `e3adea74aad966c15e87f9fdb7407bb9d9d810e20158da54bad639466f70b8bb`.
- Firmware downloads: [GitHub Releases](https://github.com/bradjtrammell/dell-nmc-firmware/releases).
- Factory maximum image size: **1,517,029 bytes**; firmware rebuild refuses larger output.
- Core: Python 3.11+, C++17 `g++`, `zopfli==0.4.2`, and the original Dell firmware ZIP supplied by the operator.

## Release and source status

**Release v0.5.10** is the baseline to retain for subsequent firmware work. The release artifact must be the exact 1,517,026-byte firmware BIN named above, not an HTML demonstration. The engineering ZIP contains the corresponding source and test assets.

**Source synchronization:** this repository’s checked-in historical builder predates the v0.5.10 engineering package. A clone of `main` must not be represented as a reproducible v0.5.10 build until the v0.5.10 sources have been synchronized. The build and test commands below apply to the complete **v0.5.10 engineering package** rather than the historical `main` tree.

## Build from source

Run these commands from the extracted v0.5.10 engineering package. Keep proprietary Dell firmware and credentials out of version control.

```bash
mkdir -p originals
cp "$HOME/Downloads/NMC_Firmware_01_18_0005_X2JYN.zip" originals/
python3 -m pip install -r requirements-build.txt
python3 build_native.py
```

The build produces `build/H910P_01.18.0005_INSTRUMENT_PANEL_EXPERIMENTAL.bin`, `build/VALIDATION.json`, and `build/served-native-ui/`. Build does not connect to or flash an NMC. Ensure `g++` is installed beforehand. The binary format, checksum, RomPager resource layout, and decoded embedded assets are validated.

## Browser sessions and device polling

- Signing in stores a reversible HTTP Basic Authorization header in **tab-scoped `sessionStorage`**, not persistent `localStorage` or a long-lived server session. This is a compromise required by the existing NMC Basic-auth firmware: the current implementation does not provide a native server-side session/token endpoint. Browser session restoration may preserve tab storage; choose **Lock** to remove it explicitly.
- Refreshing the same tab revalidates the stored credentials against the NMC before showing management controls. Rejected credentials clear storage, and network errors prompt for sign-in without silently bypassing device authentication.
- **Security:** HTTP Basic and browser session storage do not encrypt the password. Restrict NMC access to trusted management networks or a VPN. A secure on-card HTTPS/session implementation remains outstanding.
- The single-threaded RomPager CGI interface is protected from overlapping operations using one serialized NMC request queue. XML reads may retry once after a malformed response or timeout; configuration writes and power operations are **never replayed automatically**. One transient telemetry failure preserves the last successful reading; sustained failures remain visible.

## Test

```bash
python3 -m unittest test_auth_credentials.py -v
node test_session_restore.js
python3 test_native_entry.py
python3 test_final_firmware_assets.py
python3 -m unittest discover -s tools -p 'test_*.py' -v
python3 test_browser.py
python3 test_config_browser.py
python3 test_notification_bridge_browser.py
python3 -m unittest test_notification_relay.py -v
```

The firmware unit tests require the additional unmodified Dell ZIP at `originals/NMC_Firmware_01_16_0001_YJ6JM.zip`; browser tests use local Playwright/Chromium. These tests verify the firmware file and simulated NMC behavior, **not** physical boot, live notification delivery, or uninterrupted-update recovery.

## Firmware deployment

First verify the released BIN. This example assumes it has been downloaded directly to `~/Downloads`:

```bash
BIN="$HOME/Downloads/H910P_Custom_UI_v0.5.10.bin"
EXPECTED="e3adea74aad966c15e87f9fdb7407bb9d9d810e20158da54bad639466f70b8bb"
printf '%s  %s\n' "$EXPECTED" "$BIN" | sha256sum -c -
test "$(wc -c < "$BIN")" -eq 1517026
```

**Use a tested recovery image and a lab device first.** This firmware is not production-qualified. Before deploying, verify the exact release SHA-256 and image size; obtain card-specific confirmation of the FLASH write. Never treat an HTTP redirect as sufficient proof. The existing OEM firmware handler is `/Forms/app_upload_1`, and `/app_upload.htm` exposes the card's status.

For a specifically authorized test NMC, the established manual HTTP/1.0 firmware upload is:

```bash
BIN="$HOME/Downloads/H910P_Custom_UI_v0.5.10.bin"
EXPECTED="e3adea74aad966c15e87f9fdb7407bb9d9d810e20158da54bad639466f70b8bb"
NMC="192.168.1.50"

printf '%s  %s\n' "$EXPECTED" "$BIN" | sha256sum -c - &&
test "$(wc -c < "$BIN")" -eq 1517026 &&
curl --http1.0 \
  --basic \
  --user admin \
  --header 'Expect:' \
  --header 'Connection: close' \
  --connect-timeout 15 \
  --max-time 300 \
  --show-error \
  --form "file=@$BIN;filename=Dell_nmc_01_18_0005.bin;type=application/octet-stream" \
  --dump-header "$HOME/Downloads/H910P-v0.5.10-upload-headers.txt" \
  --output "$HOME/Downloads/H910P-v0.5.10-upload-response.html" \
  --write-out '\nHTTP Status: %{http_code}\nBytes Sent: %{size_upload}\nElapsed: %{time_total}s\n' \
  "http://$NMC/Forms/app_upload_1"
```

The command prompts for the NMC password and uses a placeholder lab-card address. Confirm the **exact** OEM status `SUCCESS : Firmware was successfully written in FLASH` and then validate `/default.htm`. Do not repeatedly retry a 401. Never assume HTTP 302 proves a flash completed.

A subsequent file is only a release when the hosted asset matches this BIN's checksum. Do not upload the engineering ZIP or an HTML preview as the firmware asset.

## Login authentication

The H910P enforces HTTP Basic authentication independently of this UI. The app displays one centered sign-in form and supplies the entered credentials to the card. **v0.5.10 changes browser credential handling** to prevent Chromium from launching its own Basic-auth popup on a 401 challenge. Card-side authentication is preserved, and credentials are not written to localStorage or cookies by this UI. Live compatibility must be checked after the firmware is installed.

HTTP on a LAN is unencrypted. Restrict the NMC to a protected management segment or VPN; do not put it on the public internet.

## Directory map

- `source/` — modern embedded UI, local assets, and management modules.
- `build_native.py` — Dell firmware patcher/optimized image builder.
- `tools/` — firmware parser, resource extraction, compressor, tests, and diagnostics.
- `notification-relay/` — optional local relay for HTTPS webhook destination delivery.
- `docs/` — protocols, deployment, design and engineering notes.
- `test_*.py` — browser, hardware-entry, embedded-assets, and auth regressions.
- `originals/` — local only, proprietary Dell source inputs (not distributed).
- `build/` — generated binaries, validation reports and test logs.

## Status

**v0.5.10 is the locked release baseline.** The modern UI is a functional, work-in-progress firmware project. Full replacement is not yet proven on physical hardware for every Dell control and configuration workflow. In particular, the automatic GitHub updater, transactional recovery, native HTTP webhook delivery, and full access-control modernization are not complete. Read [`RELEASE_NOTES.md`](RELEASE_NOTES.md) for the precise scope and verification of the current release.

Independent community project. Not affiliated with Dell. See [Dell H910P firmware](https://www.dell.com/support/home/en-us/drivers/driversdetails?driverid=x2jyn) and [Dell's UPS guide](https://dl.dell.com/manuals/all-products/esuprt_ser_stor_net/esuprt_rack_infrastructure/dell-line-interactive-rack-ups-1920r_user%27s%20guide13_en-us.pdf).

The optional `test_session_http.py` attempts a browser reload over a locally hosted HTTP mock. Environments that block local Chromium navigation report a **skip**; this test does not verify the actual physical H910P.