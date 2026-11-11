#ifndef OU_PROVIDER_H
#define OU_PROVIDER_H

#include "openh910p.h"

/* Read-only hardware adapter boundary. Nothing in this module touches MMIO,
 * sends UART bytes, writes flash or accepts UPS power commands. */
#define OU_REPORT_VERSION 1U
#define OU_SAMPLE_MAX_AGE_MS 5000U

/* A driver must set version=1 and verified=1 only after its physical read-only
 * transaction has passed framing, checksum, device identity and range checks.
 * No physical driver is currently linked to the on-card target. */
typedef struct {
    uint32_t version;
    uint32_t received_monotonic_ms;
    uint32_t sequence;
    uint32_t uptime_seconds;
    uint32_t runtime_seconds;
    uint16_t battery_percent;
    uint16_t load_percent;
    uint16_t input_decivolts;
    uint16_t output_decivolts;
    uint8_t utility_present;
    uint8_t battery_present;
    uint8_t segment_1_on;
    uint8_t segment_2_on;
    uint8_t verified;
} ou_driver_report;

/* Reject unsupported, corrupted or stale readings; on rejection, output is
 * initialized as unavailable, NEVER to stale or simulator values. */
int ou_provider_accept(const ou_driver_report *report,
                       uint32_t now_monotonic_ms, ou_snapshot *output);

#endif
