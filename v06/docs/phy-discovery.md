# NS7520 PHY address discovery

IEEE 802.3 Clause 22 PHY addresses are five-bit values **0 through 31**.
The initial driver mistakenly disallowed address zero. This revision fixes
that error and adds `ou_phy_discover`, a bounded, read-only bus inventory.

The routine uses the existing MII management register reads (PHY IDs 2/3,
BMSR 1 twice), and returns success **only when exactly one valid PHY ID**
is observed. No device or more than one PHY returns failure rather than
silently guessing a model/address. Prior clock and exclusive-MAC ownership
qualification is still mandatory; the scan is not an independent proof of
on-card clock configuration or the physical port's exact PHY wiring.

Tested with simulated PHY at address 0, at 31, two devices, zero devices,
and denied ownership/clock; host GCC, ASAN/UBSAN and armeb ARM7 compiler.

IEEE PHYAD five-bit specification: https://standards.ieee.org/
Digi NS7520 MII management register map:
https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/
