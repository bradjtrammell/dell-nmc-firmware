# 0.3.0 embedded UI — engineering results

**2026-11-09, experimental build.** Development branch `dev` tracks the engineering status; the separately attached engineering ZIP contains the latest full source and BIN. The GitHub source tree remains at the previously committed version until synchronized—do not treat it as a source match for this BIN.

New implementation in this iteration:
- Stable refresh preserving focus and scroll.
- Enhanced `get_object.xml` parsing.
- Read-only product identification via `/product.xml` with credential-field filtering.
- NMC-native configuration editing driven by `/configuration_descriptor.xml`, gated to RW fields.
- Locally validated, operator-confirmed multipart `objectList` / `Configuration.xml` submissions to Dell's documented `/Forms/set_obj_2` endpoint.
- Independent readback through `get_object.xml` before showing confirmed success.
- Hidden control groups excluded from generic settings forms.

Executed: firmware rebuild and checksum checks PASS; 18 firmware regression tests PASS; 10 embedded UI browser checks PASS; 6 configuration browser checks PASS. The actual HTTP integration test was SKIPPED because local Chromium networking was blocked. Configuration-write tests use simulated NMC responses, not the actual NMC.

Firmware file: `H910P_Instrument_Panel_0.3.0_EXPERIMENTAL.bin`, 1,534,393 bytes, SHA-256 `a9b8f4954cb8fe77da0cba6a2cbb26923da522d05e21fadb2e0e9d4e18fb7269`. Increased compressed image size versus OEM: 17,364 bytes.

Remaining native UI parity gaps: power switching, weekly scheduling, and several legacy management workflows still require Dell's original UI. Twelve legacy Help resources are repurposed by this build. Browser XML, native MIME, boot and hardware protocol operations have not been tested on a physical H910P.

Official Dell M2M contract: https://dl.dell.com/manuals/all-products/esuprt_ser_stor_net/esuprt_rack_infrastructure/dell-line-interactive-rack-ups-1920r_user%27s%20guide13_en-us.pdf
