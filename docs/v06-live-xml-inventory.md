# H910P v0.6 — live schema inventory

**Source:** three original read-only HTTP response captures from the owner's H910P, received November 11, 2026. The raw product, descriptor and object XML files and their headers are **private** and are not stored in Git.

## Validated evidence

| Area | Finding |
| --- | --- |
| Network-management firmware | Dell NMC `01.18.0005` |
| OEM product-family label | `Mosaic 4M 16M` (not verified flash/RAM chip capacity) |
| UPS family | Dell UPS LI R 1920 |
| Separate UPS version reported | `01.08.0001` (not the NMC firmware version) |
| Web server | Allegro RomPager 4.01 |
| Configuration groups | **24** |
| Configuration fields | **302**: RW 290, RO 8, WO 4 |
| Captured GET_OBJECT objects | **420**: System 270, UPS 118, Environment 32 |
| Descriptor field coverage | **302/302** fields represented in GET_OBJECT |
| Additional observed objects | **118**, not listed as configurable |
| OEM field type families | Boolean 158, Time 46, String 37, List 24, Integer 19, Password 5, IPv6 4, Email 4, IPv4 3, Float 2 |
| OEM conditional type expressions | Present (e.g. thresholds involving `$TLow`, `$HHigh`), must be preserved |
| Service security hints | OEM product advertises `GET_OBJECT` and `GET_CONFIG` as `security="none"`, and SET operations as `security="basic"`; this is an advertisement, **not** a live unauthenticated access test |
| HTTP response media type | Descriptor and GET_OBJECT returned **text/plain** despite containing parseable XML |
| Configuration records | 6 hidden OEM groups are represented and remain indexed in the sanitized contract |

The `Mosaic 4M 16M` string also appears in older, published Dell protocol examples across hardware and firmware revisions. It **does not establish** that 4 MiB flash and 16 MiB RAM are physically installed. Verify these independently from board/bootloader evidence before constructing an ARM linker script.

## Four declared write-only fields

- `System.Network.HostName`
- `System.Network.IPAddress`
- `Environment.Temperature.Offset`
- `Environment.Humidity.Offset`

All four names appeared in the captured read response despite their descriptor access code, so v0.6 should not copy OEM permission semantics blindly. Every native endpoint requires explicit authorization and a tested read/write permission model.

## Security and privacy

The captured GET_OBJECT response contained System settings, including configured access-control and network-management fields. The raw captures must remain private. The published `v06-oem-capability-inventory.json` contains **names and schema definitions only**: no object values, passwords, communities, device addresses, hostnames, contacts, serial numbers or recipient addresses.

Do not interpret `security="none"` as proven unauthenticated accessibility without a separate controlled read-only test. The native v0.6 JSON API must enforce access control on the server regardless of what controls are hidden in the browser.

## Deterministic reproducibility

Use the standard-library-only `tools/extract_live_contract.py` with three already saved XML files named `product.xml`, `descriptor.xml`, and `ups.xml`. It **does not** contact the NMC, transmit, modify device settings or flash firmware. It refuses duplicate object names, invalid XML roots, oversize files, unexpected paths, invalid type syntax, or overwriting existing output.

Run: `python3 tools/extract_live_contract.py /path/to/private/capture --output v06-oem-capability-inventory.json`.

Test: `python3 -m unittest test_extract_live_contract.py -v`.

The generated file is an **OEM migration inventory**, not a running v0.6 JSON API. It is sufficient to begin a type-aware host simulator and independent management API contracts; it is insufficient to certify bootloader recovery, physical RAM/flash size or native UPS control protocols.
