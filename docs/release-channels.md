# Firmware release channels

The repository uses exactly two active development branches:

| Channel | Git branch | Artifact classification | Purpose |
| --- | --- | --- | --- |
| Release | `main` | Published non-prerelease GitHub Release with an H910P BIN | Approved experimental release baseline |
| Beta | `dev` | Published prerelease targeting `dev` with an H910P BIN | Preproduction device tests and engineering validation |

**Baseline:** v0.5.10 on `main`. Further changes start on `dev`; promotion to `main` requires offline tests, final BIN validation and hardware acceptance appropriate to the release.

The firmware page provides an **Update channel** selector. `Release (main)` queries GitHub's latest published non-prerelease. `Beta (dev)` queries published releases, selecting the first non-draft prerelease explicitly targeting `dev`. The chosen release is displayed only when it contains a compatible H910P `.bin` asset and a GitHub download URL.

This is **release discovery, not automatic installation**. Selecting `dev` never downloads raw source code or offers an arbitrary branch commit as flashable firmware. File transfer still uses the OEM firmware upload workflow, and only the NMC's final FLASH-write status confirms a successful write. No transactional rollback is available.

When publishing a beta with GitHub CLI, set `--target dev --prerelease`. A stable release should use `--target main` without `--prerelease`. Publish the standalone BIN and its SHA-256 digest, and confirm the asset exists in GitHub Releases. The ROM browser is not itself required to access GitHub for normal UPS monitoring.

**Security:** release API and download URLs are trusted for *discovery*, not for firmware authenticity. Manual operators must verify the release's pinned checksum and hardware compatibility before flashing. An asset filename alone is not sufficient proof. Work toward a signed firmware manifest and recovery mechanism continues separately.
