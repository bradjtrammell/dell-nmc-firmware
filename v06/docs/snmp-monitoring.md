# H910P native SNMP monitoring

The independent native firmware now contains a **bounded, read-only SNMPv2c BER GET/GETNEXT codec**, integrated with the device service's validated UPS telemetry. It does not yet bind UDP/161 or receive production requests; that requires a trusted-source ACL, independently provisioned community credentials, and a verified network interface. The platform's more secure **SNMPv3 authPriv** remains required before broadly enabling network-accessible monitoring.

## Implemented protocol behavior

`src/snmp.c` exposes a single `ou_snmp_respond` function. It parses definite-length BER message/varbind boundaries and supports no more than eight varbinds per packet (maximum datagram: 768 bytes). It verifies the SNMPv2c version, community, request ID, GET/GETNEXT method, zero error fields, canonical OIDs, and NULL request values. Invalid packets are rejected. SNMP SET is denied and **never** forwarded to UPS power controls or configuration. UDP listeners, GETBULK, SNMPv3 USM, SNMPv1, and outgoing trap transport remain outstanding.

The caller's access policy requires **all** of a verified network ACL, qualified community store, isolated trusted network and a positive community authorization callback for each request. A denied request produces **no response** or UPS data. Community secrets are not included in events, HTTP JSON, or diagnostics; they are accepted only in the request/response protocol boundary. SNMPv2c community remains inherently plaintext on the wire, so the device must not offer it on untrusted networks.

## UPS-MIB telemetry mapping (RFC 1628)

| OID | Value |
|---|---|
| `1.3.6.1.2.1.33.1.2.1.0` | Battery status, normal or low |
| `1.3.6.1.2.1.33.1.2.3.0` | Estimated remaining runtime, minutes |
| `1.3.6.1.2.1.33.1.2.4.0` | Battery capacity, percent |
| `1.3.6.1.2.1.33.1.3.3.1.3.1` | Input voltage, integer volts |
| `1.3.6.1.2.1.33.1.4.1.0` | Output source, normal/battery |
| `1.3.6.1.2.1.33.1.4.4.1.2.1` | Output voltage, integer volts |
| `1.3.6.1.2.1.33.1.4.4.1.5.1` | Output load, percent |

The seven OIDs are returned **only from native telemetry that has passed the existing verification and staleness guard**. Absent or stale measurements return `noSuchInstance` (and a missing object returns `noSuchObject`); the agent does not invent simulated values. GETNEXT returns the next known OID or `endOfMibView`. The current mapping is deliberately limited to fields actually provided by the validated snapshot and is not claimed to be the complete UPS-MIB implementation.

## API, UI and qualification

`GET /api/v1/snmp/capabilities` lists the exact seven OIDs and declares the compiled read-only codec, disabled listener, no provisioned community, unverified source ACL and missing SNMPv3 authPriv. The SNMP page displays this state and the real OID inventory, alongside existing alert/trap configuration planning. All HTTP writes remain 405.

Unit and native-service tests exercise wire response parsing, GET/GETNEXT, unknown OID, malformed BER, unauthorized community, missing ACL, SET refusal, native-only measurement gating and telemetry withdrawal on disconnect. ARM7 big-endian link and sanitizers remain mandatory. **No physical card, SNMP listener, or UPS power interface is modified.**

## Bounded GETBULK MIB walking

The native BER protocol engine also supports the SNMPv2c
`GetBulkRequest-PDU` (`0xA5`) with canonical non-negative
`non-repeaters` and `max-repetitions` fields. Non-repeaters
return one lexicographic successor. Repeaters advance through
the known UPS-MIB OIDs in row-major order, returning
`endOfMibView` after the final known object. Repetitions and
response amplification are **always capped at eight returned
varbinds**, irrespective of request values. Zero repetitions
and no non-repeaters legitimately produce an empty result.
Malformed, unqualified or unauthorized requests never expose
telemetry; SNMP SET remains blocked.

This extension does not add unsupported UPS objects, enable
UDP/161, authenticate SNMPv3, or configure communities. Host
wire tests cover bulk walks, non-repeaters, zero/large repetition
counts, end-of-MIB, missing physical telemetry, wrong community,
and invalid BER. Big-endian ARM7 and sanitizers remain required.
