# H910P v0.6 — independent firmware feasibility decision

**Date:** 2026-11-11 · **Status:** Independent application-build feasibility **confirmed offline**; on-device acceptance/recovery **not yet demonstrated**.

## Answer

**Yes:** There is now strong hardware-format evidence that the Dell H910P can have a genuinely independent **application firmware**, replacing the proprietary management application, XML/CGI interfaces, web server, UPS management services and RTOS-level functionality, **while retaining the existing NET+OS-compatible bootloader** as startup/recovery infrastructure.

**No:** It is **not safe to install** such a replacement on the attached card until the bootloader's actual recovery entry point, physical flash geometry, UPS hardware communications and a controlled first-boot procedure are established. A complete replacement of the bootloader itself is unnecessary for native management firmware and remains unproven.

These two statements must never be conflated: a structurally correct image or a valid checksum is **not** evidence of hardware execution.

## Evidence gathered this pass

### OEM boot header, application startup and compression

| Property | Dell 01.16.0001 | Dell 01.18.0005 |
| --- | --- | --- |
| Vendor BIN size | 1,416,830 bytes | 1,517,029 bytes |
| Decompressed application | 2,533,980 bytes | 2,757,180 bytes |
| Native main-image format | NET+OS `bootHdr` v2, flags 9 | Same |
| Flash-offset field | `0x00010000` | Same |
| RAM-destination field | `0x08004000` | Same |
| First ARM instruction | `0xE59FF038` | Same |

Both vendor images are independently CRC-validated and decompressed using the existing `tools/h910p_rebuild.py` decoder. The address fields are **boot-image values**, not proof of a physical partition or full RAM capacity.

### Identical embedded recovery-code image in both versions

Both decompressed applications contain a **byte-identical**, separately packaged NET+OS executable:

- Complete embedded image: **44,140 bytes**, SHA-256 `2083936d3217efd8fd7bb71b8612fbd5a07ef513564b175b01e699b603f3214f`.
- Boot header: version **3**, flags **9**, flash-address field **0**, RAM-destination field **`0x00404000`**, compressed executable payload **44,100 bytes**.
- Decompressed ARM program: **119,276 bytes**, with validated CRC and original ARM vector structure.
- Its code contains messages for corrupted application flash, starting a TFTP download of `image.bin`, writing flash, decompression and execution. Processor register literals include `0xFFB00000` and nearby NS7520 system-control addresses.
- The main image and embedded executable target **different RAM addresses**. Do not infer flash layout or actual bootloader install state from this alone.

**Classification:** Real embedded executable with recovery behavior. It has not been confirmed to be the bootloader currently resident on the installed NMC, nor that serial/TFTP recovery is available after a failed application flash. The firmware also contains bootloader-2.0 text, which is **not** a readout of the installed bootloader version.

### Independent code generation: proven offline

Tested with LLVM clang/LLD 17:

1. Independently wrote big-endian ARM7TDMI assembly and C (no Dell code or RTOS linked).
2. Cross-compiled to a valid **ARM ELF32 big-endian** executable, entry `0x08004000`.
3. Verified the OEM-style first instruction `0xE59FF038`, 15 instruction pads and a linked reset vector.
4. **Only in memory**, compressed the new program with LZSS2, assembled `bootHdr` v2/flags 9, calculated its NET+OS CRC, and decoded the 154-byte synthetic image successfully with the OEM-compatible parser.
5. Discarded the image. No bootable device firmware was saved, uploaded, or tested against the NMC.

**Testing:** 7 recovery/image forensic regression tests + 3 independent ARM compiler-proof tests passed. Both original Dell archives were decoded independently. No hardware execution tests were performed.

The local research deliverable is `H910P_v0.6_Independent_Firmware_Feasibility_Pack.zip`, containing the read-only auditor, original-source compiler proof, tests, and JSON output reports. **Do not add proprietary vendor executable bytes to Git.**

## Relevant vendor documentation

Digi explicitly documents [separate `rom.bin` bootloader and `image.bin` application](https://www.digi.com/support/knowledge-base/how-to-production-firmware-update-on-net-os-based), and states the bootloader initializes chip selects, copies or decompresses the application to RAM, then executes it. The [NET+OS Programmer's Guide](https://hub.digi.com/dp/path%3D/support/asset/net-plus-os-programmers-guide/) documents the image header, CRC, compression, optional recovery image, serial and TFTP recovery architecture. The [Green Hills BSP Porting Guide](https://hub.digi.com/dp/path%3D/support/asset/net-works-6-3-with-green-hills-bsp-porting-guide/) explains why a custom RAM load address, startup implementation, hardware BSP and drivers must match the target board.

**Critical qualifier:** These are generic vendor mechanisms. Dell can customize the loader, updater, and rescue paths. The actual installed H910P behavior has not yet been observed at boot.

## Go/no-go gates

| Gate | Pass condition | Status |
| --- | --- | --- |
| G0 — Format/toolchain | Independent ARM7 code + OEM-style image round trip | **PASS (offline only)** |
| G1 — Bootloader | Installed bootloader identity, boot sequence, rescue entry and failure path recorded | **OPEN** |
| G2 — Board memory | Actual chip-select/RAM/flash sizes, flash sectors, bootloader/data boundaries verified | **OPEN** |
| G3 — UPS transport | Independent read-only native communication implementation tested with real protocol evidence | **OPEN** |
| G4 — Recovery | Credible recovery from failed native application without power loss to managed loads | **OPEN** |
| G5 — Hardware start | A native image boots on separately recoverable H910P with serial trace and watchdog/network health | **OPEN** |
| G6 — Feature parity | Full UPS management, schedules, notifications, HTTPS, configuration, signed updates and UI tested | **OPEN** |

## Decisions and next safe work

- Preserve stable **v0.5.10 on `main`**. Native v0.6 lives on **`dev`**, no extra branches.
- **Do not reset, flash, factory-default, interrupt serial boot, or manipulate UPS outputs** on the installed card as part of passive discovery.
- Retain the bootloader for v0.6 unless independent evidence shows a hard incompatibility. Investigate direct bootloader replacement separately.
- Continue offline disassembly of both the native boot/recovery executable and OEM flash writer, including sector mapping, image validation and TFTP error paths.
- Reconstruct the native UPS link and protocol before enabling power commands. A 420-object XML inventory is a management schema, not proof of protocol implementation.
- Use a spare recoverable card or an approved downtime window to observe boot/rescue behavior. Without this, **hardware-safe native flashing remains blocked**.

**Verdict:** Technically **YES** to an independent application firmware, with unusually strong offline evidence. Operationally **NOT YET** to installing it on the running H910P until boot recovery, flash geometry and UPS communications have been verified.
