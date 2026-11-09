# H910P v0.5.1 — Engineering status

**Release state:** offline build candidate; no verified physical H910P installation. Base firmware remains Dell `01.18.0005`. Previous physical v0.5.0 deployed, but current 0.5.1 must be separately qualified.

## Hardware-specific findings

Inspection of the actual OEM 01.18.0005 decompressed application descriptor establishes exactly four writable email recipient groups: `Email.Email[0]`, `Email.Email[1]`, `Email.Email[2]`, `Email.Email[3]`. Each contains fields `System.Email[n].Recipient` (`Email:99`), `System.Email[n].Selected` (`Boolean`) and many independent `System.Email[n].EventList.*` selectors. Existing SMTP fields include `System.Network.SmtpServer.HostName` and `.SmtpPort`, plus authentication credentials; user-visible senders and subjects are in `System.Email.*`. This informs the new UI model; only actually RW-advertised fields may be written.

## Implemented source

- Genuine native resource bootstrap: full embedded CSS/JS, all scripts present, offline decompression, no CDN.
- Branded upper-left Dell mark; sign-in form verified against OEM HTTP Basic protected GET; state/credentials in RAM only; client-side lock.
- Dashboard quick navigation and recorded-event feed; existing telemetry, gauges and visual power path.
- Four-recipient manager; modal create/edit/disable; event toggles; per-field readback; partial failure reported; SMTP settings.
- Integrated ntfy, Monita and Discord **email-routing guidance**, not installed service connectors. Actual mail delivery still requires the OEM card SMTP transport and correctly configured receiver/bridge.
- User-facing firmware maintenance guidance. UI-only GitHub updater has no installer because flash storage, static-serving, transactionality and rollback are not established.

## Storage and deprecation impact

- Retired **47,762 bytes** of fixed string content from old synoptic and legacy Help resources, without touching the original uploader or executable handlers. These legacy visuals/help screens cease working; this is explicitly part of the owner-approved single-UI transition.
- Application remains **2,757,180 decompressed bytes**; new BIN **1,515,874 bytes**, original OEM **1,517,029 bytes**. Valid NET+OS CRC; native entry and all descriptor callback pointers preserved.
- This does NOT prove spare flash filesystem capacity for ADR-0001 asset updater.

## Validation

- 18 firmware format/allowlist tests: passed.
- 4 optimized compressor tests: passed.
- 4 embedded-resource/entry tests: passed.
- Dashboard, all 15 routes, login rejection/success/lock, no unauthenticated telemetry, event filtering, recipient modal create/save/readback and power confirmation: simulated browser tests passed.
- The v0.5.1 BIN matches byte-for-byte when built from OEM ZIP or verified v0.4.2 BIN.
- Physical hardware: NOT tested. Actual firmware uploader acceptance, live XML/device behaviors and mail delivery: NOT verified.

## Authentication boundary

The UI form validates credentials against the existing protected Dell `/app_upload.htm` resource. This is **not new firmware-side authentication** and does not change per-resource server ACLs. Client-side Lock is not a revocation of any Basic auth challenge/cached browser credential. Plain HTTP sends Basic authentication in a recoverable form on the local network. Restrict management access to trusted networks until a verified secure transport is deployed.

## Blocking work

1. Live hardware login and read-only page verification.
2. Live NMC email recipient configuration form/readback before enabling any alert action on a production card.
3. Real SMTP transmission and Monita/ntfy receiver acceptance checks.
4. Standalone signed Flash0 asset installer with verified capacity, file-serving and rollback.
5. Completion of native weekly schedule, shutdown clients, SNMP and advanced UPS controls to full parity.
