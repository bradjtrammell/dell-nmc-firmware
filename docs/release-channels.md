# Firmware release channels

The repository has exactly two long-lived branches. Do not create additional development/release branches.

| Channel | Git branch | Purpose |
| --- | --- | --- |
| **Release** | `main` | Locked stable baseline **v0.5.10**, Dell-derived firmware with the integrated custom UI |
| **Beta** | `dev` | **v0.6.x** independent firmware replacement development and testing |

The v0.5.10 release is retained without changes until a separately built, tested and hardware-qualified v0.6 release is explicitly promoted. The earlier v0.5.11 beta work remains a source of historical/UI compatibility code on `dev`; it must not be represented as independent firmware.

The v0.6 target is defined in [ADR-0002](adr-0002-v06-independent-firmware.md). It replaces Dell's NMC application and XML CGI management transport with an independently built native firmware, typed JSON API, native management UI, security, notification transport and a proven firmware update/recovery path. Boot ROM or bootloader replacement depends on verified board capabilities and is not assumed.

Published firmware uses actual BIN release assets, integrity checks and explicit hardware-compatibility declarations. Main is the stable channel; dev prereleases are betas. Existing firmware's release/beta selector provides GitHub **discovery**, not an on-device automatic installer or rollback. Do not advertise v0.6 update installation until it is implemented and validated.

No v0.6 BIN should be distributed as flashable before the board CPU/flash map, independent boot process and recovery method are proven. Always distinguish host-only simulation, offline image validation and live H910P acceptance.

**Security:** Checksum alone does not establish authenticity; use a signed manifest and image signatures for the native updater. Never embed GitHub credentials or webhook secrets in firmware releases.
