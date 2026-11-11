# OpenH910P v0.6 — independent firmware foundation

**Status: native ARM7 core compiling; host-only HTTP/JSON simulator functional; NOT flashable firmware.**

This directory is deliberately separate from the stable Dell-derived v0.5.10 firmware. No Dell proprietary application code, Dell UI assets, RomPager resource table or XML management transport is linked into this project. This is a new codebase with a clean native JSON transport model. The native board support package, flash driver, UPS serial protocol, watchdog setup, secure sessions, persistent configuration and recovery boot validation are **not implemented**.

## Design

The API response engine in `src/core.c` is **freestanding C99**: it contains no Linux, TCP/IP, heap allocator, clock or filesystem dependencies. It can be compiled directly for big-endian ARM7TDMI, subject to eventual hardware BSP integration. The `host/server.c` adapter is a host-only POSIX HTTP server that binds exclusively to `127.0.0.1`. Both hosted API and ARM compiler use the *same* C core. All JSON responses are marked `"simulated": true`; there is no physical UPS connectivity.

Routes implemented: `/api/v1/health`, `/api/v1/device`, `/api/v1/capabilities`, `/api/v1/ups/status`, `/api/v1/ups/measurements`, `/api/v1/ups/outlets`, `/api/v1/events` and a metadata-only `/api/v1/configuration/fields` endpoint with pages `/api/v1/configuration/fields/0` through `/18`. GET and HEAD are supported. Any mutation returns **405 READ_ONLY** and **cannot** reach the card. There are no XML endpoints. The GUI serves the same-origin API without third-party assets.

The legacy **302-field** schema is now compiled into the freestanding ARM7-ready C core and exported through a paginated, metadata-only JSON API. The browser's configuration catalog supports searching all 302 definitions without reading any configured values. The 420-object inventory is separately documented at `../docs/v06-oem-capability-inventory.json`, but is not exposed as live device values. Regenerate `src/schema.c` from that sanitized contract with `python3 v06/tools/generate_schema.py docs/v06-oem-capability-inventory.json --output v06/src/schema.c` from the repository root. The generated file is checked into Git so the standalone source package builds without a copy of the original Dell XML. New v0.6 JSON endpoints have typed units and a defined schema version.

## Build and test (Ubuntu/Debian/Linux x86_64)

Requirements: C99 compiler, GNU Make, Python 3.10+. Optional ARM check: LLVM clang 17 or compatible `armeb-none-eabi` target support.

```sh
cd v06
make clean all
make test
make arm-check
./build/openh910p-sim --port 8760
```

Open `http://127.0.0.1:8760/` from the **same machine**. Ctrl+C stops the simulator. The server does not listen on LAN addresses. To run on another port use `--port 8761` with a valid TCP port number. The website is served from `v06/web`, so start the program with `v06` as the current working directory.

`make arm-check` emits only `build/core-armeb-arm7tdmi.o` and `build/schema-armeb-arm7tdmi.o`, checks ELF32 big-endian ARM relocatable format and does **not** produce a device-installable BIN. It is a core target compatibility test, not a claim of full BSP or boot support.

## Security and operational limitations

- Development server only: single-process, sequential accept loop with a 3-second socket timeout and 8 KiB header limit; no TLS, sessions, remote connections or role-based authentication. It is not appropriate for deployment on a public or production network.
- Loopback-only binding; no CORS; restrictive CSP, no-store caching, nosniff, deny framing; fixed static allowlist prevents arbitrary file reads. No credentials or secrets are accepted or persisted.
- No hardware side effects: no device discovery, serial port usage, firmware uploads, UPS commands, scheduled tests, network settings or flash writes.
- Numerical values come from deterministic **test fixtures**, never the attached H910P.
- Physical telemetry, configuration writes, outlet control and firmware installation explicitly advertise `false` capabilities.
- The source is authored for future open-source publication, but no project-wide license has yet been selected; do not assume the existing repository is already open-source licensed.
- The eventual device firmware will require a proven bootloader recovery path and validated memory map before even an experimental flash is allowed.

## Tree

```text
v06/
  Makefile
  README.md
  include/openh910p.h       Native API contract and simulated snapshot
  include/ou_schema.h       Native metadata field shape
  src/core.c                Freestanding C99 JSON serializer + router
  src/schema.c              Generated native 302-field schema, metadata only
  host/server.c             Loopback-only POSIX HTTP/static-asset adapter
  tools/verify_arm.py       Inspects ARM7 big-endian ELF object
  tools/generate_schema.py  Deterministic sanitized contract -> C generator
  web/index.html            New branded management interface
  web/style.css             Independent visual design tokens and layout
  web/app.js                Same-origin JSON client with error states
  web/favicon.svg           Original device mark (no Dell branding)
  tests/test_core.c         Native C routing/serialization/safety tests
  tests/test_http.py        HTTP API, security, UI delivery integration tests
  tests/test_browser.py     Chromium UI checks at 1440, 768, 390, 360 px
  tests/test_schema_gen.py  Generator integrity and field safety tests
  docs/visual-identity.md  Locked v0.6 design brief
  docs/api-v1.md           Read-only JSON schema and safety policy
```

## Next gates

1. Reconstruct the actual flash erase geometry, chip selects, clock and RAM controller settings from the two OEM images.
2. Verify bootloader rescue access during an approved maintenance window or on a spare, recoverable card.
3. Identify physical NMC↔UPS transport, implement native **read-only** driver and compare device measurements against the existing card without any mutating commands.
4. Add hardened credentials, local HTTPS, safe configuration persistence, Monita/ntfy/Discord/SMTP transport and model-specific control operations only after underlying hardware interfaces are verified.
5. Add signed firmware release validation, hardware compatibility and proven recovery before producing a flashable `.bin`.
