# Dell H910P NMC Custom UI

Research and development repository for a desktop-first embedded UI replacement of the Dell H910P Network Management Card (NMC), targeting firmware 01.18.0005.

**Status: experimental; do not flash production hardware.** Offline image integrity and interface tests are distinct from hardware acceptance, boot, and recovery validation.

The NMC hosts the web UI directly; no reverse proxy, server, or additional container is required. This repository intentionally excludes copyrighted Dell firmware files and private device configurations. Build tooling consumes the original vendor ZIP supplied by the device owner.
