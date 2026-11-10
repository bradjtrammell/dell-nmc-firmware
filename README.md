# Dell H910P NMC — Modern Firmware Interface

**A modern, local-first web management interface embedded directly into the Dell H910P UPS network management card.**

This project extends the useful life of Dell H910P network management cards by replacing their aging, frame-based web interface with a modern desktop-first UPS management console. The UI is packaged **inside the card's firmware**: it does not need Docker, a second computer, a cloud account, or an external runtime service.

> [!WARNING]
> **Experimental — working on real hardware, not recommended for production-critical deployments.** Earlier custom builds have been installed and used on a physical H910P. However, the newer **v0.5.4 and v0.5.5 builds have a confirmed on-device UI startup regression**: the custom interface fails to load, and legacy web pages are not a reliable fallback. They are **not recommended for flashing**. A valid firmware checksum or a successful FLASH-write message does not establish a functioning UI or UPS management parity. Keep a verified recovery route and a known-working firmware image.

**Independent community project. Not affiliated with or endorsed by Dell.**

## Why this project exists

An H910P can still report essential UPS information and manage connected equipment, but its original interface relies on older frames and scattered configuration pages. Administrators should be able to see utility status, battery charge, electrical measurements, load, recent events, and configuration without hunting through disconnected screens.

Rather than add another always-on service in front of the card, this project aims to modernize the software **on the hardware you already have**:

- **Direct management:** open the card's IP address in a browser.
- **Operational visibility:** dashboard, power flow, load, battery, output groups, events, and trends.
- **Usable controls:** consistent forms, validation, confirmation, and state readback for supported actions.
- **Local-first behavior:** no internet dependency to operate the UPS management dashboard.
- **Protocol preservation:** reuse Dell's existing on-card HTTP/XML management interfaces and firmware callbacks where practical.
- **Responsible firmware maintenance:** check compatibility, confirm successful writes, and establish recovery before calling an update safe.

## Compatibility and architecture

| Property | Details |
| --- | --- |
| Hardware target | Dell H910P Network Management Card, rev A06 |
| Base firmware | Dell 01.18.0005 |
| Example UPS | Dell UPS Rack 1920W / 1500W LV |
| Hosting | Embedded in the NMC firmware (no server or container) |
| Client | Modern browser with JavaScript and gzip decompression support |
| UI | HTML/CSS/JavaScript, locally embedded assets |
| Data | Existing Dell HTTP/XML endpoints, including `/get_object.xml` |
| Firmware format | Dell NET+OS-derived `.bin` image with embedded RomPager resources |
| Development status | Hardware-functional in earlier revisions; incomplete and currently regression-affected |

The NMC controls and monitors physical power equipment. **Treat this repository as experimental firmware engineering, not a general-purpose web theme.**

### What is covered

The intended modern UI has 15 destinations:

| Monitoring | UPS operations | Configuration |
| --- | --- | --- |
| Overview | UPS control | Email notifications |
| Measurements | Weekly schedule | Network |
| Output groups | Shutdown parameters | System |
| Event history | | Shutdown clients |
| | | Access control |
| | | SNMP |
| | | Time & NTP |
| | | Firmware |

The overview and telemetry have run against real H910P data in earlier builds. Configuring email recipients, live network settings, shutdown clients, power switching, and every firmware update/recovery path are **not yet comprehensively verified on hardware**. Showing a screen or passing a simulated browser test is not proof that a management command works.

The automatic GitHub updater, secure activation, and rollback **are not yet complete**; the existing firmware uploader is a separate OEM mechanism.

## Using a working installation

On a card running a proven working custom firmware image, open:

```text
http://<nmc-ip-address>/index.htm
```

For example, for a lab card at `192.168.1.50`:

```text
http://192.168.1.50/index.htm
```

Authenticate with the NMC's configured credentials. When the custom interface is running, measurements are requested directly from the card rather than relayed through GitHub.

**Network security:** on firmware exposing HTTP Basic authentication over plain HTTP, credentials and traffic are not encrypted. Restrict access to a trusted management network or VPN; do not publish the NMC to the internet.

### Recovery considerations

The retained OEM firmware-upload status page is typically:

```text
http://<nmc-ip-address>/app_upload.htm
```

A reachable upload page is **not** proof of bootloader-level recovery. Experimental builds may replace legacy HTML/help/graphics resources, and some original pages may show blank content. Do not assume `/default.htm` remains a working fallback, and do not attempt repeated blind flashes when the interface fails.

## Building the source currently on GitHub

> [!NOTE]
> The checked-in `main` branch contains the **earlier experimental builder**, not a synchronized or qualified v0.5.5 source release. The following commands describe that checked-in builder. They **do not** produce a verified fix for the current on-device v0.5.5 failure.

### 1. Clone the project

```bash
git clone https://github.com/bradjtrammell/dell-nmc-firmware.git
cd dell-nmc-firmware
mkdir -p originals
```

### 2. Supply the Dell factory firmware archive

Obtain the official Dell 01.18.0005 H910P firmware archive yourself. For a downloaded file in `~/Downloads`:

```bash
cp "$HOME/Downloads/NMC_Firmware_01_18_0005_X2JYN.zip" \
  originals/NMC_Firmware_01_18_0005_X2JYN.zip
```

The image is proprietary Dell software and is **not** included in this source repository. Never commit factory firmware, credentials, device logs containing secrets, or generated custom BIN images to source control.

Official base firmware: [Dell driver X2JYN](https://www.dell.com/support/home/en-us/drivers/driversdetails?driverid=x2jyn).

### 3. Build the embedded interface and firmware image

For Python 3.11+ and the checked-in code:

```bash
python3 build_native.py
```

This performs an **offline** rebuild and generates:

```text
build/
├── H910P_01.18.0005_INSTRUMENT_PANEL_EXPERIMENTAL.bin
├── VALIDATION.json
└── served-native-ui/
```

The script packs compiled UI resources into existing firmware string slots, verifies the application and CRC, and checks that changes stay inside the expected resource ranges. It does **not** connect to or flash a physical NMC.

Newer engineering-package builds use additional dependencies (such as a C++17 compiler and pinned `zopfli`) and may produce a different image. Use each release's own source and build instructions; do not assume this old builder matches the newest firmware.

### 4. Run regression tests

To run the firmware regression suite, also provide the separate unmodified Dell `01.16.0001` archive at `originals/NMC_Firmware_01_16_0001_YJ6JM.zip`:

```bash
python3 -m unittest discover -s tools -p 'test_*.py' -v
```

For browser tests, install Python Playwright and Chromium in the environment expected by `test_browser.py`, then run:

```bash
python3 test_browser.py
```

Those browser tests use simulated card responses, and **cannot establish actual NMC HTTP serving behavior**. The v0.5.4/v0.5.5 regressions demonstrate why firmware-embedded byte checks and browser mocks are necessary but insufficient.

### 5. Inspect the build results

```bash
cat build/VALIDATION.json
sha256sum build/H910P_01.18.0005_INSTRUMENT_PANEL_EXPERIMENTAL.bin
ls -lh build/H910P_01.18.0005_INSTRUMENT_PANEL_EXPERIMENTAL.bin
```

**Do not flash that output solely because offline validation passes.** Device-side HTTP MIME handling, authenticated sign-in, actual live UPS data, power controls, and recovery must also be validated on hardware.

## Firmware updates

The Dell firmware upload handler is `/Forms/app_upload_1`; the status page is `/app_upload.htm`. A manually initiated HTTP/1.0 multipart upload has worked on hardware, but the card's explicit flash-write success response must be checked. A redirect or a successful file transfer alone does not prove successful activation, and neither confirms the web UI loads correctly.

There is **no general installation command for an unqualified image**. For a version that has been verified on the target hardware, use its release-specific checksums and deployment instructions. Do not use the failed v0.5.4 or v0.5.5 images as a recovery image.

## Repository layout

```text
source/             Embedded UI modules and styles
build_native.py     Offline firmware/resource packer
tools/              Dell image parser, compressor, RomPager inspection, validation
test_browser.py     Browser QA with simulated device responses
docs/               Recorded hardware/UI baseline and engineering notes
originals/          Local-only proprietary Dell firmware input (gitignored)
build/              Locally generated firmware and validation outputs (gitignored)
```

## What full replacement requires

The project is aiming for complete functional parity, not a collection of navigable mock screens. It still needs validated real-card settings read/write/readback, robust event and outlet handling, reliable notification management, complete legacy feature coverage, a working on-device update workflow, and tested recovery after interrupted/failed updates.

**Most urgent current issue:** fix the actual H910P-served embedded loader/resources and stop erasing the legacy UI until the replacement interface can be independently verified on hardware.

## Documentation and references

- [Current UI and parity baseline](docs/current-ui-baseline.md)
- [Dell firmware 01.18.0005](https://www.dell.com/support/home/en-us/drivers/driversdetails?driverid=x2jyn)
- [Dell UPS protocol and management documentation](https://dl.dell.com/manuals/all-products/esuprt_ser_stor_net/esuprt_rack_infrastructure/dell-line-interactive-rack-ups-1920r_user%27s%20guide13_en-us.pdf)

Dell, H910P, and related marks belong to their respective owners. The project is independently developed and neither endorsed nor supported by Dell.
