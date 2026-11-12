# Native NS7520 DMA + Ethernet packet integration

This connects the driver's receive DMA queue (channel 1A) to the actual
freestanding Ethernet/ARP/IPv4 packet processor and then to the transmit
DMA queue (channel 2). It is the first complete software path from a
DMA-delivered Ethernet frame to a queued reply frame.

- No dynamic allocations or guessed physical pointers.
- The owner supplies and independently qualifies both CPU and DMA bus mappings.
- Descriptor and receive/transmit buffer regions must not overlap.
- Frames are accepted only with DMA RXOK, a bounded length, and no RX error.
- Transmit backpressure drops new reply opportunities without overwriting an
  in-flight DMA buffer; source descriptors are rearmed after CPU consumption.
- Invalid/partial DMA operations return errors rather than continuing with an
  untrusted frame.
- Unit tests use a strict, injected register adapter and simulate RX/TX
  completion and a reply generation. The same code cross-compiles for ARM7.

This does NOT yet configure the NS7520 MAC/EFE, real PHY clocks, dynamic
IP settings, interrupts, ARP cache, TCP/IP, HTTP network socket or actual
physical UPS report transport. A real H910P MMIO adapter and physical board
smoke test remain required. This image must not be flashed to a live card.

References: Digi NS7520 Hardware Reference Rev G Chapter 8 DMA and Chapter 9
Ethernet, including the RX1A/TX2 channel assignments.
https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/
