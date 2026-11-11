# Notification delivery retry policy (dev)

Automatic alerts remain independent of the NMC's four native email slots. The optional LAN relay delivers Monita, Discord and ntfy alerts, honoring each destination's event subscriptions.

When a configured HTTPS destination is temporarily unavailable, the relay now holds up to 256 pending deliveries in memory and retries each no more than five times with increasing delays. The next monitoring cycle processes eligible retries. Removing or disabling a destination cancels its pending deliveries at retry time. The status endpoint includes `pending_deliveries` without exposing webhook URLs or secrets.

**Limitations:** pending alerts are in memory, not persistent across relay restarts; a relay outage or prolonged remote outage can still lose events. Delivery fanout is sequential and can delay polling if many destinations time out. This is not a guaranteed-delivery message broker, and the card does not send HTTPS notifications without the relay. All NMC polling is read-only.

Tests: `python3 -m unittest test_notification_relay.py -v`. Live Monita, Discord and ntfy delivery is not certified by mocked tests.
