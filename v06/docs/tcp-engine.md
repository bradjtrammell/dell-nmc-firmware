# ARM7 VLAN-keyed stateful TCP transport

The freestanding TCP engine adds 4 independent connection slots keyed by
VLAN ID, tagged/native mode, peer MAC/IP/port and local IPv4 address.
Each slot handles SYN/SYN-ACK/ACK, in-order bounded HTTP requests up to
4096 bytes, reply segmentation at 1024 bytes (also bounded by the peer's
validated SYN MSS and its advertised receive window), ACK and FIN handling,
750 ms retransmission and 20 s idle
expiry. The response callback may use the existing read-only native HTTP API.

The engine uses only caller-owned static storage and preserves 802.1Q tags
and per-interface egress PCP. Ingress MUST pass the separately tested
TCP/IPv4 checksum and VLAN HTTP-read ACL verifier.

This is an initial TCP subset: no TCP timestamp negotiation, SACK, zero
window probing, IPv4 fragmentation or high-throughput pipelining. There is
no cryptographic ISN source or real MAC driver wired to the runtime yet.
The hardware adapter must supply unpredictable per-boot ISN seed and valid
monotonic milliseconds; never enable plaintext device control. Authentication,
physical UPS communications, UI and Ethernet hardware bring-up remain.

The test uses a mock of the already-tested VLAN/TCP ingress validator to
exercise session sequencing, output frame generation, retransmission and
multi-VLAN isolation independently. Native C99, ASAN/UBSAN and ARM7 builds.

The SYN parser now validates TCP option lengths, rejects duplicate/zero/
malformed MSS options, and defaults to the IPv4 MSS of 536 bytes when
no MSS option is advertised. The sender enforces the negotiated per-peer
MSS before queuing any TCP response payload. Host regressions include
small-MSS segmentation and real-checksum SYN option validation. This
does not replace full TCP interoperability or hardware qualification.
