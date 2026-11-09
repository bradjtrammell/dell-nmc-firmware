# Native UI 0.4.1 — Engineering handoff

**Date:** 2026-11-09. **Current source snapshot:** engineering ZIP attached to the current ChatGPT conversation; `dev` branch source files have not yet been synchronized. This document is a tracking record, not a matching source commit.

Built BIN: `H910P_Instrument_Panel_0.4.1_EXPERIMENTAL.bin`, SHA-256 `e63a0ed493a93120dba3d6204669b1f9f877072b914f140a06ae818e96215567`, 1,533,558 bytes. NET+OS application/pointers and callback tables preserved offline; firmware image 16,529 bytes larger than Dell original.

Implemented in bundled source:
- Frame-free dashboard, live XML telemetry, events, output status, instrumentation.
- Data-driven settings: GET `configuration_descriptor.xml`, GET `System.*`, validate individual RW fields, confirmed multipart `Configuration.xml` writes via `Forms/set_obj_2`, verify value by readback.
- Separate outlet power on, restart, and power off based on the descriptor's hidden Control.Outlet command objects, with exact typed confirmation and no false claim of observed completion.
- Single-script RomPager loader fetching compressed resource chunks in order, and selected-navigation class fix.
- Current-UI visual baseline preserved.

Executed: 18 Python firmware tests passed; 10 browser categories passed; 8 configuration/control browser categories passed; embedded JS syntax checks passed. Actual host HTTP navigation test SKIPPED (test environment policy). None of the simulated protocol writes are evidence of successful H910P hardware control.

Still incomplete native parity: weekly shutdown schedule editor, some vendor-specific management screens, firmware upload workflow. Original Dell management links remain for these. Legacy Help resource pages used for embedded assets are overwritten. Exact hardware behavior must be established by operator testing.

The publicly accessible GitHub tree must not redistribute proprietary Dell OEM binaries. The latest engineering package includes OEM files solely for local owner use.
