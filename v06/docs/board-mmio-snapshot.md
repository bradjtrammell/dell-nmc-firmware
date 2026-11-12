# Physical H910P board MMIO snapshot (read-only)

This is a controlled real-MMIO adapter for the existing NS7520 board decoder.
A future, separately qualified H910P board port can supply
`ou_ns7520_arm_read32` (or its independently mapped equivalent) as the
physical read callback; the adapter forwards **only** the 23 documented
non-FIFO registers required by `ou_board_probe_read`:

- GEN system control/status and interrupt enable (3).
- EFE Ethernet control/status (2).
- Serial A/B control A (2). No serial receive/data registers.
- Memory controller MMCR and 5 x (BAR, ORA, ORB) (16).

Three independent opt-in gates must all be exactly 1: authorization,
verified peripheral mapping and independently verified recovery access.
The function cannot write registers or issue any UPS command. It
rejects unlisted UART FIFO, GPIO, flash and memory addresses.

The resulting snapshot decodes the actual controller aperture and
checks the OEM application load-address window, but **does not measure
installed RAM or flash capacity**. It explicitly never sets the
hardware-verified, UPS, recovery or flash-install approval flags.

The host fixture tests all 23 reads, a simulated DRAM chip-select
covering 0x08004000, refusal before qualification, denial of unsafe
addresses, and failure cleanup for every individual read. This file
does not make a live H910P application bootable; the physical memory
map, installed chip geometry, UPS bus and independent recovery path
remain to be measured from an approved, recoverable board.

References: Digi NS7520 Hardware Reference Rev G, Table 35 and GEN,
EFE and serial-control register maps.
