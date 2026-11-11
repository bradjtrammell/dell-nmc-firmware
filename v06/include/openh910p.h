#ifndef OPENH910P_H
#define OPENH910P_H

#include <stddef.h>
#include <stdint.h>

#define OU_API_VERSION 1U
#define OU_FIRMWARE_VERSION "0.6.0-dev.2"
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
int ou_dispatch(const char *method, const char *path,
                const ou_snapshot *snapshot, char *body,
                size_t capacity, ou_reply *reply);

/* Explicitly simulated, never derived from attached UPS hardware. */
ou_snapshot ou_simulated_snapshot(void);
/* Zero-value / unavailable snapshot: never substitute sample readings. */
ou_snapshot ou_unavailable_snapshot(void);

#endif
