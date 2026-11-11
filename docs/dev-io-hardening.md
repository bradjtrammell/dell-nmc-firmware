# Device I/O hardening (development)

- Event history retains up to 435 entries, matching the original NMC log capacity.
- Event CSV export now uses authenticated, queued same-origin transport rather than navigation that can prompt for a second browser login. Empty, HTML and oversized responses are rejected before download.
- XML retries reject oversized payloads; retry is limited to specific read-only OEM URLs.
- Config validation rejects integers outside the JavaScript safe integer range and ambiguous IPv4 octets. Power commands are never retried automatically.

Device hardware acceptance remains required before promotion to main.
