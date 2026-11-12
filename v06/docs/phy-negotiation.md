# PHY negotiation and loss-of-link fail-closed acceptance

The physical NS7520 PHY must not be driven with a guessed duplex setting.
The board runtime now reads IEEE Clause 22 BMCR/BMSR (BMSR twice for
latch-low link status). If autonegotiation is active, it requires the
autonegotiation-complete flag and resolves shared ANAR/ANLPAR 10/100
technology bits in IEEE preference order: 100FD, 100HD, 10FD, 10HD.
If the link is forced, it takes speed/duplex from BMCR. It refuses reset,
loopback, isolated, powered-down, remote-fault, link-down, unsupported
1000-Mbps modes, or unresolvable advertised partner capabilities.
All operations are read-only PHY transactions via the existing HAL;
no negotiation restart or PHY write is introduced.

Runtime startup compares actual negotiated duplex with the MAC
configuration. Every 1000 monotonic milliseconds the runtime checks the
same PHY again. Disappearance, negotiation failure, duplex change or
speed change quiesces MAC and RX/TX DMA and requires full board
re-provisioning, rather than allowing existing TCP sessions to survive
a hardware change. This is a fail-closed policy, not transparent
reconnection.

Local GCC C99, ASAN/UBSAN tests and freestanding ARM7 big-endian object
compilation passed. The emulated cases include forced and advertised
10/100 half/full, incompatible advertisement, autoneg incomplete,
loopback/isolation, PHY I/O errors, startup mismatch and runtime link
loss. Physical H910P PHY negotiation remains unverified and the firmware
is not flash-qualified.

References:
- Digi NS7520 Hardware Reference, Ethernet control chapter
  https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/
- IEEE 802.3 Clause 22 MII BMCR/BMSR and advertisement register definitions
  https://github.com/torvalds/linux/blob/master/include/uapi/linux/mii.h
