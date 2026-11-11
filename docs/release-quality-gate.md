# Offline release integrity and publication

The H910P management card has a strict factory image limit of 1,517,029 bytes. Never bypass the limit or treat a boot-header match as evidence of flash safety.

The offline \`tools/release_preflight.py\` checks the independently supplied SHA-256, original NET+OS boot header and CRC, the original factory image ceiling, both native landing pages, the static startup scripts and their JavaScript MIME types. The resulting JSON explicitly reports that hardware boot and rollback were not tested.

Run from the repo with the actual BIN from the matching engineering package:

\`\`\`bash
python3 tools/release_preflight.py H910P_Custom_UI_v0.5.10.bin \
  --sha256 e3adea74aad966c15e87f9fdb7407bb9d9d810e20158da54bad639466f70b8bb \
  --json-output build/release-preflight.json
python3 -m unittest test_release_preflight.py test_release_publisher.py -v
\`\`\`

\`tools/publish_release.py\` uses the authenticated GitHub CLI installed on a release workstation to attach an actual standalone BIN asset. It rejects wrong channel/version combinations, empty release notes, checksum mismatch and stable releases without explicit \`--hardware-approved\`. It refuses to overwrite an existing release.

\`\`\`bash
python3 tools/publish_release.py H910P_Custom_UI_v0.5.10.bin \
  --sha256 e3adea74aad966c15e87f9fdb7407bb9d9d810e20158da54bad639466f70b8bb \
  --channel main --notes RELEASE_NOTES.md --hardware-approved --dry-run
\`\`\`

Remove \`--dry-run\` only when actual deployment acceptance is documented and \`gh auth status\` succeeds. Development beta assets use a filename ending \`-beta.N.bin\` and \`--channel dev\`.

The scripts do not create secrets, update the live NMC, prove external webhook delivery, or provide on-card transactional firmware installation. Never publish untested code as a stable release because its offline integrity checks passed.
