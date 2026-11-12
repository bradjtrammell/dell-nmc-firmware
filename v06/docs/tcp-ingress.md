# Native VLAN-aware TCP management ingress

The freestanding ARM7 ingress guard validates every received Ethernet packet
against its exact ingress VLAN, configured destination address, source CIDR
and read-only HTTP service permission before a future TCP session can use it.

This version rejects corrupt IPv4/TCP checksums, IPv4 fragments and options,
malformed TCP headers, incorrect ports, VLAN mismatch and denied networks.
It implements mandatory RFC 9293 TCP pseudoheader checksum validation.

The resulting descriptor provides VLAN identity, source/destination IP
and ports, TCP sequence/acknowledgment/flags and a borrowed payload pointer.
Copy borrowed bytes before rearming the RX DMA descriptor.

This is not a TCP handshake, retransmission engine, socket, actual Ethernet
listener, authenticated management endpoint, TLS, or UPS control driver.
L2 VLAN tags and source IPs must never be treated as authentication.

GCC C99 tests, Clang ASAN/UBSAN and big-endian ARM7 cross-compilation
have been executed on equivalent source. Combined dockeradm build remains
to be verified. See https://www.rfc-editor.org/rfc/rfc9293.html
