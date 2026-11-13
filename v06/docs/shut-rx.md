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


## HID Feature descriptor interpreter

The read-only `board/hid_report.c` module can compile a bounded HID report
descriptor into Feature fields on USB HID **Power Device (0x84)** and
**Battery System (0x85)** usage pages. It respects Feature report IDs,
constant padding, bit offsets and little-endian bit numbering, signed
logical ranges and per-report layouts. Short-item PUSH/POP and collection
boundaries are checked. Long items, unsupported ranges, malformed layouts,
oversized reports and values outside descriptor-declared limits fail closed.

It decodes raw integers without guessing HID unit exponents or physical
scales. Feature decoding never permits an Output or SET REPORT operation,
and decoded values always retain `telemetry_verified=0`. A descriptor
from an independently identified real H910P UPS and a physical read driver
are required before connecting any field to production telemetry. Current
tests use a **synthetic** descriptor matching the layout of the published
MGE example; it is not asserted to be the Dell H910P layout.

Reference: USB HID 1.11 item encoding and HID Power Device usage tables
(USB-IF), plus MGE 51029473ZA AC p.14 for the example 4-byte report.


## Complete descriptor-bound telemetry assembly (research only)

`ou_hid_telemetry` is the new native SHUT/HID-to-provider bridge. It accepts
only a completed SHUT response, validates it against the independently
sourced HID Feature report descriptor, and maps **nine explicit required
fields** (battery %, load %, remaining runtime, input/output decivolts,
AC present, battery present, and both switched load-segment states). The
HID usage pages, usage IDs, report IDs, and integer calibration ratios must
be supplied from real hardware evidence: **no guessed field positions or
implicit physical units**. Missing or duplicate descriptor fields, bad
ranges, fractional scaling, short/extra packet data, stale or replayed
messages fail closed.

A complete snapshot requires all nine fields, each captured within a
four-second coherence window. Each complete set is consumed once,
assembled atomically into the already-existing `ou_driver_report` and
checked by the provider before submission to the stateful native
telemetry guard and HTTP API. The integration host test exercises the
full path: **synthetic SHUT reports -> HID descriptor -> mapped measurements
-> provider -> native HTTP 200**, as well as untrusted-source 503,
staleness, truncation, missing fields, replay, invalid units, cache
invalidation and monotonic clock wrap.

**IMPORTANT:** The interface takes four explicit physical-source evidence
assertions. The current research-only target supplies none. These assertions
are an *interface contract*, **not actual independent attestation**. The
test fixture intentionally synthesizes them; that is not a working
H910P driver or real UPS telemetry. Never set them from an XML response,
known MAC address, string matches, generic SHUT checksum or wishful test
fixture. The weak native H910P port still refuses physical startup until
electrical routing, actual descriptor, source identity and recovery
have been independently established. No power-control or write path exists.

## Qualified NS7520 FIFO to SHUT session adapter

`board/shut_uart_bridge.c` connects the new documented NS7520 UART
receive path to the existing bounded SHUT transaction state machine.
When a single 32-bit FIFO read crosses a SHUT fragment boundary and
the receiver must emit an ACK before continuing, the adapter **retains
the unread bytes locally** and resumes after the external transaction
owner has drained the pending ACK. It never drains additional FIFO data
while SYNC, ACK, or GET REPORT transmit is waiting. It also rejects
physical receive errors without returning a stale verified report.

The integration test uses a synthetic register/byte-sequence backend
to exercise the real Status A/FIFO register addresses, a SYNC/GET REPORT
exchange, two checksum-verified fragmented SHUT packets crossing FIFO
word boundaries, inter-fragment ACK gating, preserved bytes, matched
report ID, and failure after a missing physical-source qualification.
All outgoing frames are **only returned to the host test**;
the target implements **no UART transmitter** and no real UPS electrical
wiring. This is therefore still **NOT FLASHABLE**. Actual on-card
serial channel identity, verified UART timing, FIFO wiring, independent
recovery and physical protocol qualification remain outstanding.

## Loss of UART trust invalidates the complete SHUT session

An MMIO read failure or receive error now calls the central
`ou_shut_session_abort(..., OU_SHUT_SESSION_ERR_IO)` handler,
which scrubs raw SHUT fragments, pending outbound packet bytes,
completed-but-unconsumed telemetry, and the parser state. The FIFO
adapter clears its callback and refuses additional polls until a newly
qualified hardware port rebinds the session. Tests verify the error
reason, failure counter, zeroed buffers, and denial of resumed polling.

## Two-phase physical TX handoff

The SHUT state machine now provides `peek_tx` and `confirm_tx` to avoid
entering receive mode before all pending wire bytes are physically sent.
A partial transmission must abort, not confirm. The existing synchronous
`take_tx` API uses these primitives and remains backward compatible.
