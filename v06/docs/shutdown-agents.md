# Independent H910P shutdown application coordination

**Implementation:** native fixed-memory registration and lifecycle engine, heartbeat/identity gates, advance-warning transactions, one-at-a-time notice delivery callbacks, authenticated acknowledgements, deadline handling, and native API/UI reporting. The installed card has no qualified remote-agent transport or UPS shutdown driver. This is a coordination engine, not a covert power-action path.

## Native client management

`src/agents.c` with `include/ou_agents.h` supports **up to 12 registered shutdown applications** with unique positive integer IDs, validated labels, and configurable heartbeat leases of 10–300 seconds. The engine rejects duplicate IDs, stale/replayed sequence numbers, unverified client identity, invalid names, unauthorized enrollment/removal, and capacity overflow. No plaintext agent credentials, shared passwords, network endpoints, or hardcoded device secrets are stored in the fixed-memory registry.

Production enrollment and heartbeat must be performed by a trusted, authenticated device-side service. Every `peer_identity_verified` assertion must come from a verified mTLS/credential transport, never an untrusted HTTP parameter. The current read-only development HTTP surface does **not** expose enrollment, heartbeat, acknowledgement, or shutdown requests.

## Safe shutdown-warning coordination

An authorized operator and separately verified shutdown intent are **both mandatory** to begin a warning. A warning requires at least one enrolled client and all registered clients to have healthy heartbeat leases. Warning windows are 30–1,800 seconds. The service allocates a monotonic transaction ID and tracks every registered client through `WAITING_NOTICE`, `NOTICE_SENT`, `ACKNOWLEDGED`, or `UNREACHABLE`.

An outbound notice may only be emitted by a separately verified transport with qualified client identity, authenticated TLS peers and network safety. Notices contain the target client ID, transaction ID and deadline; they do not contain credentials or direct UPS power commands. Failed/unqualified delivery never fabricates a confirmation. Acknowledgements require the matching transaction, verified client identity, a previously sent notice, an online heartbeat, and an unexpired warning window. Duplicate acknowledgements are rejected.

The bounded cooperative scheduler declares success only when all enrolled applications acknowledge. A missed deadline, lost heartbeat, or permanent delivery failure marks the affected client unreachable and returns failure. There is **no code path that shuts down an OS, UPS output, or power segment** after acknowledgement or timeout. That separate power-control orchestration must be explicitly authorized and tested later, with an independent physical driver and agent protocols.

## API and UI integration

`GET /api/v1/agents/capabilities` states the registry capacity, heartbeat/warning bounds and compiled transaction logic, while explicitly marking registration writes, real transport, mTLS, verified shutdown intent and physical power control as unavailable.

The native-only `GET /api/v1/agents/status` provides enrolled, online, waiting, acknowledged, unreachable and active-warning counts. The loopback host simulator responds 503 instead of inventing clients. The Notified Applications page now provides locally validated client registration drafts (IDs, names, lease) and review JSON exports; the Shutdown Parameters page displays the warning policy and lets operators export a warning-window review. All exports are **not installable**, and none change the card.

## Validation and remaining production work

Host C tests cover enrollment, access denial, identity/heartbeat replay, lease expiry, twelve-client limits, transaction preparation, notification delivery qualification, acknowledgement, expired/missing clients, deadline rollover, completion, and failed transports. Native-service integration tests exercise the real API and the dispatcher interface. HTTP and browser behavior tests verify no live mutation path. ARM7 cross-compilation and sanitizer execution are mandatory.

Not yet complete: persistent agent enrollment and secure identities, mTLS credential provisioning and renewal, actual notifications over the trusted network, real Windows/Linux shutdown-companion protocols, independent recovery, complete UPS power-control interlock, and real hardware acceptance. These are **not removed from the original project scope**.
