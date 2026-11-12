#include "ou_ups_link.h"

void ou_link_init(ou_link_observer *observer) {
    if (observer) *observer = (ou_link_observer){0};
}

int ou_link_observe(ou_link_observer *observer, uint8_t channel,
                    const uint8_t *bytes, size_t size, uint32_t now) {
    uint32_t hash = UINT32_C(2166136261);
    size_t i;
    ou_link_record *record;
    if (!observer) return -1;
    if (!bytes || size == 0U || size > OU_LINK_MAX_REPORT_BYTES ||
        channel > OU_LINK_CHANNEL_OTHER) {
        observer->rejected_reports++;
        return -1;
    }
    /* A capture's timestamp is trusted only as an observation clock, not as
     * a UTC wall clock; test no serial-frame structure before it is known. */
    for (i = 0U; i < size; ++i) {
        hash ^= bytes[i];
        hash *= UINT32_C(16777619);
    }
    record = &observer->log[observer->next];
    record->monotonic_ms = now;
    record->fingerprint = hash;
    record->size = (uint16_t)size;
    record->channel = channel;
    record->reserved = 0;
    observer->next = (uint8_t)((observer->next + 1U) % OU_LINK_LOG_CAPACITY);
    if (observer->count < OU_LINK_LOG_CAPACITY) observer->count++;
    else observer->overwritten_records++;
    observer->reports_seen++;
    if (UINT32_MAX - observer->bytes_seen < size) observer->bytes_seen = UINT32_MAX;
    else observer->bytes_seen += (uint32_t)size;
    observer->last_monotonic_ms = now;
    return 0;
}

int ou_link_recent(const ou_link_observer *observer, unsigned index,
                   ou_link_record *result) {
    if (!observer || !result || index >= observer->count) return -1;
    *result = observer->log[(observer->next + OU_LINK_LOG_CAPACITY - 1U - index) % OU_LINK_LOG_CAPACITY];
    return 0;
}
