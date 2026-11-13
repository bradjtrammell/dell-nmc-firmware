#ifndef OU_AGENTS_H
#define OU_AGENTS_H
#include <stddef.h>
#include <stdint.h>

/* Authorized shutdown-application coordination with no outbound sockets,
 * default credentials, power switching or heap allocation. */
#define OU_AGENT_MAX 12U
#define OU_AGENT_NAME_BYTES 32U
#define OU_AGENT_MIN_LEASE_MS UINT32_C(10000)
#define OU_AGENT_MAX_LEASE_MS UINT32_C(300000)
#define OU_AGENT_MIN_WARNING_MS UINT32_C(30000)
#define OU_AGENT_MAX_WARNING_MS UINT32_C(1800000)
enum ou_agent_phase {
    OU_AGENT_IDLE=0,
    OU_AGENT_WAITING_NOTICE=1,
    OU_AGENT_NOTICE_SENT=2,
    OU_AGENT_ACKNOWLEDGED=3,
    OU_AGENT_UNREACHABLE=4
};
typedef struct {
    uint32_t id;
    char label[OU_AGENT_NAME_BYTES];
    uint32_t heartbeat_sequence;
    uint32_t last_seen_ms;
    uint32_t lease_ms;
    uint32_t ack_transaction;
    uint8_t occupied;
    uint8_t heartbeat_seen;
    uint8_t phase;
} ou_agent;
typedef struct {
    ou_agent agents[OU_AGENT_MAX];
    uint32_t transaction;
    uint32_t warning_start_ms;
    uint32_t warning_deadline_ms;
    uint32_t acknowledgements;
    uint32_t missed;
    uint8_t warning_active;
    uint8_t initialized;
} ou_agent_registry;
typedef struct {
    uint32_t agent_id;
    uint32_t transaction;
    uint32_t warning_deadline_ms;
} ou_agent_notice;
typedef struct ou_agent_status {
    uint32_t enrolled;
    uint32_t online;
    uint32_t waiting;
    uint32_t acknowledged;
    uint32_t unreachable;
    uint8_t warning_active;
} ou_agent_status;
typedef int (*ou_agent_notify_fn)(void *ctx,const ou_agent_notice *notice);
typedef struct {
    ou_agent_notify_fn send_notice;
    void *context;
    uint8_t tls_peer_verified;
    uint8_t destination_identity_qualified;
    uint8_t network_qualified;
} ou_agent_sender;

void ou_agents_init(ou_agent_registry *registry);
/* The caller MUST establish role authorization and securely provision
 * per-client mutual TLS identity outside the record. */
int ou_agents_enroll(ou_agent_registry *registry,uint32_t id,
                     const char *label,uint32_t lease_ms,int admin_authorized);
int ou_agents_remove(ou_agent_registry *registry,uint32_t id,
                     int admin_authorized);
/* Monotonic heartbeat counter prevents replay. peer_identity_verified
 * MUST be derived from authenticated transport, not a user-set HTTP flag. */
int ou_agents_heartbeat(ou_agent_registry *registry,uint32_t id,
                        uint32_t sequence,uint32_t now_ms,
                        int peer_identity_verified);
/* Opens explicit warning window, never executes OS/UPS shutdown itself.
 * Externally authorized operator and independently verified hazard or
 * test-mode approval are both mandatory. */
int ou_agents_begin_warning(ou_agent_registry *registry,uint32_t now_ms,
                      uint32_t warning_ms,int operator_authorized,
                      int verified_shutdown_intent);
int ou_agents_deliver_one(ou_agent_registry *registry,
                      const ou_agent_sender *sender,uint32_t now_ms);
/* ACK only from authenticated client with matching transaction. */
int ou_agents_ack(ou_agent_registry *registry,uint32_t id,
                  uint32_t transaction,uint32_t now_ms,
                  int peer_identity_verified);
/* Returns 1 when all active notifications acknowledged, 0 when pending,
 * -1 on deadline/disconnect or missing warning transaction. No power act. */
int ou_agents_step(ou_agent_registry *registry,uint32_t now_ms);
int ou_agents_status(const ou_agent_registry *registry,uint32_t now_ms,
                     ou_agent_status *status);
#endif
