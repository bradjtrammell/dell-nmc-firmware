# H910P 0.2.1 — Engineering verification (2026-11-09)

Branch: dev. Main retains the earlier 0.2.0 baseline.

This iteration is packaged as `H910P_Instrument_Panel_0.2.1_Engineering_Package.zip`, with `H910P_Instrument_Panel_0.2.1_EXPERIMENTAL.bin` as a separately generated firmware image. The binary is not committed to this public repository because it contains proprietary Dell firmware.

Changes implemented in the package:
- Stable SPA view updates preserving keyboard focus and page scroll.
- XML object parsing for text nodes, child VALUE nodes and value attributes.
- Read-only direct `/product.xml` card-identification screen, filtering credential-like fields.
- Ten browser test categories with simulated XML, including identification and secret suppression.
- Compact original RomPager entry HTML to fit the additional embedded script asset.

Executed verification:
- Original and rebuilt firmware decompression/checksum/resource invariants: build passed.
- Python firmware regression suite: 18 tests passed.
- Chromium embedded-resource test: 10 categories passed; no uncaught JavaScript exceptions.
- End-to-end HTTP-hosted Chromium test: SKIPPED because local HTTP navigation was blocked by the execution environment. No actual NMC HTTP serving was tested.

**Not implemented:** A complete native replacement for the original control/configuration forms. The new UI still links to Dell's original management pages for writes. No tested bootloader rollback or hardware acceptance.

Custom BIN SHA-256: `47340cb5324d9591d01c9e1b1e00088c5c81836fad6cd237d6c39ce2e15331cc`.

Source-of-truth for this iteration: engineering package attached in the originating ChatGPT conversation; the GitHub source tree is still 0.2.0 until source files are synchronized. Do not assume this branch represents the 0.2.1 code.
