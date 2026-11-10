# H910P v0.5.5 — native loader corruption regression fix

## Root cause

v0.5.4 generated packed JS chunks in `/html/synoptic/ajax.js` and
`/html/synoptic/png.js`, then unconditionally erased these same RomPager
static string slots while retiring the legacy synoptic UI. The temporary
`build/served-native-ui/` copies were written BEFORE the erasure, and
all browser tests used those copies. The final compressed BIN therefore
contained 2 missing chunk resources and could not decompress the full UI.

This was reproduced by extracting the **actual v0.5.4 BIN**: both
resources contained only spaces. 45,194 base64 characters survived versus
48,160 expected; base64 decoding failed with an invalid length.

## Fix

- Never retire an active compressed-asset resource.
- Validate each packed asset's byte sequence against its final, re-decoded
  **firmware image** after all cleanup passes.
- Extract and decompress all base64 chunks directly from the final BIN,
  compare byte-for-byte to the source CSS+JS package.
- Add a regression suite validating live resource preservation, complete
  decompression, versioned app content, and asset/retirement disjointness.
- Use deterministic Zopfli gzip 0.4.2 compression (pinned) and drop CSS
  documentation comments only to retain factory image size compatibility.
- Preserve all management runtime code and card callbacks.

## Offline evidence

- 18 firmware parser/rebuild tests passed; previous additional 4 compressor tests passed.
- 4 native-entry tests passed; 3 final-BIN asset regression tests passed.
- The extracted BIN's 17 chunk resources executed sequentially in Chromium;
  login and dashboard rendered without uncaught JS errors.
- Browser UI and configuration/updater simulation suites passed.
- XML diagnostic browser tests passed.
- Final binary length 1,516,805 vs 1,517,029 OEM (-224 bytes).
- CRC, original application size, runtime pointers, and callback tables verified.
- Firmware SHA-256: `bbd5ebce543107dcefe31d36c5365741ebcce0ff968cbe653611ad0470e408cb`.

## Hardware caveat

Actual flash acceptance, NMC HTTP responses, real configuration writes,
UPS power controls and automatic updater rollback remain unverified. A
successful offline browser simulation is not a device-level success claim.
