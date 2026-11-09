# Current Dell H910P NMC UI — visual and functional baseline

**Authority:** the user's November 8, 2026 screencast of the **current deployed UI**. This is the existing product to preserve and surpass, **not** an image of our proposed replacement.

## Review method and limitations

- Recording: 54.161661 seconds, 1906 × 877 native pixels.
- 516 decoded frames inspected across the timeline; every frame was processed to identify the selected navigation state, and representative frames for each screen were inspected at native resolution.
- Frame ranges below correspond to the sidebar's *selected* state; content transitions may precede or follow the selected-state update by several frames.
- UI controls visible in the video are documented as *present*, not claimed tested. The recording does not prove Save/Execute behavior, authentication results, firmware upload success, or network protocols.
- Do not commit unredacted screenshots, user/device credentials, serial numbers, configuration, or copyrighted OEM firmware into this public repository.

## Current visual identity

- Black/charcoal, near-full-width horizontal Dell masthead, approximately 76 px high. Dell wordmark in bright blue at left, white "UPS NETWORK MANAGEMENT CARD" title.
- Fixed black left navigation rail approximately 195–200 px wide, with three text-group headings: **UPS**, **Logs and Notification**, **Settings**.
- Sidebar items use small triangular disclosure-like bullets and a pale ice-blue selected row; headings separated with hairline rules.
- Bright white main workspace with narrow vertical separator, full-width pale-blue section header strips and blue-gray table heading bands.
- Dense operational forms and spreadsheets: narrow, squared-off inputs, native select controls and checkboxes, compact type, crisp thin-gray rules, alternating white/light-gray table rows.
- Main UPS Properties view has a recognizable rackmount-UPS image, large green health indicator, and a green/yellow power-flow schematic with UPS stages and battery connection. Output load and battery are shown as discrete green segmented bars.
- The design uses hierarchy and high information density rather than large rounded cards. Preserve its operational seriousness and navigation clarity, but substantially improve legibility, spacing, icon consistency, affordances, hierarchy, and motion in the new UI.
- No evidence in this recording that the existing UI provides modern animated transitions, real-time charting, or an iframe-free architecture. Do not invent those features as current behavior.

## Frame-by-frame navigation inventory

These are the 15 selected-navigation runs obtained by examining all 516 frames. Frames are 1-indexed.

| Frames | Sidebar destination | Observed content / controls |
|---|---|---|
| 1–182 | UPS Properties | Health, UPS hardware photo, power-flow graphic, status/properties selector, mains, load %, three output states, battery %, remaining runtime, About UPS details including NMC/UPS revisions |
| 183–205 | UPS Control | On Battery, Load Segment 1, Load Segment 2; per-output status, action selector, Off Delay, Toggle Duration, On Delay; distinct Execute and Save |
| 206–228 | Weekly Schedule | Seven weekdays with independent Shutoff Time and Restart Time selects, Save |
| 229–263 | Shutdown Parameters | On-battery remaining-runtime shutdown threshold; system shutdown duration; per-load-segment off/on timing; advanced-parameters disclosure, Save |
| 264–347 | Measurements | Scrollable dated/time-stamped measurements for AC input voltage and frequency, output voltage/frequency/power/load, battery capacity and remaining runtime; Save Log and Clear Log |
| 348–354 | Event Log | Dated UPS events with numeric codes and descriptions, including AC problems and transfer events; Save Log and Clear Log |
| 355–375 | System Log | Management-card startup history; Save Log and Clear Log |
| 376–391 | Email Notification | Recipient list and enable state, attach measurement/event/system logs, periodic reports, per-event selection, test, defaults, SMTP navigation |
| 392–402 | Network | IPv4 address/netmask/gateway, hostname/domain, IPv6 and autoconfig, DHCP, firmware-upload enable, DNS, SMTP settings and credentials |
| 403–416 | System | Contact/location/language, historical/environment recording intervals, Reset Communication, Factory Reset and Keep TCP/IP Parameters |
| 417–433 | Notified Applications | Shutdown clients and NMS membership list, Remove, Utility Failure Test, Shutdown Test, Modify/Add NMS |
| 434–457 | Access Control | Manager login/password, authentication modes, SSL mode, default/custom certificate, PEM/key upload and key passphrase |
| 458–481 | SNMP | Version and port; v1 read-only/write communities and write enable; v3 read-only/read-write users, security modes and notification identity |
| 482–488 | Time | Current NMC time; manual time or NTP, NTP hostname, timezone and DST controls |
| 489–516 | Firmware Upload | Firmware file selection and Upload with explicit warning not to interact during the upgrade |

The frame classification reflects the *video's selected navigation item*; screenshots captured during transitions may display new page content while the previous menu row remains highlighted.

## Concrete behavior that the replacement MUST preserve

1. **UPS status:** Mains/AC status, rack hardware model, graphical power path, load, battery charge/state, estimated runtime, three separately displayed outputs, and hardware/firmware identity.
2. **Outlet operations:** Primary output plus two load segments, current states, action selection, off/on delays, and toggle intervals; distinguish *Execute* from *Save*. Power-changing actions must not be simulated.
3. **Schedules and shutdown:** Weekly on/off scheduling, on-battery automatic shutdown thresholds, shutdown duration, segment sequencing, and advanced configuration.
4. **Measurements and logs:** All original input/output and battery measurement columns; long scrollable history; exports and log-clear actions with confirmation.
5. **Notifications:** Multiple recipients, per-recipient alarm subscriptions, logs as attachments, recurring summaries, SMTP setup, and test operations.
6. **Management:** IPv4/IPv6/DHCP/DNS/SMTP, identity/contact/location, history intervals, notified applications/NMS, utility/shutdown tests, authentication, certificate/key upload, SNMP v1/v3, NTP/timezone, and firmware upload.
7. **Safety:** Preserve firmware callbacks, existing permission checks, and all recoverable manufacturer UI paths until equivalent new workflows are proven. Preserve original firmware control logic.

## New UI visual contract

- **Purpose:** desktop-first, instrument-grade, premium hardware-management UI, hosted **directly by the NMC**. No external service, reverse proxy dependency, cloud assets, iframe, or frameset.
- **Identity:** precise, assured, technical. Reference: professional rack instrumentation with clear Dell-era industrial lineage, modernized rather than copied pixel-for-pixel.
- **Palette starting point:** graphite header `#17191B`, bright workspace `#F7F9FA`, icy section tint `#D8E8F2`, Dell-derived blue `#1786C1`, healthy green `#14834F`, warning amber `#AE6300`, critical red `#B42318`, body text `#17232C`. Final colors require contrast checks.
- **Typography:** practical system sans for UI, tabular monospaced numerals for power and measurements. Do not require network-hosted font files.
- **Layout:** persistent top device/status strip; persistent left navigation by actual NMC functions; full-width dashboard and dense tables where appropriate. Avoid decorative card grids and wide empty gutters.
- **Visual elements:** detailed vector power-flow diagram and outlet topology; legible UPS/battery gauges; coherent Lucide-style icon system using bundled SVG paths or assets; rich tables, subnavigation, inline labels, meaningful animated state changes.
- **Motion:** 150–250 ms state changes with reduced-motion support. Transitions must not hide pending/failing power actions.
- **Usability:** visible keyboard focus, WCAG 2.2 AA contrast for text, explicit busy/error/success/empty/disabled states, confirmations for shutdown/test/reset/log-clear/flash operations.
- **Performance:** compact, precompiled assets; no Tailwind runtime or CDN. Polling and compatibility must be tested against actual NMC firmware and network behavior rather than simulation alone.
- **Mobile:** welcome but secondary; prioritize 1280–1920 px operators' screens without making the UI unusable on narrower windows.

## Current experimental prototype — known gaps

- A previous local experimental build only adds a read-only frame-free dashboard with stub-tested XML polling; it does not replace every visible management page.
- Its old Help-resource repurposing removes twelve OEM Help pages. That does **not** meet full parity.
- Its BIN is larger than the Dell baseline. The actual device's flash space, image acceptance, boot behavior, and recovery remain unverified.
- No firmware image is approved for flashing to a power-protecting production NMC until these blockers are closed with hardware evidence.

## Acceptance conditions

- All 15 original destinations have native functional replacements, or a clearly documented retained native fallback with functioning navigation and no broken frame dependencies.
- Read-only telemetry confirmed live on the actual NMC; forms are connected to verified NMC handlers; destructive actions require confirmations; full parity regression is demonstrated.
- Firmware format, compression, CRC, app size and resource tables validate offline, **and** flash acceptance, reboot, UI availability, recovery, and UPS monitoring are verified on spare hardware.
- The actual modified BIN is traceable to the exact source commit, original vendor firmware checksum, tests, and release notes.
