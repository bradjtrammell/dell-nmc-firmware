#ifndef OPENH910P_H
#define OPENH910P_H

#include <stddef.h>
#include <stdint.h>
#include "ou_events.h"

#define OU_API_VERSION 1U
#define OU_FIRMWARE_VERSION "0.6.0-alpha.1"
#define OU_JSON_CAPACITY 4096U

enum ou_origin {
    OU_ORIGIN_UNAVAILABLE = 0,
    OU_ORIGIN_SIMULATOR = 1,
    OU_ORIGIN_NATIVE = 2
};

typedef struct {
    uint32_t sequence;
    uint32_t uptime_seconds;
    uint32_t battery_percent;
    uint32_t estimated_runtime_seconds;
    uint32_t input_voltage_decivolts;
    uint32_t output_voltage_decivolts;
    uint32_t output_load_percent;
    uint8_t ac_present;
    uint8_t battery_present;
    uint8_t segment_1_on;
    uint8_t segment_2_on;
    uint8_t origin;       /* validated source, never inferred by browser */
    uint8_t sample_ready; /* false => no telemetry until driver exists */
} ou_snapshot;

typedef struct {
    int status;
    const char *content_type;
    size_t body_length;
} ou_reply;

/* Pure, freestanding C99: no I/O, heap, clock, network, or Dell dependencies.
 * The snapshot is supplied by a caller, not populated from unverified hardware.
 * Returns -1 on invalid arguments or insufficient response buffer.
 */
struct ou_history_log;
/* Native-only history is optional; simulator never manufactures prior UPS
 * samples. Compatibility dispatcher functions remain source-stable. */
struct ou_notify_engine;
int ou_dispatch_managed(const char *method, const char *path,
                const ou_snapshot *snapshot, const ou_event_log *events,
                const struct ou_history_log *history,
                const struct ou_notify_engine *notifications,
                char *body, size_t capacity, ou_reply *reply);

int ou_dispatch_observed(const char *method, const char *path,
                const ou_snapshot *snapshot, const ou_event_log *events,
                const struct ou_history_log *history,
                char *body, size_t capacity, ou_reply *reply);

/* Extended native dispatcher uses an independently owned event store.
 * Existing host simulation remains unchanged through ou_dispatch(). */
int ou_dispatch_events(const char *method, const char *path,
                const ou_snapshot *snapshot, const ou_event_log *events,
                char *body, size_t capacity, ou_reply *reply);

int ou_dispatch(const char *method, const char *path,
                const ou_snapshot *snapshot, char *body,
                size_t capacity, ou_reply *reply);

/* Explicitly simulated, never derived from attached UPS hardware. */
ou_snapshot ou_simulated_snapshot(void);
/* Zero-value / unavailable snapshot: never substitute sample readings. */
ou_snapshot ou_unavailable_snapshot(void);

#endif
