# Persistent VLAN interface configuration

The single-port 802.1Q trunk supports up to 64 VLAN interfaces. This module
serializes the complete physical MAC, trunk/native flags, IPv4/prefix,
optional gateway metadata, service permissions, PCP, DEI, local-subnet
policy and eight source CIDR ACL entries per VLAN, with a deterministic
versioned binary representation. There are no pointers, host endian values,
variable struct padding, or sensitive default credentials in the payload.

Format: 24-byte OUVL header, version 1, count, flags, native VID, six-byte
MAC address, big-endian total length, CRC32; 59 bytes per VLAN interface.
CRC32 is calculated over the whole record with its CRC field zeroed.
The largest configuration is 24 + 64*59 = **3,800 bytes**, below the
existing 4,096-byte journal payload ceiling. Unsupported versions, reserved
flags, duplicate VLANs, invalid service/ACL/mask/ID settings, truncation,
CRC errors and noncanonical unused ACL bytes are all rejected.

The versioned payload can be saved/loaded via ou_vlan_journal_save/load,
which call the preexisting two-slot NOR journal (power-loss recovery,
write verification, commit-marker-last) instead of implementing another
flash writer. The existing journal refuses writes until the physical
flash erase geometry, dedicated slots and explicit write enablement are
verified. This code **does not install a real H910P flash backend**.
The load path clears invalid output and metadata; it does not apply a
corrupt or partial policy to an operating Ethernet interface.

Software-only tests cover 1..64 configured VLANs, policy round-trip,
CRC tampering, truncation, version mismatch and write-denied journal
conditions. Real flash geometry and staged admin rollback are still
required before live VLAN configuration can be exposed through the UI.
