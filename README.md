# Dell H910P Network Management Card — Modern Firmware

A modern, locally hosted management interface for the Dell H910P Network Management Card. This project modernizes the interface **inside the NMC firmware**, preserving the card's hardware and Dell's underlying UPS communications. The goal is a complete replacement for the original browser interface—not a separate dashboard or an always-on management server.

**Stable release: v0.5.10** · Dell-derived embedded UI firmware · **Next generation: v0.6.x native firmware replacement (in development)**

v0.6.x is a complete firmware architecture rework, not another HTML patch. It targets an independently built NMC application, native JSON API, hardware UPS interface, authentication, notifications and update/recovery system. The current source tree's v0.5 patcher and its existing beta UI do **not** yet implement that firmware. [Read the v0.6 architecture decision](docs/adr-0002-v06-independent-firmware.md).

The custom interface runs on real H910P hardware, but development and device-level qualification are ongoing. Critical UPS power-control, recovery, and update operations must be validated before this firmware is used in a production environment.

## Why I built this

The H910P remains capable hardware, but its original management interface is dated and cumbersome to navigate. I wanted a single, modern place to see what the UPS is doing, manage its configuration, review events, and eventually update the management firmware without giving up local control or adding a separate service to the network.

The interface is hosted by the NMC itself. A modern browser loads the UI and communicates directly with the card's existing endpoints. No cloud connection is required for UPS monitoring or native email notification.

## Hardware and firmware

| Component | Details |
| --- | --- |
| Management card | Dell H910P, revision A06 |
| Base firmware | Dell 01.18.0005 |
| UPS example | Dell UPS Rack 1920W / 1500W LV |
| Release baseline | **v0.5.10** |
| Firmware file | `H910P_Custom_UI_v0.5.10.bin` |
| Image size | **1,517,026 bytes** |
| SHA-256 | `e3adea74aad966c15e87f9fdb7407bb9d9d810e20158da54bad639466f70b8bb` |

The firmware builder enforces Dell's original 1,517,029-byte image-size ceiling and validates the packed resources, checksum and image structure. Passing those checks does not replace testing on a physical card.

## Features

The management interface includes 15 sections covering the power dashboard, electrical measurements, output groups, event history, UPS controls, weekly scheduling, shutdown parameters, notification destinations, network, system, shutdown clients, access control, SNMP, time/NTP and firmware maintenance.

The dashboard reads live UPS telemetry and presents battery, load, power flow and key electrical measurements. The interface includes configuration editing and readback checks where supported by the card. The newer builds add login continuity across same-tab refreshes and bounded recovery from transient XML read failures.

**Notifications:** One destination table supports four native Dell email recipients (slots **0–3**), and separately configured **Monita, Discord and ntfy destinations without a four-destination limit**. SMTP configuration opens in its own dialog. External HTTPS notifications use an optional local relay; the H910P firmware does not yet provide a proven native HTTPS webhook client. Do not treat the relay as optional if you depend on webhook delivery.

**Authentication:** The card retains its native HTTP Basic authentication mechanism. The custom sign-in is an interface over that mechanism; it is not a replacement server-side session system. Tab-scoped session restoration temporarily stores a reversible Basic authorization header. Use the card only on a trusted management segment or behind an appropriate VPN; avoid sending passwords over untrusted plain HTTP.

## Release channels and branches

| Channel | Git branch | Purpose |
| --- | --- | --- |
| **Release** | `main` | Approved firmware baseline. Currently **v0.5.10**. |
| **Beta** | `dev` | Continuing development, fixes and hardware qualification. |

`main` remains the v0.5.10 stable baseline. Work on the independent v0.6.x firmware belongs on `dev` until its native image is hardware-qualified, reviewed and explicitly promoted. Existing dev v0.5.11 beta patcher code remains historical/compatibility work and does not become v0.6 firmware by changing its version text. The firmware-update interface is being developed to offer a **Release / Beta** selector. Release checks must use published GitHub firmware assets and verify their integrity; choosing `dev` must not treat an arbitrary source commit as a flashable image. The optional update mechanism is not a transactional on-device installer with rollback yet.

## Getting started

Open the NMC in a browser at:

```text
http://NMC-IP/default.htm
```

For example, a lab card at `192.168.1.50` uses `http://192.168.1.50/default.htm`. Sign in using the credentials configured on the card. The default Dell credentials should be changed before putting a card on a management network.

### Verify the release BIN

After downloading the **standalone** `H910P_Custom_UI_v0.5.10.bin`:

```bash
BIN="$HOME/Downloads/H910P_Custom_UI_v0.5.10.bin"
EXPECTED="e3adea74aad966c15e87f9fdb7407bb9d9d810e20158da54bad639466f70b8bb"

printf '%s  %s\n' "$EXPECTED" "$BIN" | sha256sum -c -
test "$(wc -c < "$BIN")" -eq 1517026
```

### Build from source

Python 3.11+, `g++` (C++17), and the pinned Python dependencies are required. **Dell's proprietary original firmware archives are not stored in this repository.** Obtain the official 01.18.0005 firmware and, for firmware regression tests, 01.16.0001 from Dell. Place the archives into `originals/` with these filenames:

```text
originals/NMC_Firmware_01_18_0005_X2JYN.zip
originals/NMC_Firmware_01_16_0001_YJ6JM.zip
```

On Linux, from the repository root:

```bash
python3 -m pip install -r requirements-build.txt
python3 build_native.py
```

The build is offline; it does not contact or flash an NMC. Inspect `build/VALIDATION.json`, the generated `build/served-native-ui/` resources, and the final firmware checksum before considering any hardware test. To reproduce the **released** v0.5.10 BIN, build from `main` at the corresponding baseline commit and use the exact Dell original archives; compare the resulting BIN's hash with the table above.

### Run tests

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

Browser tests require Chromium and Playwright. The tests cover packaged firmware assets, UI functionality and simulated NMC responses. They do not establish real-device session, power-control, flash or recovery behavior.

### Flashing an experimental card

Only flash a card you can recover. The Dell firmware upload service is `POST /Forms/app_upload_1`; its status page is `/app_upload.htm`. The following **lab-only** example uses the already verified BIN and prompts for the configured NMC password:

```bash
BIN="$HOME/Downloads/H910P_Custom_UI_v0.5.10.bin"
EXPECTED="e3adea74aad966c15e87f9fdb7407bb9d9d810e20158da54bad639466f70b8bb"
NMC="192.168.1.50"

printf '%s  %s\n' "$EXPECTED" "$BIN" | sha256sum -c - &&
test "$(wc -c < "$BIN")" -eq 1517026 &&
curl --http1.0 --basic --user admin \
  --header 'Expect:' --header 'Connection: close' \
  --connect-timeout 15 --max-time 300 --show-error \
  --form "file=@${BIN};filename=Dell_nmc_01_18_0005.bin;type=application/octet-stream" \
  --dump-header "$HOME/Downloads/H910P-upload-headers.txt" \
  --output "$HOME/Downloads/H910P-upload-response.html" \
  --write-out '\nHTTP Status: %{http_code}\nBytes Sent: %{size_upload}\n' \
  "http://${NMC}/Forms/app_upload_1"
```

The NMC must explicitly report **`SUCCESS : Firmware was successfully written in FLASH`**. A 302 redirect does not prove the write succeeded. If the card responds with 401, stop and investigate authorization rather than retrying the upload repeatedly. Verify `default.htm`, UPS telemetry, settings and alerts after restarting.

## Serial Settings port

The second RJ45 jack is a **serial Settings port**. Dell documents 9600 baud, 8N1 and no hardware/software flow control. Its configuration menu provides network readout/recovery even if the Ethernet interface is unavailable. **It is not a second Ethernet port or a documented firmware debug console.** See [Serial Settings port](docs/serial-settings-port.md) before connecting a generic Cisco rollover cable: identical RJ45 connectors do not guarantee compatible pinouts.

## Project layout

```text
source/                 Embedded UI source, styles and assets
build_native.py         RomPager resource packer and firmware image builder
tools/                  Firmware parser, compressor and validation tools
notification-relay/     Optional HTTPS notification transport
docs/                   Firmware, serial and protocol documentation
test_*.py               Firmware, authentication and browser tests
requirements-build.txt  Pinned build dependency
originals/              Local proprietary Dell firmware inputs (gitignored)
build/                  Generated outputs (gitignored)
```

## Current limitations

This is usable experimental firmware, not a completed production replacement. Full device-level verification of power and configuration writes, transport security, browser authentication edge cases, native webhook transport, update rollback and recovery remains in progress. The built-in GitHub firmware discovery and manual upload controls are **not** a verified automatic update-and-rollback system.

For changes, see [CHANGELOG](CHANGELOG.md). For console research, see [Serial Settings port](docs/serial-settings-port.md).

**Maintained independently.** Dell and H910P are trademarks of their respective owners. This project is not affiliated with Dell.

## Release integrity gate

Before publishing a firmware BIN, run the [offline release preflight and publication commands](docs/release-quality-gate.md). The checks establish exact BIN identity, header and CRC correctness, size compatibility, and native asset structure. Stable releases additionally require hardware acceptance. Neither release discovery nor offline checks are automatic on-card installation or rollback.

## v0.6 hardware evidence

The original Dell image and vendor release notes have been analyzed for NS7520/ThreadX identity, ARM startup and flash storage. See [confirmed hardware evidence and recovery limits](docs/h910p-v06-hardware-evidence.md). These findings are offline and do not establish the installed flash/RAM capacity or safe native firmware recovery.

## Native v0.6 independent firmware feasibility

The v0.6 native ARM7 application path is now **proven at the offline code-generation and image-format level**. The original Dell 01.16 and 01.18 images contain the same separately packaged boot/recovery executable, and an independent ARM7 program successfully passed OEM-style compression, header and CRC round-trip validation. Actual on-card boot, recovery, memory geometry and UPS bus implementation are still open safety gates. See [v0.6 feasibility decision](docs/v06-independent-firmware-feasibility.md). Stable v0.5.10 remains unchanged.

## v0.6 independent native C development environment

The [`v06/`](v06/) directory now contains the **first actual independent-firmware code**, not another patch of Dell firmware: a freestanding C99 JSON API core compiling for big-endian ARM7TDMI, a loopback-only POSIX HTTP simulator using that same core, capability-gated outputs, end-to-end tests, and offline ARM object verification.

**Not yet a flashable image.** No live UPS drivers, flash startup/bootloader integration, secure credential store, or hardware-control commands are present. The simulator reports every reading as simulated and denies all write methods. The existing `build_native.py` remains exclusively the old Dell-image patcher for v0.5.x; never use it to label a new v0.6 firmware.

Run on a development Linux system:

```sh
cd v06
make clean all
make test
make arm-check
./build/openh910p-sim --port 8760
```

Query `http://127.0.0.1:8760/api/v1/health` **from the same machine**. The simulator binds only the loopback interface, never the UPS card or the LAN. See [v0.6 development README](v06/README.md) for code boundaries, safety and limitations.

The simulator now also publishes **all 302 validated configuration field definitions through the native C JSON engine**, in 19 metadata-only pages. The API includes a metadata-only field catalog; the device does not expose live settings or allow writes. The existing UI is unchanged. The generator is `v06/tools/generate_schema.py`; the offline ARM7 target compiles both JSON routing and field metadata into big-endian object files. See [the API contract](v06/docs/api-v1.md) and [native source](v06/).
