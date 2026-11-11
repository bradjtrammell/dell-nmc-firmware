# H910P v0.6 — confirmed offline hardware evidence

**Status:** Read-only research; November 11, 2026. This is not a hardware-validated flash layout or bootloader recovery procedure. **Stable `main` v0.5.10 remains unchanged.**

## Vendor firmware evidence

Original Dell X2JYN archive `NMC_Firmware_01_18_0005_X2JYN.zip`, locally verified with project NET+OS decoder.

| Item | Evidence |
| --- | --- |
| Original `Dell_nmc_01_18_0005.bin` | 1,517,029 bytes; SHA-256 `ed5a8eb47ae5bdf72817286d22fccd06b8a68381194f461f7fb271325b1a6a6a` |
| Header | 36-byte NET+OS `bootHdr`, version 2, flags 9, passing original CRC |
| Header flash offset | `0x00010000` (not physical flash-map proof) |
| Header RAM load address | `0x08004000` (not installed RAM capacity) |
| Application | 1,516,989 compressed bytes, 2,757,180 decompressed bytes, LZSS2 |
| CPU/BSP hints | `ns7520_a_esp` at `0x1AF5E4`; `NS7520 Development Board` at `0x20D808` |
| OS hint | `ThreadX ARM7/Green Hills Version G4.0.4.0` at `0x297345` |
| ARM boot vector | `0xE59FF038` (big-endian ARM `LDR PC,[PC,#0x38]`); literal `0x08064884` at application offset `0x40` |
| Rescue prompt | `Press a key to display the Rescue Menu` at `0x20CAFD` |
| Embedded bootloader version | `BOOTLOADER VERSION : 2.0` at `0x232BBC` (NOT proof of installed bootloader version) |
| Diagnostics | `RAM autotest SUCCESS.` at `0x232BF4`; `get_flash_size` linker-limit diagnostic at `0x20D178` |
| Persistent storage | `FLASH0/NetworkSettings.cfg` at `0x1D4EE8`; `RAM0/EVENT_UPS.CSV` at `0x18FF20` |

Original Dell release-note findings:

- **01.08.0001:** Adds a rescue menu during boot.
- **01.13.0001:** Corrects a flash-sector-60 overlap between the filesystem and sensor logging. Do not infer sector geometry or chip capacity from this number.
- **Older releases:** Mention Dell H910P bootloader 1.3 compatibility; installed bootloader version is unconfirmed.

The image also embeds bootloader failure messages (`DOWNLOAD_FAILED`, `INVALID_IMAGE`, `FLASH_ERROR`, `OUT_OF_MEMORY`) and TFTP-thread strings. These **do not** prove that any remote recovery interface is accessible on the installed card. In particular, the rescue prompt's presence **inside the application** does not establish that a corrupted application can still be recovered independently.

## Vendor image cross-check (01.16.0001)

The older original Dell `Release_01_16_0001.bin` was independently decoded: SHA-256 `df6471c61126fb95b393f6c91006d136d0e63612f19ae2f5d43f33fbaef932ba`, **1,416,830 bytes**, LZSS2 application **2,533,980 bytes** decompressed. Its image header retains the same flash offset `0x00010000` and RAM load address `0x08004000`. It also contains the `ns7520_a_esp` identifier, rescue-menu prompt and embedded bootloader-2.0 string. This independently corroborates the same BSP/boot-vector lineage across the two vendor versions, **not** actual installed bootloader or flash-chip capacity.

## NS7520 documentation and memory map

Digi's [NET+OS Programmer's Guide](https://hub.digi.com/dp/path%3D/support/asset/net-plus-os-programmers-guide/) documents these **reference BSP address windows**, which must not be interpreted as physical RAM/flash part sizes:

- RAM on CS1/CS2: `0x00000000–0x01FFFFFF` (up to 32 MiB address aperture)
- ROM/flash on CS0: `0x02000000–0x021FFFFF` (reference 2 MiB address window)
- NVRAM on CS3: `0x03000000–0x03001FFF`
- SoC peripherals: `0xF0000000–0xFFFFFFFF`

The [NS7520 Hardware Reference](https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/) documents the ARM7TDMI processor, memory controller, serial peripherals, watchdog, memory aliasing, GEN System Control at `0xFFB00000`, and System Status at `0xFFB00004`. These are **CPU physical-register addresses, not commands to run through the card's serial Settings port or an SSH session**.

### RAM mapping and bootloader recovery caveats

Digi's standard ARM7 BSP reference RAM aperture is `0x00000000–0x01FFFFFF`, whereas the Dell header specifies `RamAddress=0x08004000`. **These are not the same address window**, and the Dell value is not a simple `0x40000000`/`0x80000000` reference alias. Do not use the generic memory map as the H910P linker script without reconstructing the OEM board's address decode and memory-controller configuration. The `FlashOffset` boot-header field is an offset within flash, not an absolute mapped CPU address, according to Digi's NET+OS header specification.

Digi's NET+OS Programmer's Guide, Chapter 5, describes a generic staged bootloader with application CRC, backup-image, DHCP/BOOTP/TFTP and serial-download recovery options. The Dell firmware's embedded `DOWNLOAD_FAILED`, `INVALID_IMAGE` and rescue-prompt strings are **consistent with** this design, but are not proof the installed Dell bootloader supports all those paths. **No** recovery key presses, TFTP transfers, serial image uploads or deliberate flash failures are authorized by this research.

## PCB photographic evidence

The [original eBay listing](https://www.ebay.com/itm/203124239830) includes a [component-side photograph](https://i.ebayimg.com/images/g/LIgAAOSw1LpghkQw/s-l1600.webp) with an unpopulated `JTAG`-labeled footprint. The [reverse side](https://i.ebayimg.com/images/g/eSsAAOSwjUNghkQs/s-l1600.webp) shows board routing. These are photos of the **listed** card; no installed-board pinout, voltage or revision identity is yet certified.

## Live hardware read-only collection — November 11, 2026

The owner executed three sequential authenticated, HTTP/1.0 **read-only** requests from `dockeradm` to the live H910P at `192.168.0.174`. All responses were HTTP **200** (the card responded with HTTP/1.1 headers), and all XML parsed successfully:

| Endpoint | Size | Root | Parsed inventory | SHA-256 |
| --- | ---: | --- | --- | --- |
| `GET /product.xml` | 1,112 bytes | `PRODUCT_INFO` | Device identity schema (field names pending sanitization) | `00a7fc3abebfbee0a691f5f460d9f356015a61f8ad78ad1b72269d93acf1aa01` |
| `GET /configuration_descriptor.xml` | 35,146 bytes | `SYSTEM_CONFIG` | 24 `GROUP` and 302 `ITEM` nodes | `8a1995f1e479b239b70ddb09a7437e25d9d82e41983ed30c72d986945702bbe0` |
| `POST /get_object.xml`, `objectName=UPS.*` | 27,716 bytes | `GET_OBJECT` | 420 `OBJECT` nodes | `7d9b78d8e8a1429ebc88c35b3d60bdc91783fe9756f96250207fb4e5233c8311` |

This corroborates the 24-group/302-item OEM configuration descriptor already found offline and establishes that the card can return the full reported schema and telemetry on successful requests. **A single successful sample does not rule out intermittent malformed responses or prove reliable write operations.** HTTP/1.0 in curl controls the client request; an HTTP/1.1 server response is not a mismatch or error.

The owner retained raw response files privately on `dockeradm`, not in this public repository. Their contents may include host/device identifiers or sensitive configuration values. Until a sanitized field inventory is received, do not infer installed RAM size, flash-chip geometry, bootloader version or board revision from these XML counts.

## Safe live collection on dockeradm

The confirmed FTDI FT232R serial Settings interface is `/dev/ttyUSB0`, 9600 8N1, no flow control. It has already displayed a valid maintenance menu and read-only DHCP configuration. It does not expose a known OS/debug shell.

For an independent read-only identification pass, query sequentially (no retries) these **existing Dell endpoints** on the trusted management LAN:

1. `GET /product.xml` — device identity, possibly hardware and UPS serials.
2. `GET /configuration_descriptor.xml` — response format and schema parseability (intermittently malformed on OEM firmware).
3. `POST /get_object.xml` with body `objectName=UPS.*` — **read-only** UPS measurements.

Use HTTP/1.0 and HTTP Basic with a password prompt, 5-second connection and 25–30-second total timeout, and at least two seconds between requests. Save raw files with `umask 077`. Do not upload raw XML or headers to this public repo; they can contain identifiers and sensitive values. Instead report only HTTP status, length, SHA-256, XML parse result and counts of `GROUP`, `ITEM` and `OBJECT` elements. Do not query `System.*` casually because it may include secrets.

The original EEPROM/flash capacities, exact flash sector map, actual bootloader revision/recovery behavior, memory-chip markings, and UPS electrical/protocol implementation remain **unverified**. The evidence is adequate to begin **offline** ARM7 startup, linker, memory-controller and firmware-driver reverse engineering; it is **not adequate to flash** native v0.6 firmware.

## Full schema-only inventory — live capture

A subsequent **offline** inspection of the owner's saved responses confirmed the OEM interface advertised by `PRODUCT_INFO`, with **24 configuration groups, 302 configuration items, and 420 UPS/system objects**. No further network requests or configuration writes were performed.

### `PRODUCT_INFO` capabilities (tag names only)

`SUMMARY`, `HTML_PROPERTIES_PAGE`, `XML_SUMMARY_PAGE`, `CENTRAL_CFG`, `CSV_LOGS`, `ALARMS`, `SUBSCRIPTION`, `BROADCAST`, `MANAGEMENT`, `XML_MANAGEMENT_PAGE`, `MANAGEMENT_PAGE`, `UPS_DATA`, `GET_CONFIG`, `SET_CONFIG`, `GET_OBJECT`, `SET_OBJECT` and `TGW`.

This is primarily a **device capability and protocol discovery document**, not a physical chipset/memory inventory. It describes how the OEM application can access configuration, management, alarms, logs and UPS objects. It does not reveal installed RAM/flash chips.

### Actual config-group distribution

| Group | Items | RW | RO | Other |
| --- | ---: | ---: | ---: | ---: |
| Network | 18 | 16 | 0 | 2 |
| System | 5 | 5 | 0 | 0 |
| Shutdown.Outlet[1] | 8 | 8 | 0 | 0 |
| Shutdown.Outlet[2] | 5 | 5 | 0 | 0 |
| Shutdown.Outlet[3] | 5 | 5 | 0 | 0 |
| Control.Outlet[1–3] (3 groups) | 9 | 9 | 0 | 0 |
| Access | 26 | 18 | 8 | 0 |
| Five privacy-redacted groups | 150 | 150 | 0 | 0 |
| Schedule | 14 | 14 | 0 | 0 |
| NetworkManagementSystem[0–2] (3 groups) | 21 | 21 | 0 | 0 |
| ClientCfg | 5 | 5 | 0 | 0 |
| Environment.Temperature | 10 | 9 | 0 | 1 |
| Environment.Humidity | 8 | 7 | 0 | 1 |
| Environment.Input[1–2] (2 groups) | 14 | 14 | 0 | 0 |
| Time | 4 | 4 | 0 | 0 |
| **Total** | **302** | **290** | **8** | **4** |

**Five redacted groups** have sizes 6, 36, 36, 36, and 36; the filter hid their group names because it matched the term `email` among other privacy patterns. Their observed structure is consistent with the OEM notification groups, but raw names were not published.

### Telemetry domains observed

The 420 objects include `BatterySystem` (11), `PowerSummary` (43), `PowerConverter` (35), `OutletSystem` (14), individual outlet instances (4, 8, 8), four `Email[0..3]` instances (36 each), `Network` (38), three network-management-system instances (7 each), seven schedule entries (2 each), environment sensors, time/NTP, and login/security state. These are **schema/object counts, not proof that every optional sensor or control exists electrically**.

### Practical implication for independent v0.6

The native JSON schema must cover the full existing management domain, but should use semantically typed resources and explicit capability flags rather than replicate 420 opaque OEM key/value strings. Power-changing operations must be implemented and tested independently with guarded state transitions. The existing XML snapshots are useful offline compatibility fixtures; they are not the intended runtime transport.

The captured schema does **not** identify actual flash chip, RAM chip size, installed bootloader revision, or complete board memory decode. Those still require independent OEM image/boot-driver analysis or planned boot diagnostics; the existing Settings menu does not expose a memory read/debug shell.

## References and guardrails

- Original vendor package: `NMC_Firmware_01_18_0005_X2JYN.zip`, containing `ReleaseNote_DELL_NMC_01_18_0005.txt`, the OEM BIN and the public `MibUpsDell.mib`.
- [Digi NS7520 datasheet](https://hub.digi.com/dp/path%3D/support/asset/ns7520-data-sheet/).
- [NET+OS Programmer's Guide](https://hub.digi.com/dp/path%3D/support/asset/net-plus-os-programmers-guide/).
- [NS7520 Hardware Reference](https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/).
- [ADR-0002](adr-0002-v06-independent-firmware.md).

Never perform a speculative serial command, NMC reset, UPS power-control action, flash-write, JTAG attachment, or direct register access on the installed card. A separately approved maintenance restart and proven physical recovery path are prerequisites for investigating the boot menu.
