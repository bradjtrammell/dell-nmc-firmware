# NS7520 Ethernet RX1A / TX2 DMA engine

The DMA primitive implements the documented hardware descriptors and
channel registers for the NS7520 SoC, not generic DMA pseudo-registers.
RX1A channel registers: FF900000/10/14. TX2: FF900080/90/94.
The fly-by descriptor is two 32-bit big-endian words: bus address with
W/I/L ownership bits; status, F bit, and 15-bit byte length.
Only caller-supplied 1024-aligned descriptor rings are allowed here.

This is one descriptor per RX and TX channel, suitable for incremental board
bring-up, not a high-throughput network release. All MMIO and memory ordering
are through injected verified, independently controlled callbacks.
No CPU pointer is ever assumed to equal a DMA physical address.
The DMA start fails without explicit ownership, PHY clocks and verified
DMA-bus address mapping. It never initializes the unknown physical PHY,
controls UPS power or performs firmware writes.

- RX: prepare descriptor empty, synchronize, start channel 1A, inspect F,
  validate RXOK, length/error bits, synchronize data, deliver once, rearm.
- TX: bounded copy into DMA buffer, synchronize payload/descriptor, program
  channel 2, wait for F to clear, require TXOK, disable channel.
- On uncertain or invalid completion the queue faults closed instead of
  presenting damaged frames or overwriting in-flight buffers.
- The physical adapter still needs the real H910P memory map, MAC/PHY
  initialization, EFE RX/TX FIFO enablement, timer and IRQ control.
- There is NO physical MMIO adapter in the research firmware image yet.

Manufacturer: Digi NS7520 Hardware Reference Rev G, pp. 130-145,
sections DMA descriptors, register map, fly-by mode and error handling.
https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/

Validation: standard C99 tests, ASAN/UBSAN, ARM7 big-endian compilation,
and linkage in standalone research ELF.
