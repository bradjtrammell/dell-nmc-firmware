# Boot preflight: live decoded RAM and CPU/DMA address identity

The native H910P runtime now refuses to initialize the Ethernet MAC until
all four memory allocations have been checked through a board-supplied
CPU-pointer-to-DMA-bus translation callback. A physical DMA bus number
matching an aperture does not establish that the CPU-side pointer refers
to the same data.

A captured `ou_board_probe` is also mandatory. It must show:
- successful read-only system/memory-controller inventory;
- an observed NS7520 SoC revision matching the supported reference;
- a decoded DRAM window covering the full 1 MiB application research
  envelope starting at 0x08004000;
- the configured DMA aperture within a non-overlapping DRAM decode window.

The new MMIO snapshot adapter can obtain these register values from an
explicitly authorized and independently recoverable H910P port. It is not
automatically invoked by the incomplete weak boot port.

A DRAM **decode aperture is not proof of populated physical RAM capacity**
and the resolve callback must be implemented by a genuine board memory
mapping driver, not simply return success. Existing hardware verification,
UPS transport, authentication and independent recovery qualifications
remain mandatory before installation.

The runtime host fixture tests: correct mapped buffers; wrong CPU
buffer with an otherwise valid DMA address; missing translator;
invalid SoC revision; and failed application-window validation. The
test compiles real NS7520 HAL, PHY, memory decoder and boot coordinator;
only the Ethernet packet loop and physical bus callbacks are simulated.
