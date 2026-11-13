#include "ou_events.h"

void ou_events_init(ou_event_log *log) {
    if (!log) return;
    *log = (ou_event_log){0};
    log->next_id = 1U;
}

int ou_events_add(ou_event_log *log, uint8_t code, uint32_t now_ms) {
    ou_event_record *entry;
    if (!log || log->next_id == 0U || log->next_id == UINT32_MAX ||
        code < OU_EVENT_FIRST || code > OU_EVENT_LAST ||
        log->count > OU_EVENTS_CAPACITY ||
        log->next_slot >= OU_EVENTS_CAPACITY) return -1;
    entry = &log->records[log->next_slot];
    entry->id = log->next_id++;
    entry->monotonic_ms = now_ms;
    entry->code = code;
    log->next_slot = (uint8_t)((log->next_slot + 1U) % OU_EVENTS_CAPACITY);
    if (log->count < OU_EVENTS_CAPACITY) ++log->count;
    else if (log->overwritten != UINT32_MAX) ++log->overwritten;
    return 0;
}

int ou_events_recent(const ou_event_log *log, size_t newest_index,
                     ou_event_record *record) {
    size_t slot;
    if (!log || !record || log->count > OU_EVENTS_CAPACITY ||
        log->next_slot >= OU_EVENTS_CAPACITY ||
        newest_index >= log->count) return -1;
    slot = ((size_t)log->next_slot + OU_EVENTS_CAPACITY -
            1U - newest_index) % OU_EVENTS_CAPACITY;
    *record = log->records[slot];
    if (record->code < OU_EVENT_FIRST || record->code > OU_EVENT_LAST ||
        record->id == 0U) return -1;
    return 0;
}
