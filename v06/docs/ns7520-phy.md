# NS7520 MAC MDIO / PHY driver

This is native freestanding C99 code for the NS7520 Ethernet controller's
MII-management interface, not just a register decoder.

It implements a synchronous IEEE 802.3 Clause 22 MDIO read transaction with
the chip's actual MCMD, MADR, MRDD and MIND register addresses. It reads PHY
ID1/ID2 and the twice-sampled Basic Mode Status Register to determine link.
It handles BUSY/NOTVALID, prevents a competing scan transaction, bounds
polling to 1024 cycles, zeroes results on failure and clears MCMD after all
initiated transactions.

Only the expected register writes occur; no PHY configuration or network
packet transmission. On-device access requires the MAC's TXCLK/RXCLK to
already be valid, an exclusive MAC owner, and an independently verified MMIO
adapter. This module does not invent the NMC's PHY model or address.

**Not a complete Ethernet driver:** TX/RX DMA descriptors, EFE enablement,
PHY startup clock configuration, network frame handling, ARP/IPv4/TCP,
authentication and the actual H910P runtime integration remain separate
implementation work. No direct-MMIO adapter is installed yet; the module is
included in the native ARM7 ELF and tested against a strict register emulator.

Digi NS7520 Hardware Reference Rev G, Ethernet register map Table 52,
pages 156-157, including the explicit MAC clock warning:
https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/

Validation:
- Native C99 host tests: correct register sequence, identity/link extraction,
  no-writing-on-missing-clock/ownership, invalid addresses, all I/O failure
  stages, bounded stuck-BUSY handling and cleanup.
- AddressSanitizer + UndefinedBehaviorSanitizer.
- Clang armeb-none-eabi big-endian ARM7TDMI object compilation.
- Linked into research-only ELF, not flash-qualified.
