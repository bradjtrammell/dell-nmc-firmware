#ifndef OU_NOTIFY_H
#define OU_NOTIFY_H
#include "ou_events.h"

/* Fixed-memory delivery manager. Transport slots are pre-provisioned
 * identifiers, NOT URLs, email addresses or credentials supplied by the
 * wire. No physical network activity without a bound qualified sender. */
#define OU_NOTIFY_RULES 8U
#define OU_NOTIFY_QUEUE 24U
#define OU_NOTIFY_MAX_ATTEMPTS 5U
#define OU_NOTIFY_BACKOFF_BASE_MS 2000U
#define OU_NOTIFY_BACKOFF_MAX_MS 32000U
enum ou_notify_transport {
    OU_NOTIFY_EMAIL=1, OU_NOTIFY_WEBHOOK=2, OU_NOTIFY_SNMP_TRAP=3
};
typedef struct {
    uint16_t event_mask;       /* bit (event_code - 1), known eight events */
    uint32_t cooldown_ms;      /* per rule, min 0, max 1 hour */
    uint8_t enabled;
    uint8_t transport;
    uint8_t destination;       /* 0..7, pre-provisioned out of band */
} ou_notify_rule;
typedef struct {
    uint32_t event_id;
    uint32_t boot_generation;
    uint32_t created_ms;
    uint32_t due_ms;
    uint8_t code;
    uint8_t transport;
    uint8_t destination;
    uint8_t attempt;
} ou_notify_job;
typedef struct ou_notify_engine {
    ou_notify_rule rules[OU_NOTIFY_RULES];
    ou_notify_job queue[OU_NOTIFY_QUEUE];
    uint32_t last_trigger_ms[OU_NOTIFY_RULES];
    uint32_t last_event_id[OU_NOTIFY_RULES];
    uint32_t last_boot_generation[OU_NOTIFY_RULES];
    uint8_t triggered[OU_NOTIFY_RULES];
    uint8_t used[OU_NOTIFY_QUEUE];
    uint32_t enqueued, delivered, dropped, failed, retried;
    uint8_t sender_qualified;
    uint8_t initialized;
} ou_notify_engine;
/* 1=delivered, 0=temporary failure, -1=permanent failure. This callback
 * must be a verified authenticated outbound driver; no callback means
 * NOT sent. Never accepts a raw network destination. */
typedef int (*ou_notify_send_fn)(void *context,const ou_notify_job *job);
typedef struct {
    ou_notify_send_fn send;
    void *context;
    uint8_t transport_qualified;
    uint8_t destination_store_qualified;
    uint8_t transport_security_verified;
} ou_notify_sender;

void ou_notify_init(ou_notify_engine *engine);
/* Replace rules atomically. Externally authorized administrative function,
 * not exposed over read-only HTTP. */
int ou_notify_set_rules(ou_notify_engine *engine,
                        const ou_notify_rule *rules,size_t count,
                        int authorized);
/* Enqueues zero or more jobs; ignores duplicate records and blocked
 * destinations. Returns number queued, -1 on malformed event state. */
int ou_notify_route(ou_notify_engine *engine,const ou_event_record *event,
                    uint32_t now_ms);
/* Retry one due delivery. Returns 1 delivered, 0 pending/no eligible job,
 * -1 permanent error/dropped. A sender is NEVER called until fully
 * qualified, and transport type remains selected by trusted rule only. */
int ou_notify_step(ou_notify_engine *engine,const ou_notify_sender *sender,
                   uint32_t now_ms);
size_t ou_notify_pending(const ou_notify_engine *engine);
#endif
