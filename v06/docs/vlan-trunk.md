# Native single-port 802.1Q trunk and multi-network controls

The H910P has one Ethernet port and one separate serial port. The single
Ethernet interface can handle many logically independent VLAN interfaces on
a properly configured 802.1Q switch trunk. This does not make the serial port
a network port or enable cross-VLAN forwarding.

## Implemented in the independent ARM7 code

- Up to 64 configured interfaces, each with its own VLAN ID (1..4094),
  IPv4/prefix, optional gateway metadata, allowed service mask,
  up to eight explicit CIDR allow-rules, local-subnet policy, egress
  802.1p PCP, optional DEI allowance and per-interface packet counters.
- Native (untagged) VLAN optional; tag-only trunk supported, with
  explicit opt-ins for tagged native VLAN and priority-only VID 0 tags.
- Incoming frames are assigned by exact VLAN ID before address matching.
  Unknown, double-tagged/QinQ, 4095, DEI-disallowed, or too-large frames
  are rejected. Unconfigured native/untagged traffic never falls back to a
  tagged control VLAN.
- ARP and ICMP echo replies are generated using the correct per-VLAN IP
  and MAC. Replies preserve VLAN ID and use configured egress PCP instead
  of trusting inbound priority markings. No routing or bridging.
- Separate service flags for ARP, ICMP, HTTP read, SNMP, UPS control and
  firmware updates, all additionally restricted by source-CIDR policy.
  Only ARP/ICMP are implemented by the current packet path: future
  HTTP/TCP and control implementations MUST enforce these service flags
  again after authenticated session validation.
- The actual MAC configuration uses the documented NS7520 maximum frame
  limit of 1522 bytes including FCS for VLAN-tagged Ethernet. The DMA
  interface handles 1518 bytes with the FCS stripped, and requires
  independently verified bus mapping and cache coherency.
- DMA RX1A -> per-VLAN demultiplexer -> ARP/IPv4 -> DMA TX2, plus
  managed MAC activation with MAC identity, frame length and
  hardware initialization ordering checks.

## Example (illustration, not device defaults)

A tagged-only switch trunk could carry:
- VLAN 100: management, 192.168.100.0/24
- VLAN 200: monitoring, 10.20.0.0/24
- VLAN 300: UPS control, 10.30.0.0/24
- VLAN 400: automation, 10.40.0.0/24

Each VLAN must be explicitly configured. A future admin UI should expose
VLAN interfaces, PVID/native selection, per-VLAN IP settings, permissions,
allowed source ranges, PCP, counters, and management lockout recovery.
Never configure VLAN 1 as a fallback behind the user's back.

## Remaining physical firmware work

This does NOT itself initialize the H910P's physical PHY clocks, installed
MAC/DMA address map, TCP/IP/HTTP listener, native authentication, persistent
policy storage, actual UPS bus driver, browser UI, or flash-recovery path.
The firmware image is not yet an operational hardware release.
QinQ (double tagging) is deliberately blocked, since it requires a
separate 1526-byte MAXF policy and validation against VLAN hopping.

Digi NS7520 Hardware Reference MAXF register and IEEE 802.1Q format:
https://hub.digi.com/dp/path%3D/support/asset/ns7520-hardware-reference-manual/
https://www.ieee802.org/1/files/public/docs2009/ba-cgunther-vlan-tagging-0309.pdf
