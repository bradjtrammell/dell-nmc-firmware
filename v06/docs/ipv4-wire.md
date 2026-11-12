# ARM7 Ethernet ARP / IPv4 packet-response implementation

Freestanding, static-memory Ethernet frame processor that handles valid ARP
who-has requests and IPv4 ICMP echo requests and produces bounded ARP/echo
response frames. It validates address targeting, lengths, fragmentation,
headers, and IPv4/ICMP checksums; rejects unsupported packets. Frames are
supplied with CRC stripped, and the future MAC transmit path must add CRC.

The module compiles and links to big-endian ARM7, with host and sanitizers
testing checksums, ARP field layout, source/destination address swapping,
padding, malformed inputs and invalid configuration.

The following are not implemented by this module: NS7520 Ethernet RX/TX DMA,
MAC/PHY initialization, DHCP, ARP cache, TCP/HTTP network endpoint, full
H910P board startup, or physical UPS communication. This is a packet-layer
driver component, not an installable firmware or a complete TCP/IP stack.
