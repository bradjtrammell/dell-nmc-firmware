# v0.6 — Storage and RAM budget discipline

**Status:** enforced for offline ARM7 research builds only. The physical RAM, flash, protected bootloader blocks, and application partition on the installed H910P remain **unverified**. Stable v0.5.10 on main and the existing management UI are unchanged.

## Real measurements and initial engineering limits

| Item | Bytes | Meaning |
| --- | ---: | --- |
| Dell 01.18.0005 firmware update image | 1,517,029 | Observed image length, NOT total flash capacity |
| Original decompressed Dell application | 2,757,180 | Observed executable size, NOT installed RAM |
| Provisional research raw-binary cap | 1,200,000 | Development policy, 317,029 bytes beneath Dell's update-image length |
| Provisional research RAM span cap | 1,048,576 | Research-only bound, NOT usable physical RAM |
| Research stack reserve floor | 8,192 | No claim of sufficient TLS, network or interrupt stack |
| Real flash/RAM and recovery partition | Unknown | Mandatory hardware-identification gate |

The machine-readable policy is at v06/resource-policy.json. Unknown physical capacities are null, not guessed. Run **make footprint-check** from v06 to verify the actual big-endian ARM ELF allocatable sections, NOLOAD BSS/stack, raw binary size, and linker address spans. The resulting build/resource-report.json explicitly says RESEARCH_ONLY_NOT_FLASHABLE. The --require-production-ready option deliberately refuses to approve deployment.

The measurements exclude the actual NIC, TCP/IP, TLS, concurrent browser requests, persistent configuration, native UPS bus, event backlog, recovery slots, and UI assets, which are **not yet linked**. Every additional capability must be measured when implemented, including peak network/TLS heap and flash wear. The NS7520 development board has documented memory configurations, but those are not proof of the H910P card's installed chips.

## Modern home-lab appliance priorities

- **Fundamental UPS functions:** trustworthy voltage, load, battery, estimated runtime, outlet/segment controls, graceful shutdown, schedules and local logs; fail closed if telemetry is stale.
- **Native JSON interface:** compact typed on-device status/configuration APIs; bounded messages and buffers. Preserve the currently approved UI and replace only its underlying transport when hardware services are qualified.
- **Security:** local authentication and authorization, secured sessions, CSRF protection, signed upgrades, verified recovery and TLS only after measuring handshake RAM/CPU on the real ARM7 device.
- **Integration:** SNMPv3 when practical, email and native Monita/Discord/ntfy destinations, per-event subscriptions and bounded notification retries.
- **Efficiency:** no containers, Linux/Python/Node runtime, external cloud service, unbounded history queues, huge databases, or unnecessary flash writes. Keep the existing browser assets local and optimize their packaging without redesigning them.
- **Safety:** image size must eventually include the entire runtime, UI, configuration migration, signing metadata, recovery reserve and flash erase-block alignment. No production BIN until actual flash geometry and bootloader recovery are proven.

This policy is deliberately conservative and can be adjusted after physical capacity is reliably measured. It is **not** a substitute for chipset/board support or an invitation to flash the research image.

## Offline tests

Run from the v06 directory:

    make clean all test footprint-check

An intentional negative test:

    python3 tools/audit_footprint.py build/native-research-NOT-FLASHABLE.elf build/native-research-NOT-FLASHABLE.dat --require-production-ready

The negative test exits with status 3. No command here contacts or changes the live NMC.
