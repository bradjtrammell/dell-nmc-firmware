# H910P native hardware boot and MAC/DMA port

This changes the previously diagnostic-only `target/runtime.c` from a
one-shot HTTP check followed by an unconditional halt into a guarded reset
continuation that can run the real Ethernet polling event loop.

## Implemented

- A direct, big-endian ARM7 NS7520 MMIO read/write primitive for documented
  Ethernet MAC, PHY management and RX1A/TX2 DMA registers.
- A writable address whitelist: software cannot use this HAL to write memory
  controller, flash, serial UART, GEN clock, system reset or UPS outputs.
- Independent evidence gates for H910P board identity, exclusive MAC
  ownership, PHY clocks, DMA bus mapping, and an explicit DMA coherence/
  ordering callback. The DMA allocation must fit a qualified bus aperture.
- Native boot coordinator: validates all provisioned VLAN interfaces and
  DMA buffers; enables optional read-only TCP/HTTP with a nonzero boot
  sequence seed; starts MAC after DMA and PHY preflight; polls with verified
  monotonic time, and disables the MAC on clock/network failure.
- The reset routine now invokes an H910P board-port provisioning hook and,
  after successful provisioning, invokes the actual boot coordinator and
  event loop. The ELF reserves network state statically in BSS, not on stack.
- Unit tests cover denied MMIO accesses, DMA bounds, failed preflight,
  clock regression, transport failure, and fail-closed MAC shutdown.
  The runtime/port objects cross-compile for big-endian ARM7TDMI.

## Critical boundary: still not flash-qualified

`ou_h910p_port_prepare` intentionally defaults to failure. This is a weak
link-time BSP hook for an actual H910P implementation that will need verified
installed RAM and DMA mappings, physical PHY clock bring-up and identity,
MAC address, real monotonic timer, hardware-sourced TCP seed, stable
settings partition and a validated UPS physical interface.

The code does not magically supply these physical measurements; setting a
boolean in a test fixture is not hardware validation. A production port
also needs a watchdog/boot failure recovery strategy and approved method
for preventing management lockout if trunk VLAN policy is misconfigured.

**Never flash the research image produced by this branch to a functioning
UPS management card.** Independent recovery and full UPS telemetry/control
qualification remain necessary. `main` remains the existing stable code.

Register addresses: Digi NS7520 Hardware Reference Rev G Ethernet,
DMA, GEN, and Memory Controller chapters.
https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/

## Physical startup evidence is mandatory (new gate)

`ou_board_runtime_start` now requires independent evidence of the
**actual installed H910P board**, the actual Ethernet/PHY wiring,
and an **independently recoverable boot path** before activating the
NIC or DMA. A passive register snapshot deliberately initializes these
fields to zero, even if the SoC revision and DRAM decode look correct.

The future board adapter must supply genuine verified evidence after
board qualification. Do **not** set these flags merely because generic
NS7520 registers read successfully; a real flash/install approval remains
separate. The runtime still defaults to refusing the unqualified port.
Host tests check missing evidence independently and verify no NIC startup.
