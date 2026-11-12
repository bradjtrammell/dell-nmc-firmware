# Board runtime physical PHY link preflight

A caller-supplied link_confirmed bit is no longer sufficient to start
physical NS7520 Ethernet. The runtime now reads the Clause 22 PHY ID and
BMSR link state through the restricted MAC/MDIO HAL, discovers a unique
PHY on addresses 0..31, and refuses DMA startup unless the hardware
reports link-up. This is an actual board driver read path, not a stub.

The physical PHY ID is stored in ou_board_runtime.phy for diagnostics.
The boot port must still independently establish verified PHY clocks,
MAC bus ownership, installed board identity, RAM DMA mappings and PHY
negotiated speed/duplex before boot can run.

Host tests include real PHY MDIO discovery with valid and absent link,
plus the existing boot/clock/failure invariants; tested under GCC,
ASAN/UBSAN, and armeb ARM7 compilation.
