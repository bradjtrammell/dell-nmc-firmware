#ifndef OU_EVENTS_H
#define OU_EVENTS_H

#include <stddef.h>
#include <stdint.h>

/* Freestanding, fixed-memory, typed native event history. No heap, flash
 * writes, raw UPS payloads, usernames, credentials or free-form messages.
 * This volatile log is NOT a substitute for the future persistent journal. */
#define OU_EVENTS_CAPACITY 32U
#define OU_EVENTS_PAGE_SIZE 8U

enum ou_event_code {
    OU_EVENT_SERVICE_START = 1,
    OU_EVENT_TELEMETRY_ONLINE = 2,
    OU_EVENT_TELEMETRY_OFFLINE = 3,
    OU_EVENT_REPORT_REJECTED = 4,
    OU_EVENT_UTILITY_LOST = 5,
    OU_EVENT_UTILITY_RESTORED = 6,
    OU_EVENT_BATTERY_LOW = 7,
    OU_EVENT_BATTERY_RECOVERED = 8
};
#define OU_EVENT_FIRST OU_EVENT_SERVICE_START
#define OU_EVENT_LAST OU_EVENT_BATTERY_RECOVERED

typedef struct {
    uint32_t id;
    uint32_t monotonic_ms;
    /* Distinguishes timestamps from different boots after archive restore. */
    uint32_t boot_generation;
    uint8_t code;
} ou_event_record;

typedef struct ou_event_log {
    ou_event_record records[OU_EVENTS_CAPACITY];
    uint32_t next_id;
    uint32_t boot_generation;
    uint32_t overwritten;
    uint8_t count;
    uint8_t next_slot;
    /* Source transition tracking, reset on requalification. */
    uint8_t source_online;
    uint8_t state_known;
    uint8_t last_utility_present;
    uint8_t last_battery_low;
} ou_event_log;

/* Initializes a blank RAM-only history. Call once at service startup. */
void ou_events_init(ou_event_log *log);

/* Add a validated typed event with an automatically monotonic ID.
 * Returns -1 on unknown codes or ID exhaustion without changing history.
 * Overflow evicts the oldest record and increments overwritten. */
int ou_events_add(ou_event_log *log, uint8_t code, uint32_t now_ms);

/* Read entry at newest-first index (0 is latest). */
int ou_events_recent(const ou_event_log *log, size_t newest_index,
                     ou_event_record *record);

#endif
