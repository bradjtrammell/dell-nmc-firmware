# v0.5.6 — root cause isolation and compatibility restoration

**Context:** Physical H910P v0.5.4 produced "Unable to load the custom interface", while the original Dell UI was blank. v0.5.5 still failed on the card despite final-image payload decompression passing offline.

## Evidence

- v0.5.2 successfully presented authenticated UI routes in the hardware screencast. The v0.5.2 builder put **all payload script tags in the native index**, then the bootstrap last.
- v0.5.3-v0.5.5 switched to **one script tag that dynamically injected 17 more script resources**, awaiting each `onload`. This introduced a new boot dependency that had only been simulated locally. The physical-card failure stage is not observable without live HTTP responses; a precise failing resource or MIME header has **not** yet been proved.
- v0.5.4 also blanked active synoptic resource strings after exporting its temporary test assets. Its final BIN was demonstrably corrupted. v0.5.5 fixed the overwrite, but did not revert the loader change.
- v0.5.3-v0.5.5 explicitly blanked `/default.htm`, `/menu.htm`, `/toolbar_main.htm`, `/html/mgeweb.css`, and `/mgeups/default.htm`, explaining the independently reported blank legacy interface.

## Corrective engineering

1. Replaced the async request chain with static parser-ordered `<script>` references; reduced the package from 18 scripts to **16**, fitting native `/index.htm` (713 bytes) with a non-JavaScript firmware recovery link.
2. Converted gzip transport from base64 to Python base85, adding a deterministic JavaScript decoder and explicit encoded-size validation; static compressed payload itself remains unchanged in meaning.
3. Removed the five critical OEM entry/menu/toolbar/stylesheet resources from retirement; final firmware bytes equal OEM for these URLs.
4. Rebuilt and verified the output from both factory Dell ZIP and exact previous v0.4.2 BIN; resulting SHA-256 is identical.
5. Added real browser parser-order tests that fulfill resources extracted from final firmware, including injected missing-resource failure; added read-only live-card HTTP byte/MIME probe and unit tests.

## Limits

This is evidence-backed remediation of the identified build and boot architecture defects, **not proof of on-device success**. The NMC's physical HTTP server cannot be reached from the build environment. Its actual GET responses and behavior after upload must be checked in a controlled hardware qualification before calling the issue closed. Some OEM Help and synoptic presentation resources remain repurposed, so the original UI is not guaranteed full parity.
