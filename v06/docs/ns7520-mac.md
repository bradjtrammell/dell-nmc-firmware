# NS7520 native Ethernet MAC/EFE startup

Implements the documented NS7520 register programming for:
- All three 16-bit MAC station address registers SA1/SA2/SA3.
- Inter-packet-gap and collision/retry timing, supplied by an independently
  verified source rather than guessed constants.
- Maximum frame size and MAC2 CRC-generation/padding and full-duplex mode.
- Correctly ordered enablement of EFE receiver/transmitter DMA and MAC1 RXEN.
- Validation that an RX DMA channel is active and TX channel prepared before
  enabling receive traffic; explicit disable and partial-failure rollback.

Calls are refused unless MAC is exclusively owned, PHY clocks and DMA
mapping are verified, link status confirmed, and timing settings supplied
from known-good board configuration. An already-running MAC is never seized.
No direct physical register adapter is connected in the research image.

This is the MAC initialization primitive, **not** a completed H910P NIC BSP.
It does not initialize PHY clocks, identify the actual installed PHY, configure
interrupts/watchdogs, set the full board memory map, or establish an IP/TCP
HTTP listener. It cannot safely run on the operating H910P yet.

Digi NS7520 Hardware Reference Rev G, Ethernet chapter, tables 52, 53,
60, 61, and 77-79.
https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/

## EFE full-duplex coordination

The NS7520 MAC2 FULLD and EGCR EFULLD bits must agree with verified
PHY duplex negotiation. The prior implementation set only MAC2 FULLD.
This change stores the validated duplex mode and applies it to both
registers before receive enablement, with a half/full duplex regression.
