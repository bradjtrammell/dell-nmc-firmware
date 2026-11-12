#ifndef OU_UPS_LINK_H
#define OU_UPS_LINK_H

#include <stddef.h>
#include <stdint.h>

/* OEM 01.18 code contains COPI_READ, shutHostGetReport and HID report
 * references; its physical bus, framing and checksum are NOT yet verified.
 * This receive-only bounded observer accepts already-delimited opaque reports
 * from a FUTURE verified link adapter. No TX, polling or power commands. */
#define OU_LINK_MAX_REPORT_BYTES 256U
#define OU_LINK_LOG_CAPACITY 32U
#define OU_LINK_CHANNEL_UNKNOWN 0U
#define OU_LINK_CHANNEL_SERIAL_A 1U
#define OU_LINK_CHANNEL_SERIAL_B 2U
#define OU_LINK_CHANNEL_OTHER 3U

typedef struct {
    uint32_t monotonic_ms;
    uint32_t fingerprint; /* FNV-1a, non-cryptographic; not message validation */
    uint16_t size;
    uint8_t channel;
    uint8_t reserved;
} ou_link_record;

typedef struct {
    ou_link_record log[OU_LINK_LOG_CAPACITY];
    uint32_t reports_seen;
    uint32_t bytes_seen;
    uint32_t rejected_reports;
    uint32_t overwritten_records;
    uint32_t last_monotonic_ms;
    uint8_t next;
    uint8_t count;
} ou_link_observer;

void ou_link_init(ou_link_observer *observer);
/* Returns 0 if accepted for passive metadata-only recording. This NEVER
 * marks driver telemetry verified. The contents of packets are not retained
 * in memory or exposed to API. This is not a COPI frame decoder. */
int ou_link_observe(ou_link_observer *observer, uint8_t channel,
                    const uint8_t *bytes, size_t size, uint32_t monotonic_ms);
/* newest first; 0 on success, -1 for out of range */
int ou_link_recent(const ou_link_observer *observer, unsigned index,
                   ou_link_record *result);

#endif
