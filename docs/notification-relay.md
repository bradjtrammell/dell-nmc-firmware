# Notifications in the Dell H910P manager

The four **native email** recipient slots are separate from any number of user-created Monita, Discord, or ntfy destinations. The embedded card's SMTP notification path remains independent of the webhook relay.

## Why the additional relay exists

Dell's existing H910P HTTP/XML interface has four native SMTP slots. Its firmware does not expose a proven general-purpose outbound HTTPS webhook transport or secure token store. Merely adding URL/token inputs to the card UI would not deliver notifications. The optional relay performs persistent polling and HTTP delivery off-card, while the UPS's measurements and controls remain local to the NMC.

The provided Python 3.11+ standard-library relay runs on a trusted LAN host and does not require Docker. It is designed for HTTPS termination in front of its local API (for example, through an already-managed reverse proxy). Protect the API with its 32+-character bearer token, a trusted HTTPS certificate, and source network restrictions. Do not open the relay to the public Internet. The relay binds to `127.0.0.1` in the example configuration.

## Example configuration and run

On a trusted Linux host, create a copy of `notification-relay/config.example.json` at an operator-controlled location. Change `nmc_url`, `allowed_origin`, `data_file`, and `port` as needed; leave `allow_http_destinations` false unless testing within a protected lab. The `allowed_origin` must be the exact origin from which the browser accesses the NMC management UI (scheme, host, and port).

```bash
python3 --version
mkdir -p "$HOME/.config/h910p-relay" "$HOME/.local/share/h910p-relay"
cp notification-relay/config.example.json "$HOME/.config/h910p-relay/config.json"
chmod 600 "$HOME/.config/h910p-relay/config.json"
```

Set the configuration's `data_file` to a writable path (such as `$HOME/.local/share/h910p-relay/destinations.json`, expanded to the full actual pathname). The provided example defaults to `/var/lib/h910p-relay`, which requires appropriate service ownership.

Export `H910P_RELAY_ADMIN_TOKEN` as a strong random, secret 32+-character string and, if the NMC requires credentials, `H910P_NMC_USERNAME` and `H910P_NMC_PASSWORD`. Do not put these values in firmware, Git, screenshots, or public logs. Start the relay with:

```bash
python3 notification-relay/relay.py \
  --config "$HOME/.config/h910p-relay/config.json"
```

Production-like always-on operation requires a process supervisor/systemd and TLS reverse proxy; the repository does not install or configure one automatically. On the NMC UI's Notification manager, supply the reverse proxy's HTTPS URL and the relay bearer token to connect, then Add Destination → Monita/Discord/ntfy. Each destination's secrets are stored server-side in a `0600` file; the API never returns tokens or Discord webhook URLs.

## Provider-specific fields

| Provider | Required fields | Request mechanism |
|---|---|---|
| Native email | Recipient address, selected events and NMC SMTP settings | Existing NMC SMTP; 4 physical slots |
| Monita | Server URL, application API token | `POST /message` with `X-Gotify-Key` compatibility header |
| Discord | Discord webhook URL | JSON webhook POST |
| ntfy | Server URL, topic; optional bearer token | Topic POST |

`Send test` on a remote destination performs a real HTTP POST from the relay, and returns HTTP success/failure. Browser mocks confirm UI behavior only; they are not proof of connectivity to a live service.

## Troubleshooting and safety

A connection failure may indicate mismatched `allowed_origin`, inaccessible reverse proxy, invalid credentials, a self-signed certificate not trusted by the browser, or TLS misconfiguration. A successful API connection does not demonstrate the relay can reach the NMC: check relay service logs and health status separately.

The relay issues only `POST /get_object.xml` reads to the card and never invokes firmware update or UPS control handlers. The NMC's username/password are stored in process environment variables only, not in the destination store. Because the NMC itself may be HTTP-only, keep the relay and NMC on a trusted management VLAN.
