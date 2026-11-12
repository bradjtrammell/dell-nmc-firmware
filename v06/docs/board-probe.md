# v0.6 H910P NS7520 passive board inventory

An independently compiled, freestanding read-only module checks seven
documented NS7520 control/status registers and 16 memory-controller registers.
It has no MMIO adapter in the research ELF, no network input, no write callback,
and no mechanism to flash the target. Only an independently authorized,
recoverable board may supply a physical read callback in a later stage.

The exact seven registers are GEN SYS_CONTROL (0xFFB00000), GEN SYS_STATUS
(0xFFB00004), GEN interrupt enable (0xFFB00030), Ethernet EGCR (0xFF800000),
Ethernet EGSR (0xFF800004), Serial 1 Control A (0xFFD00000) and Serial 2
Control A (0xFFD00040). The remaining 16 addresses are the NS7520 MMCR and
5 chip-select BAR/ORA/ORB triples already handled by ns7520_mem.c.

Reading the GEN revision value 0x29 supports the SoC classification, not the
H910P wiring, clock, UART mode, flash layout, or Ethernet PHY. A decoded DRAM
window at OEM load address 0x08004000 does not establish installed RAM capacity,
writeability, or safe execution. Any failed read invalidates the whole snapshot.
The probe always refuses flash qualification.

Next physical qualification gates: independent rescue/JTAG access to a spare
card, installed RAM/flash identification, correct bootloader RAM mapping,
native Ethernet PHY/DMA discovery, and actual UPS-link framing captures.
The working UPS must not be reset, modified, or used as a flash test.

Source: Digi NS7520 Hardware Reference Rev G (September 2007):
https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/

Validation: make test, make arm-check, make footprint-check, make board-sanitize.
