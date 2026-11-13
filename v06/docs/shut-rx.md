# SHUT receive-only UPS transport decoder — candidate, not verified H910P bus

The H910P OEM 01.16 and 01.18 applications contain `shutHostGetReport`,
`shutHostSetReport`, `GetReport`, `SetReport`, and HID references. Separately,
Network UPS Tools documents SHUT (Serial HID UPS Transfer) support for Eaton,
MGE, and Dell UPS models. **That combination is strong protocol evidence,
NOT confirmation of H910P internal electrical wiring, installed UPS framing,
or a functioning UPS driver.** The externally accessible RJ45 Settings
port is not proof of the internal UPS link.

This implementation parses the published SHUT framing **without sending a
single byte**: incoming UPS RESPONSE (0x04/0x84) and NOTIFY (0x05/0x85)
types, duplicated-nibble length 0x11..0x88 for 1..8 bytes, data-byte
XOR checksum, and bit 7 as the final-fragment flag. Fragments are
reassembled into at most 256 bytes, strictly by type; invalid length,
checksum, overflow, type mixing, broken monotonic time and 750 ms input
timeouts clear the partial buffer.

It distinguishes SYNC, ACK and NAK control bytes while between packets,
but it has **no sender, no ACK/NAK emission, no GET REPORT or SET REPORT
generator, no physical UART/SPI driver, no HID descriptor parser and
no telemetry validation**. It passes complete opaque reports to the
existing metadata-only UPS observer, which keeps timestamps, lengths and
noncryptographic fingerprints, not report data. The returned report's
`telemetry_verified` bit is always zero; the provider must not publish
measurements based only on a valid transport checksum.

Tests include single/multi-fragment RESPONSE, NOTIFY, control words,
invalid host-request frames, malformed length/XOR, 256-byte bounds,
time regression, clock wrap, timeouts, and 20,000 deterministic fuzzy
input bytes. Host GCC C99, ASAN/UBSAN, and ARM7 big-endian compile are
required build gates.

References:
- MGE 51029473ZA AC, Simplified SHUT (sections 2.2/2.3), published by NUT:
  https://networkupstools.org/historic/v2.8.0/protocols/mge/51029473zaac.pdf
- Network UPS Tools mge-shut driver:
  https://networkupstools.org/docs/man/mge-shut.html
- Source of OEM strings: docs/h910p-v06-hardware-evidence.md and
  v06/docs/oem-transport-evidence.json

The installed H910P must not receive any speculative SHUT command,
flash update, reset, or power-control instruction. The research image
remains explicitly NOT FLASHABLE.

## Pure read-only GET REPORT packet builder

`board/shut_request.c` builds the published HID feature GET REPORT
command using SHUT's final REQUEST packet (0x81), duplicated length (0x88),
`A1 01 <report-id> 03 00 00 00 00` and an XOR checksum. It has **no
transport callback or hardware write capability**; it only fills a
caller-supplied 11-byte array for later, independently qualified transport
integration. There is explicitly **no SET REPORT implementation**. Unit
tests check all 256 possible report IDs, XOR correctness and unchanged
caller buffers on argument/capacity failure. ARM7 and ASAN/UBSAN are
mandatory build checks.

Encoding a documented read-only SHUT command is NOT permission to send
it to an unverified UPS physical interface. Initial synchronization,
ACK/NAK scheduling, report descriptors, actual physical mapping and
recovery tests still remain.


## Bounded read-only SHUT polling state machine

`board/shut_session.c` and `include/ou_shut_session.h` add a **pure frame
orchestrator**: four-attempt `SYNC` spaced at least 600 ms, a typed feature
`GET REPORT` (never `SET REPORT`), bounded ACK/NAK retransmissions, a
checksum-verified per-fragment ACK, report-ID matching, notification
acknowledgment, explicit timeouts, 32-bit monotonic clock wrap handling, and
one-use result retrieval. Every result remains **telemetry_verified=0**.

The component has **no serial read/write routines or physical UART binding**.
`begin` requires both independently established physical-bus and protocol
qualification flags before it can even queue a logical frame. Those flags
are not hardware proofs by themselves. The only possible outgoing bytes
are `SYNC=0x16`, `ACK=0x06`, and the validated 11-byte `GET REPORT`
packet. It neither controls UPS power nor touches the NMC Settings port.

The actual H910P electrical interface, synchronization interoperability,
HID descriptors, device identity, real report meaning, and NMC watchdog/
recovery remain unverified. Host unit tests exercise the published handshake,
complete and fragmented replies, unsolicited notification, NAK exhaustion,
unexpected report IDs, corrupted data, clock regression, and timeout.
The module is compiled and linked in the non-flashable ARM7 research image.

Reference: MGE Simplified SHUT AC, sections 2.2–2.5, pp. 6–8 and
the published GET REPORT exchange on p. 14.
