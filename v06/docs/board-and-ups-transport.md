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

## Bounded NS7520 UART FIFO transmission for read-only SHUT (new)

`board/shut_uart_tx.c` now implements the **hardware-specific byte-width
FIFO transmit path** documented by Digi's NS7520 Hardware Reference
Rev G, printed pp. 212–213, 240–241 and 250. It writes one byte per
poll using an ARM7 `STRB`-equivalent callback at the selected channel's
FIFO address, `0xFFD00010` or `0xFFD00050`. Before **each**
byte it checks Control A/B UART 8N1 mode, disables DMA/loopback,
reads Status A `TRDY` (D03) and `TBC` (D01), and never writes
to a FIFO while the controller reports not-ready. A stuck TBC is
acknowledged by writing exactly `0x2` to the correct Status A
write-one-to-clear register; no other status/control writes are allowed.
The module has no clock, pinmux, GPIO, reset, firmware-update, or
UPS power-control writes.

The public transmit path does **not** accept arbitrary commands: it
can emit only the session's `SYNC=0x16`, `ACK=0x06`, and checksum-
validated, report-ID-matched 11-byte HID Feature `GET REPORT`
`81 88 A1 01 ...`. Non-read requests (including `SET REPORT`)
fail before any hardware write. Transmit is two-phase: the session
enters receive mode **only after the final byte** has been accepted.
Each step is bounded (at most one FIFO write), with timeout, TBC,
bad UART mode, callback error, partial-transmission abort and explicit
hardware requalification. A host register mock tests both channels
and no-write failure modes. ARM7 compile/link and sanitizer checks
are required by CI.

**No verified on-card H910P BSP is linked.** This is a usable,
hardware-address-level implementation but not proof that the installed
UPS transport is UART or SHUT; the H910P's electrical channel, real
bitrate, physical source identity and recovery remain unqualified.
It is still impossible to deploy the research image safely.

Reference: [Digi NS7520 Hardware Reference, Rev G](https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/).

## NS7520 hardware errata: TXBC is not a completion signal

**Digi NS7520 Errata Rev L (February 2006), printed page 14**, identifies
two UART issues missed by the basic hardware register specification:

- The Serial Status A TXBC/TBC bit (D01) is unreliable and **cannot
  establish that the final byte left the shift register**.
- Closely spaced FIFO writes can corrupt the next character's first bit.
  Digi recommends checking TXEMPTY (D00) and adding a character-time
  guard (and an extra bit time when the FIFO was empty).

The native SHUT FIFO transmitter now checks **both TRDY and TXEMPTY**
and adds a conservative **11-bit character interval** between its
single-byte FIFO writes. It will only enter the SHUT response-wait
state after it has **observed TXEMPTY after the final byte and waited
one full calibrated guard interval beyond that observation**. TBC
can still be W1C-acknowledged to unblock the FIFO, but it is never
accepted as evidence of wire completion, even if asserted midframe.

The baud rate is supplied by the future, independently verified H910P
BSP as a numeric value. Unknown, zero or out-of-range baud is rejected.
The UART code does not discover a physical channel or set a speculative
baud register. The timing uses the board's monotonic millisecond clock
and is still gated on physical-link verification. Tests cover missing
TXEMPTY, sticky/spurious TBC, both channels, exact completion ordering,
normal read-only reports, no TX readiness, rejected control frames,
and partial hardware failures.

Source: https://ftp1.digi.com/support/documentation/90000352_L.pdf
(printed page 14). This is a **functional correction** to the
previously committed transmit logic, not a newly qualified card.

## Integrated single-owner UPS transport event loop

`board/shut_uart_link.c` now provides the native read-only transport
driver that orchestrates the documented NS7520 UART receive FIFO,
errata-qualified transmit FIFO, SHUT synchronization, HID GET REPORT,
incoming fragment/packet assembly and required acknowledgments in
**one nonblocking cooperative step**. TX takes priority over RX so a
pending fragment ACK cannot be bypassed by another FIFO read. Every
transaction requires independently qualified physical source, UART
baud and mode. On RX/TX error, the link revokes both callback bindings,
discards partial bytes and refuses to publish a report.

The new host test uses a synthetic NS7520 register model (not just an
in-memory parser) to drive complete sync/request/ACK/response cycles,
repeat reads, both controller address sets, corrupted checksums and
physical callback failure. Only an unverified `ou_shut_rx_message` can
be retrieved. No UPD mutating commands, setting writes, bootloader,
flash driver, on-card measurements, or operator serial actions are
required by the software test.

**This is not a connected live H910P driver**: a verified on-card
BSP must still select the real UPS physical interface and prove that
its installed electrical transport is in fact this SHUT/UART protocol.
The research binary must not be flashed.

## Integrated native UPS monitor (read-only, experimental)

`board/ups_monitor.c` now owns the entire source-verified measurement cycle:
qualified NS7520 UART FIFO writes/reads → SHUT synchronization and GET REPORT
with ACKs → strict HID Feature report decoder → nine-metric coherence checks
→ stateful native service → `GET /api/v1/ups/status`. It derives a unique
report-ID queue and exact report byte lengths from the independently supplied
HID descriptor; it does not guess the Dell UPS's report IDs or units. It
schedules another full acquisition after 1,000 ms, without reusing cached
HID samples or accepting partial sets.

A failed physical read, rejected SHUT checksum, bad report ID, stale value,
or failed native publication immediately revokes both UART callbacks and
descriptor-mapping state, withdraws previously published status (HTTP 503),
and **preserves the native service's anti-replay watermark**. A new
verified physical source must be established before rebind. No power-changing
HID write path is reachable.

Host-only integration tests simulate two distinct NS7520 serial channels
including actual Status A/control and FIFO accesses, all three synthetic
HID feature report IDs, actual SHUT read/ACK framing, repeated acquisition,
API publication, corruption and unplug. Those host-supplied qualification
flags do NOT prove the installed H910P bus. The ARM7 image links the
module but the default board startup still refuses unqualified hardware.

## Qualified source reconnect with persistent anti-replay sequence

A report sequence is now preserved across explicit UPS link revocation and
rebind within the same native service uptime. The monitor never calls a
service reset to bypass previously accepted timestamps/sequence numbers.
Rebind must still repeat independent electrical, protocol, descriptor and
source checks; a temporary disconnection cannot automatically promote a
SHUT checksum into proof of physical device identity. Host tests confirm
a live→offline→requalified live transition with incrementing sequence.
