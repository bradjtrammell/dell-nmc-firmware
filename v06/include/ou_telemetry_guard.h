#ifndef OU_TELEMETRY_GUARD_H
#define OU_TELEMETRY_GUARD_H

#include "ou_provider.h"

/* Stateful read-only adapter for an independently validated native UPS driver.
 * The caller is responsible for verification of electrical framing, checksum,
 * source identity, and session establishment before setting report.verified.
 * This component never performs I/O or sends UPS commands. */
typedef struct {
    ou_snapshot current;
    uint32_t last_sequence;
    uint32_t last_received_ms;
    uint32_t last_uptime_seconds;
    uint32_t accepted_count;
    uint32_t rejected_count;
    uint8_t source_seen;
    uint8_t live;
} ou_telemetry_guard;

/* Explicitly reset only when the owner has authenticated/revalidated a new
 * physical source session (or at initial power-on). A driver must not call
 * this merely to make a rejected/replayed frame acceptable. */
void ou_telemetry_guard_reset(ou_telemetry_guard *guard);

/* A failed frame immediately invalidates the exposed reading, but preserves
 * replay history. Returns 0 only for a new, verified, in-order sample.
 * Timestamps and sequence numbers support 32-bit monotonic wraparound.
 * A clock rewind or ambiguous half-range jump requires an explicit reset. */
int ou_telemetry_guard_accept(ou_telemetry_guard *guard,
                              const ou_driver_report *report,
                              uint32_t now_monotonic_ms);

/* Return a verified fresh snapshot or an unavailable one. Never expose a
 * previous snapshot after a failed, replayed, or expired frame. */
int ou_telemetry_guard_read(ou_telemetry_guard *guard,
                            uint32_t now_monotonic_ms,
                            ou_snapshot *output);

#endif
