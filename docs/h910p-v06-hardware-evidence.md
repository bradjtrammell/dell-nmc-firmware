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

## NS7520 documentation and memory map

Digi's [NET+OS Programmer's Guide](https://hub.digi.com/dp/path%3D/support/asset/net-plus-os-programmers-guide/) documents these **reference BSP address windows**, which must not be interpreted as physical RAM/flash part sizes:

- RAM on CS1/CS2: `0x00000000–0x01FFFFFF` (up to 32 MiB address aperture)
- ROM/flash on CS0: `0x02000000–0x021FFFFF` (reference 2 MiB address window)
- NVRAM on CS3: `0x03000000–0x03001FFF`
- SoC peripherals: `0xF0000000–0xFFFFFFFF`

The [NS7520 Hardware Reference](https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/) documents the ARM7TDMI processor, memory controller, serial peripherals, watchdog, memory aliasing, GEN System Control at `0xFFB00000`, and System Status at `0xFFB00004`. These are **CPU physical-register addresses, not commands to run through the card's serial Settings port or an SSH session**.

## PCB photographic evidence

The [original eBay listing](https://www.ebay.com/itm/203124239830) includes a [component-side photograph](https://i.ebayimg.com/images/g/LIgAAOSw1LpghkQw/s-l1600.webp) with an unpopulated `JTAG`-labeled footprint. The [reverse side](https://i.ebayimg.com/images/g/eSsAAOSwjUNghkQs/s-l1600.webp) shows board routing. These are photos of the **listed** card; no installed-board pinout, voltage or revision identity is yet certified.

## Safe live collection on dockeradm

The confirmed FTDI FT232R serial Settings interface is `/dev/ttyUSB0`, 9600 8N1, no flow control. It has already displayed a valid maintenance menu and read-only DHCP configuration. It does not expose a known OS/debug shell.

For an independent read-only identification pass, query sequentially (no retries) these **existing Dell endpoints** on the trusted management LAN:

1. `GET /product.xml` — device identity, possibly hardware and UPS serials.
2. `GET /configuration_descriptor.xml` — response format and schema parseability (intermittently malformed on OEM firmware).
3. `POST /get_object.xml` with body `objectName=UPS.*` — **read-only** UPS measurements.

Use HTTP/1.0 and HTTP Basic with a password prompt, 5-second connection and 25–30-second total timeout, and at least two seconds between requests. Save raw files with `umask 077`. Do not upload raw XML or headers to this public repo; they can contain identifiers and sensitive values. Instead report only HTTP status, length, SHA-256, XML parse result and counts of `GROUP`, `ITEM` and `OBJECT` elements. Do not query `System.*` casually because it may include secrets.

The original EEPROM/flash capacities, exact flash sector map, actual bootloader revision/recovery behavior, memory-chip markings, and UPS electrical/protocol implementation remain **unverified**. The evidence is adequate to begin **offline** ARM7 startup, linker, memory-controller and firmware-driver reverse engineering; it is **not adequate to flash** native v0.6 firmware.

## References and guardrails

- Original vendor package: `NMC_Firmware_01_18_0005_X2JYN.zip`, containing `ReleaseNote_DELL_NMC_01_18_0005.txt`, the OEM BIN and the public `MibUpsDell.mib`.
- [Digi NS7520 datasheet](https://hub.digi.com/dp/path%3D/support/asset/ns7520-data-sheet/).
- [NET+OS Programmer's Guide](https://hub.digi.com/dp/path%3D/support/asset/net-plus-os-programmers-guide/).
- [NS7520 Hardware Reference](https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/).
- [ADR-0002](adr-0002-v06-independent-firmware.md).

Never perform a speculative serial command, NMC reset, UPS power-control action, flash-write, JTAG attachment, or direct register access on the installed card. A separately approved maintenance restart and proven physical recovery path are prerequisites for investigating the boot menu.
