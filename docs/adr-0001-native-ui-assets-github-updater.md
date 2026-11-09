# ADR-0001 — Native UI asset-store updates from GitHub

**Status:** ACCEPTED AS TARGET ARCHITECTURE, NOT IMPLEMENTED  
**Decision date:** 2026-11-09  
**Owner-approved:** Option A first; Option B (full-firmware GitHub updates) reserved for a later phase.  
**Current hardware baseline:** Dell H910P 01.18.0005 with custom UI 0.4.2 successfully written to FLASH, rebooted and served from `/index.htm` on a real card. The dashboard renders; other management pages have been reported empty/incomplete.

## Decision

1. Retain the working UPS-management backend, authentication, networking, firmware-upload handler, and firmware boot/update logic. **Do not retain the complete legacy Dell web frontend** once the replacement UI has verified feature parity. Reclaim its HTML/CSS/JS, frames, help and redundant graphics only after classifying their actual firmware references. Preserve a **minimal static, firmware-resident recovery/upload page**, independent of the modern UI bundle and external services.
2. Move changeable custom HTML, CSS, JavaScript and logo assets into a **persistent NMC-local asset store** *only after* the hardware's usable flash region, mount/format, resource serving, and transactional file-writing primitives are established.
3. Provide a fully native management **Updates** page that discovers versioned, integrity-checked UI packages published through GitHub and installs them to that store. No Docker, external app, cloud back-end, or proxy is a runtime dependency. A user-initiated browser download-and-upload bridge is acceptable *only as a compatibility fallback* if the old NET+OS TLS stack cannot fetch GitHub securely itself; it must still install persistently on the NMC.
4. The firmware-resident management entry must have a separate **static safe-mode / factory-reflash route** (prefer reusing the existing `/app_upload.htm` RomPager resource and `/Forms/app_upload_1` callback, pending validation). The recovery path must work with JavaScript disabled and with custom UI assets missing or corrupted. Do not make firmware recovery depend on a downloaded asset, GitHub availability, or the original full Dell interface.
5. **Do not** implement an updater button which links to GitHub but cannot write a verified asset to the card. Until every required primitive works on the real NMC, display the updater as **unavailable/unsupported**, not functional.
6. Postpone **Option B**: fetching and installing full firmware images from GitHub. It is a separate higher-risk feature and must not be enabled accidentally by an asset-update path.

## Legacy UI retirement policy (owner approved 2026-11-09)

- **Target:** one complete modern UI, not a permanent `/default.htm` Dell UI alongside `/index.htm`.
- **Rollback mechanism:** offer a local upload of the *user-supplied* unmodified Dell firmware BIN through the existing NMC update engine. Firmware recovery is a small, self-contained built-in page and must not require the large stock Dell UI.
- **Preconditions:** demonstrate on a real card that authentication, upload, status reporting, and stock firmware restoration work with the modern app assets disabled. Verify that remaining backend handlers do not reference removed Dell JS/HTML pages, forms, resource descriptors, or static error strings.
- **Limitation:** a firmware reflash via HTTP is only possible if the device has booted and its network, authentication, and update handler remain usable. It is not a recovery mechanism for a non-booting image. No serial/JTAG/bootloader recovery claim until proven.
- **Accounting:** inventory compressed and uncompressed bytes, pointer tables, code/data references, and flash erase boundaries before treating removed legacy pages as free space. Do not treat savings in the compressed BIN as arbitrary writable storage.
- **Staging:** retain the proven v0.4.2 image and original factory archive as owner-controlled recovery references; remove legacy UI resources only after the new page-by-page feature parity and independent rescue-upload testing are complete.

## Basis in actual 01.18.0005 firmware

Offline inspection of the original decompressed application finds `FLASH0/` and persistent file paths including:

- `FLASH0/NetworkSettings.cfg`, `FLASH0/SystemSettings.cfg`, `FLASH0/WeeklySchedule.cfg`
- `FLASH0/ConfigurationData.xml`, `FLASH0/ssl.pem`, `FLASH0/ssl.key`
- Strings for `saveFileInFlash`, `File %s written in Flash with %d bytes`, `/Forms/app_upload_1`, and the Dell firmware-upload state machine.

**What this establishes:** A filesystem-like Flash0 storage API exists for existing configuration/certificate files.  
**What it does NOT establish:** Free space, flash erase boundaries, wear budget, file-name semantics, writes exposed to the browser, or whether RomPager can serve arbitrary `FLASH0/` files at URLs. Existing static website resources live in the firmware application and have RomPager resource descriptors; nothing presently proves they can be overlaid by flash files.

## Required research and implementation gates

### Gate 1 — Non-destructive flash/storage audit

Inspect the flash driver and linked filesystem API in the OEM application; identify device map, Flash0 capacity and allocation, file format, persistence, maximum file size, quota and wear characteristics. Read-only checks on the owner's hardware if needed. **Do not** overwrite any configuration file, erase raw flash sectors, or assume its spare capacity.

### Gate 2 — Persistent static-file serving

Establish a safe native mapping such as `/ui-assets/current/*` to verified Flash0 assets, with correct MIME types, finite maximum size, secure path canonicalization, no traversal, no executable handlers, no overwrite of system settings or secrets, and a fallback to embedded recovery assets if files are missing or invalid.

### Gate 3 — Transactional installer

A private authenticated installation endpoint must enforce exact supported-device/firmware identity, byte limits, file manifest consistency, path whitelist, package cryptographic verification and monotonic version policy. Write to an inactive staging location; verify each asset through readback; atomically activate if available; preserve a known-good version; recover automatically from interrupted writes/power loss. If Flash0 cannot safely support two UI slots, do not claim rollback; choose a verified alternative rather than risking the card.

### Gate 4 — GitHub publication and retrieval

Publish versioned, static UI package artifacts and a manifest. Specify a package format and exact compatibility contract; verify release signature using a firmware-pinned trust key (SHA-256 alone does **not** authenticate a maliciously replaced manifest + asset pair). Fetch release metadata and bytes over secure HTTPS, only using TLS capabilities proven on the card, or transparently route through the user's authenticated browser as a client-side delivery bridge. No GitHub PAT, credential, or secret stored on the NMC for publicly readable releases. Test CORS, redirect, rate-limit and CDN behavior before promising direct-device retrieval.

### Gate 5 — Real NMC acceptance tests

Demonstrate successful installation on physical H910P, persistent availability after cold power cycle, offline management with no GitHub connectivity, full native UI navigation, correct UPS telemetry and commands, update rollback after corrupted image/failed boot, and a safe recovery path. Record firmware/app versions separately.

## Work sequencing

- First: finish real content and device-backed workflows for all management destinations; incorporate the supplied Dell logo as exact white artwork with alpha in the upper right. Preserve the established visual language and a **minimal native firmware-recovery upload page**, not the entire legacy Dell web interface.
- In parallel: non-destructive storage and web-serving feasibility audit; implement the UI asset package builder and manifest validations *offline*.
- Then: wire a real updater screen and native installer only after the hardware feasibility gates are met.
- Later: separate ADR for full-firmware downloads/flash updates (Option B).

## Present implementation state (2026-11-09)

- **Confirmed:** 0.4.2 firmware accepted, written, rebooted; `/index.htm` serves the custom UI.
- **Unverified/not implemented:** writable independent UI asset storage, asset-serving overlay, atomic activation/rollback, GitHub retrieval, updater installation, complete non-dashboard management screens, safe elimination of legacy RomPager pages, and a validated factory-firmware reflash path through the replacement recovery page.
- The currently tested custom UI is **still embedded in the firmware**; changing a file in the GitHub repository does **not** update the running card.
