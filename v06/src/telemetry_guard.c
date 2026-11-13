#include "ou_telemetry_guard.h"

static uint32_t add_saturated(uint32_t value) {
    return value == UINT32_MAX ? UINT32_MAX : value + 1U;
}

/* 32-bit sequence/tick forward relation, excluding equality and the
 * ambiguous 2^31 distance. The source must restart via a verified reset
 * when the source clock or boot session is discontinuous. */
static int strictly_forward(uint32_t newer, uint32_t older) {
    uint32_t delta = newer - older;
    return delta != 0U && delta < UINT32_C(0x80000000);
}

static void invalidate(ou_telemetry_guard *guard) {
    guard->current = ou_unavailable_snapshot();
    guard->live = 0U;
}

void ou_telemetry_guard_reset(ou_telemetry_guard *guard) {
    if (!guard) return;
    *guard = (ou_telemetry_guard){0};
    invalidate(guard);
}

int ou_telemetry_guard_accept(ou_telemetry_guard *guard,
                              const ou_driver_report *report,
                              uint32_t now_monotonic_ms) {
    ou_snapshot checked;
    if (!guard) return -1;
    invalidate(guard);
    if (!report ||
        ou_provider_accept(report, now_monotonic_ms, &checked) != 0 ||
        (guard->source_seen &&
         (!strictly_forward(report->sequence, guard->last_sequence) ||
          !strictly_forward(report->received_monotonic_ms,
                            guard->last_received_ms) ||
          report->uptime_seconds < guard->last_uptime_seconds))) {
        guard->rejected_count = add_saturated(guard->rejected_count);
        return -1;
    }
    guard->current = checked;
    guard->last_sequence = report->sequence;
    guard->last_received_ms = report->received_monotonic_ms;
    guard->last_uptime_seconds = report->uptime_seconds;
    guard->source_seen = 1U;
    guard->live = 1U;
    guard->accepted_count = add_saturated(guard->accepted_count);
    return 0;
}

void ou_telemetry_guard_source_lost(ou_telemetry_guard *guard) {
    if (!guard) return;
    invalidate(guard);
}

int ou_telemetry_guard_read(ou_telemetry_guard *guard,
                            uint32_t now_monotonic_ms,
                            ou_snapshot *output) {
    if (!output) return -1;
    *output = ou_unavailable_snapshot();
    if (!guard || !guard->live) return -1;
    if (now_monotonic_ms - guard->last_received_ms > OU_SAMPLE_MAX_AGE_MS) {
        invalidate(guard);
        return -1;
    }
    *output = guard->current;
    return 0;
}
