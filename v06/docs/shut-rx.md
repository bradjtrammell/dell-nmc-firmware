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
