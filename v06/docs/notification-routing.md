# Independent H910P notification routing and delivery

**Implementation status:** native, bounded routing/delivery manager; event-service integration; read-only status API; rule-review management UI. No physical outbound SMTP, HTTPS webhook, or authenticated SNMP trap adapter is qualified. These are project-native components, not Dell firmware and not a change to the release source.

## Routing, fan-out, and delivery lifecycle

`src/notify.c` and `include/ou_notify.h` implement up to eight rules over the existing eight typed events. Every rule binds an event mask to a provisioned destination slot 0–7 and an outbound delivery type: email, webhook, or SNMP trap. Rules can specify 0–3,600 seconds of cooldown. Raw addresses, email recipients, credentials, secrets, and body templates never enter the native event queue.

A verified native service event is enqueued by its event ID and boot generation; repeated observations of that same event cannot trigger a second delivery for a rule. Different rule destinations fan out independently. The bounded queue accepts at most 24 jobs, drops excess jobs with accounting, and records delivered, failed, retried, and dropped totals. One eligible job is processed per cooperative tick. Temporary failures retry with exponential delays starting at 2 seconds, capped at 32 seconds and five attempts; permanent failure or exhausting the attempt budget terminates the job. Per-rule cooldowns resist spamming and handle monotonic clock wrap.

Rules may be replaced only through the separate `ou_native_service_notify_configure` caller-authorizer boundary, not HTTP. A replacement invalidates pending work addressed to the old destinations. The sink requires *separate positive qualifications* for outbound delivery, pre-provisioned destinations, and encrypted/authenticated transport security (SMTP/webhook TLS; SNMPv3 authentication/privacy). No sender or qualification means **no network activity**.

## Device service and management integration

The native service records validated events and immediately passes typed event IDs into the router. An explicitly provisioned sender may progress its bounded queue. The service does not invent a delivery transport, credentials or real recipient and does not bind a sender on the installed H910P.

`GET /api/v1/notifications/policy` declares the supported eight event types, three transport categories, maximum queue/rules/attempts, and false values for `deliveryAvailable`, `rulesWritable`, `destinationsProvisioned`, `transportQualified`, and `credentialsConfigured`. The native `GET /api/v1/notifications/status` reports event queue counts and delivery counters; the loopback host simulator returns 503 instead of a fictitious native queue.

The Email Notification and SNMP management pages expose a local-only rule editor: select typed event, target transport category, provisioned destination slot and cooldown; duplicate routes are rejected; up to eight rules can be staged locally. JSON export is explicitly `applied:false`, `installable:false`, and `deliveryEnabled:false`. No network recipient, credential, or actual sending can be set in the preview.

## Before production use

Implement a credential-safe, qualified HTTPS transport; SMTP submission with STARTTLS/SMTPS and credential verification; or authenticated encrypted SNMPv3 traps, destination provisioning and audit controls, authenticated remote configuration, and reboot-persistent notification policy. Implement an outage-tolerant delivery spool if preserving alerts across reboots is required. Perform hardware and receiver interoperability tests and ensure no UPS power-state changes are made by notification delivery.

**Not complete:** SMTP email sending, HTTPS webhook delivery, SNMPv3 polling/traps, actual operator credential configuration, device hardware network qualification, and production alert delivery. These are not excluded from project scope.

## Verification

The host and native service tests cover typed event routing, email/webhook/trap categories, independent fan-out, replay rejection, cooldown including 32-bit rollover, a full queue, invalid rules and authorization denial, retry/backoff and terminal failures, qualified-versus-unqualified sends, shutdown/reboot clearing, HTTP status reporting, and no mutation endpoints. The Node UI suite covers rule editing, duplicate validation and review export. ARM7 link and AddressSanitizer/UBSan gates remain required. No tests send actual messages.
