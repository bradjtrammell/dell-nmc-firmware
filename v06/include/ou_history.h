#ifndef OU_HISTORY_H
#define OU_HISTORY_H
#include "openh910p.h"

/* Fixed-memory, independently source-verified UPS measurements. Never accepts
 * simulator data or caller-supplied raw UPS packets. Sampling is per verified
 * native report, not per HTTP request. No heap, flash, POSIX or clock access. */
#define OU_HISTORY_CAPACITY 64U
#define OU_HISTORY_PAGE_SIZE 8U
#define OU_HISTORY_INTERVAL_MS UINT32_C(60000)

typedef struct {
    uint32_t sequence;
    uint32_t monotonic_ms;
    uint32_t runtime_seconds;
    uint16_t input_decivolts;
    uint16_t output_decivolts;
    uint8_t battery_percent;
    uint8_t load_percent;
    uint8_t ac_present;
    uint8_t gap_before;
} ou_history_sample;

typedef struct ou_history_log {
    ou_history_sample samples[OU_HISTORY_CAPACITY];
    uint32_t overwritten;
    uint32_t latest_sequence;
    uint32_t last_capture_ms;
    uint8_t count;
    uint8_t next_slot;
    uint8_t gap_pending;
} ou_history_log;

void ou_history_init(ou_history_log *log);
/* Returns 1 if sampled; 0 for a validated but interval-skipped reading;
 * -1 for any unverified, malformed or discontinuous sample. A source change,
 * AC transition or battery-low threshold crossing bypasses interval limits. */
int ou_history_accept(ou_history_log *log, const ou_snapshot *snapshot,
                      uint32_t now_ms);
/* Marks the next valid sample as following a discontinuity. */
void ou_history_source_lost(ou_history_log *log);
/* Newest-first; no raw pointers to internal mutable storage escape. */
int ou_history_recent(const ou_history_log *log, size_t index,
                      ou_history_sample *sample);
#endif
