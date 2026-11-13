# H910P independent firmware — project GitHub update pipeline

**Only upstream:** `bradjtrammell/dell-nmc-firmware` GitHub Releases. The native update model has no Dell/OEM channel, firmware fallback, or untrusted mirror. The current experimental native branch remains **RESEARCH_ONLY_NOT_FLASHABLE**.

## Released-package acceptance implementation

`src/update_verify.c` and `include/ou_update_verify.h` provide a freestanding ARM7-compatible SHA-256 implementation and signed-release verification pipeline. No heap, TLS socket, flash IO, filesystem, or updater daemon is required by the verifier itself.

The canonical signed release manifest binds the versioned format, the **exact project repository**, the H910P target identity, semantic version (major/minor/patch), channel flag, image byte size, trusted signing-key ID, and full SHA-256 image digest. The signature bytes are validated by a **separately qualified external Ed25519 verifier**, not by a hardcoded test key or an all-zero/accept-all fallback. The test verifier is deliberately synthetic and is not installed on an H910P.

Before accepting a candidate, the verifier refuses missing/invalid manifest fields, wrong repository or hardware, equal/older versions, oversized images, unsigned data, and missing qualifications for board identity, flash geometry/application capacity, recovery, trusted GitHub HTTPS and signing keys. On success, the image can be streamed into SHA-256 with no heap allocations and is considered byte-verified only if its complete byte count and digest match the signed manifest. Failed or incomplete streams cannot be finalized.

**Integrity verification alone never activates, flashes, reboots, or changes the card.** The signed manifest is a data-contract foundation, not a bootloader or installation operation. A production implementation must keep a proven rollback image and recovery channel, verify trusted root keys and signature algorithms, use durable staging with interrupted-download handling, and test failures/power loss before the flash gate may be enabled.

## Release-check management interface

`GET /api/v1/firmware/update-policy` declares `releaseProvider:"github"`, the pinned repository, H910P board target, SHA-256/manifest-verifier availability and the running development version. Its release lookup is available, but `installEnabled`, `rollbackEnabled`, `automaticUpdateEnabled`, `signatureKeysQualified`, `flashLayoutQualified`, `recoveryQualified`, and `deviceHttpsQualified` remain **false**.

The host-only Firmware Upload page now has a functioning **Check GitHub releases** button using the pinned public GitHub API URL, not a Dell URL and not a user-supplied source. The development host Content Security Policy allows `https://api.github.com` solely for this release lookup; browser credentials and tokens are never sent. Release titles/tags and asset names are inserted as DOM text; links are constructed only under the project's GitHub release path. Draft or malformed release entries are omitted. The UI handles rate-limit/network errors and empty feeds explicitly. Downloading, selecting and installing assets as firmware are disabled.

The repository's Releases API contained **zero published releases at the time of this work**; the interface does not claim otherwise or invent version availability. Real availability must always be rechecked rather than cached as a completed update.

## Verification and limitations

Automated SHA-256 tests use the known `abc` and million-byte `a` reference vectors. The update tests verify an independently computed fixed 4,096-byte image digest, chunked streaming, tampering, missing bytes, wrong board/repository, rollback prevention, missing signature, restricted flash capacity and every hardware/trust qualification gate. HTTP tests verify policy and read-only mutation denials; browser tests check the empty release state, release rendering, GitHub API errors, pinned upstream, and absence of install actions. ARM7 compilation/linking and sanitizer tests are required.

**Not yet complete:** on-card HTTPS GitHub download, independent Ed25519 trust-key provisioning/verification, persistent staged image store, physical H910P flash driver and sector validation, dual-bank or equivalent tested recovery, rollback activation, or a device-resident updater UI. No part of this implementation touches the live card.
