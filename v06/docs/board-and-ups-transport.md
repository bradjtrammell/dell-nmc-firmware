# H910P v0.6 — native board/UPS driver engineering checkpoint

**Status:** Implemented **portable, read-only platform components**; no live physical H910P driver, verified boot recovery, network stack, or validated UPS packet decoder. Never flash the research ELF/DAT. Approved existing UI and v0.5.10 stable firmware remain untouched.

## Board register mapping: NS7520

The Digi NS7520 Hardware Reference (Rev G, September 2007), Table 35, pp. 87–88, maps the memory-controller registers:

- Memory module configuration: `0xFFC00000` (`MMCR`)
- Five chip-select groups: `BARn` at `0xFFC00010 + 0x10*n`, `ORnA` at `+4`, `ORnB` at `+8`, `n=0..4`
- A chip select's BAR **bit 0** enables its decode, **bit 1** write-protects it, and **bit 3** selects DRAM mode.
- The upper 20 bits of `ORnA` are an address-mask, **not a measured flash-chip size**. Decode may alias non-contiguous addresses.

`board/ns7520_mem.c` has a pure decoder for all 16 documented controller registers. A caller supplies a `read32` callback: **no built-in MMIO read/write, no unsafe discovery scanning, and no access to the installed NMC**. It distinguishes enabled, write-protected and DRAM windows, checks for overlapping memory-decode masks, and recognizes contiguous address windows. It deliberately never claims actual installed RAM/flash capacity. The real H910P's values remain unknown because the serial Settings menu does not expose arbitrary memory registers.

## H910P UPS protocol, not guessed UART

Independent integrity-checked decoding of both Dell original firmware versions (01.16.0001 and 01.18.0005) found the same vocabulary: `threadBinding`, `shutHostGetReport`, `shutHostSetReport`, `COPI_READ`, `NO_REPLY_EVT`, `NO_DATA_EVT`, `SendStringBufferToUPS`, `SendNMCFirmwareToUPS`, and HID/report terms. A reproducible, SHA-allowlisted, **offline-only** auditor lives at `v06/tools/probe_oem_transport.py`, and its sanitized output lives in `v06/docs/oem-transport-evidence.json`.

These are firmware **strings**, not disassembled driver call graphs or captured electrical frames. **Do not conclude USB HID, ordinary UART, SPI, GPIO or a specific checksum based on them.** The Ethernet and serial Settings interfaces are not evidence of which bus physically connects the card to the UPS.

`board/ups_link.c` provides an ARM7-compatible, bounded receive-only observer accepting **already-delimited messages from a future verified hardware adapter**. It stores up to 32 metadata records (timestamps, sizes and FNV fingerprints), never raw packet content, and has no outbound or power-control function. It does not parse the protocol, authenticate messages, or mark telemetry verified. The existing `ou_provider_accept` remains the only path for a driver to submit an independently validated sample; it fails closed.

## Flash safety and capacity

`board/flash_policy.c` validates physical geometry and five **distinct protected regions**: bootloader, recovery, application, configuration and logs. It refuses unknown geometry, unverified recovery, missing/overlapping regions, invalid erase alignment, and out-of-range offsets. It contains no erase/write implementation. Its test fixtures use an explicitly **fictional** 4 MiB device. Neither the 1,517,029-byte OEM image nor the `Mosaic 4M 16M` product string proves the installed memory capacity.

## Tested portable code and remaining physical gates

`make clean all test footprint-check` builds both host and big-endian ARM7 modules, runs C and Python tests, links the complete non-flashable research ELF, and measures stored `.text/.rodata/.data` and RAM `.bss/.stack` usage. The extra board modules increase the link size modestly but do not include an actual Ethernet stack or UPS driver.

Before a **physical** test image: verify controller values and bank/erase geometries on a separate recoverable card or through a proven read-only diagnostic facility; establish which NMC pins and host-report protocol connect to the UPS; prove independent recovery access through the connected Settings port; add board clock/interrupt/watchdog initialization, a verified NIC DMA driver and networking stack. No changes to the live 192.168.0.174 card are authorized in this engineering pass.

## Sources

- [Digi NS7520 Hardware Reference, Rev G](https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/), especially memory controller chapter, Table 35 and the BAR/OR bit descriptions.
- [Digi NET+OS Programmer's Guide](https://hub.digi.com/dp/path%3D/support/asset/net-plus-os-programmers-guide/), DMA hardware dependencies, Chapter 5 bootloader.
- Original vendor archives supplied by the hardware owner, their SHA-256 pinned in the sanitized evidence report (not redistributed).

## NS7520 dual-channel serial UART FIFO receiver (new native component)

`board/ns7520_serial_rx.c` implements the **actual documented CPU register
read path** for both NS7520 serial channels, not an abstract byte queue:
Status A at `0xFFD00008` or `0xFFD00048`; receive FIFO at
`0xFFD00010` or `0xFFD00050`. It observes RRDY (bit 11), RXFDB
(bits 21:20; 00=4, 01=1, 10=2, 11=3 valid bytes), rejects RBRK/RFE/
RPE/ROVER (bits 15:12) and inconsistent receive-buffer closure. It
**never drains** the FIFO if the caller has inadequate output capacity,
and never reads FIFO before checking receiver status and source gates.
No UART/SPI clock or pinmux configuration, register writes, resets,
interrupt ACKs, DMA or power commands exist.

Before *any* read, the H910P BSP must independently qualify: peripheral
memory mapping, the specific channel (1 or 2), UART FIFO rather than
SPI/DMA mode, FIFO byte-lane convention, exclusive UART ownership, and
**measured physical connection of that channel to the UPS**. The user-facing
RJ45 Settings console is not presumed to be the internal UPS bus. The
SHUT specification suggests **2400 baud** for a SHUT interface, while
Dell's *external Settings console* operates at **9600 baud**; neither
speed is evidence of the internal link configuration.

The guarded receive-only code is linked in the ARM7 research image and
tested for both channel register maps, 1–4-byte FIFO transfer lengths,
read failures, serial overrun/framing errors and refused unqualified
access. This does **not** establish which H910P channel is connected to
the UPS or certify its voltage levels.

Sources: Digi NS7520 Hardware Reference Rev G, chapter 10, pp. 221–222,
233–238, 250, https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/
and MGE SHUT general specification section 2.1.

## Dynamic NS7520 UART mode verification

The UART FIFO adapter now reads the **actual** Serial Control A/B registers
before every FIFO poll. It refuses to drain bytes unless the controller is
enabled, in 8N1 format (WLS=11, parity/extra stop disabled), not in serial
DMA or local/remote loopback, with UART (rather than SPI) mode, normal bit
ordering and NRZ receiver encoding. These conditions are decoded from
the documented NS7520 register bits, not copied from an assumed H910P
pinout or hardcoded baud rate. All four accesses remain read-only,
whitelisted and independently gated on a qualified board connection.

A known SHUT UART format does **not** establish which serial channel
the H910P uses or that its onboard UPS bus is even UART-based. The
component still refuses use without separate electrical proof; the
factory Settings jack remains untouched.

References: Digi NS7520 Hardware Reference Rev G, Table 85 pp.223–227,
Table 88 pp.229–231, Table 89 pp.233–238.
