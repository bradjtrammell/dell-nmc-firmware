# Changelog

## 0.5.10 — Tab session continuity and XML transport reliability (experimental)

- Restored authentication across a same-tab refresh by saving the NMC Basic header in tab-scoped browser storage and revalidating it before showing controls.
- Lock now clears tab storage and in-memory authorization; rejected cached credentials are discarded.
- Serialized all device XML and CGI operations to prevent poll/configuration races on the memory-constrained NMC.
- Added a single bounded retry for idempotent XML reads, retained last valid measurements on one missed poll, and preserved visible warnings for repeated failures.
- Retained all prior management pages, four native email slots, unlimited relay destinations and separate SMTP configuration.
- Switched the embedded Dell logo to the equivalent encoded SVG data URI and trimmed unused runtime code to remain within the factory firmware image limit.
- Added session restoration and XML regression checks; on-device validation remains outstanding.


## 0.5.9 — Browser sign-in correction (experimental)

- Changed the formal browser sign-in request and card API requests to use explicit Basic Authorization without browser-managed Basic-auth credential negotiation.
- Corrected the error displayed when the device sign-in request times out.
- Preserved card-side authorization and in-memory credentials; no authentication bypass.
- Removed developer-facing notification disclaimer from the management screen.
- Retained and retested the four native email slots, provider-specific destinations, and separate SMTP manager.
- Added dedicated authentication regression checks and rebuilt the firmware under the factory image-size limit.

## 0.5.8 — Notification manager

- Unified notification destinations in a table, with up to four native email recipients and unlimited additional supported relay destinations.
- Added separate SMTP configuration modal and destination-level actions.
