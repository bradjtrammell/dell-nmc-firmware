# Native firmware HTTP/1.0 and HTTP/1.1 wire encoder

The freestanding module src/http_wire.c translates one bounded complete
HTTP request to one JSON HTTP response using the existing native ou_dispatch
core. No POSIX, heap, OEM XML, or external HTTP library required.
Compiles for big-endian ARM7TDMI.

It is NOT yet an Ethernet driver or TCP/IP listener. Hardware network
enablement must supply verified NS7520 MAC/PHY DMA, TCP/IP framing and bounded
connection lifetime before calling this code. The Linux simulator retains
its existing loopback-only adapter.

## Limits and security behavior

- Maximum 4,096 request bytes and 32 headers; 4,096 JSON bytes output.
- Supports HTTP/1.0 and HTTP/1.1; HTTP/1.1 requires exactly one Host.
- Rejects duplicate Host/Content-Length, Transfer-Encoding, folded headers,
  unexpected body or pipeline bytes, malformed request paths and GET bodies.
- Only GET and HEAD access the read-only native /api/v1/ resources.
- Other methods return 405, with all power/config/firmware writes disabled.
- Accurate Content-Length, HEAD without body, Connection: close and no-store.
- Restrictive content security policy, no CORS, MIME nosniff and no framing.
- No auth/session/TLS exists yet. Never expose directly on a production
  network until authentication and actual NIC/TCP integration are verified.

make test, make arm-check and make board-sanitize include this module.
