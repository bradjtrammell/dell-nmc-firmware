#include "ou_native_service.h"
#include "ou_telemetry_guard.h"
#include "ou_events.h"

/* One service instance, zero allocation; native events remain available
 * independently of UPS telemetry and are intentionally volatile for now. */
static ou_telemetry_guard source;
static ou_event_log history;

static void record(uint8_t code,uint32_t now_ms) {
    (void)ou_events_add(&history,code,now_ms);
}

void ou_native_service_init(void) {
    ou_telemetry_guard_reset(&source);
    ou_events_init(&history);
    record(OU_EVENT_SERVICE_START,0U);
}

int ou_native_service_driver_report(const ou_driver_report *report,uint32_t now_ms) {
    int accepted=ou_telemetry_guard_accept(&source,report,now_ms);
    if(accepted!=0) {
        record(OU_EVENT_REPORT_REJECTED,now_ms);
        if(history.source_online) {
            record(OU_EVENT_TELEMETRY_OFFLINE,now_ms);
            history.source_online=0U;
            history.state_known=0U;
        }
        return -1;
    }
    if(!history.source_online) {
        record(OU_EVENT_TELEMETRY_ONLINE,now_ms);
        history.source_online=1U;
    }
    if(!history.state_known) {
        if(!report->utility_present)record(OU_EVENT_UTILITY_LOST,now_ms);
        if(report->battery_percent<=20U)record(OU_EVENT_BATTERY_LOW,now_ms);
    } else {
        if(history.last_utility_present!=report->utility_present)
            record(report->utility_present ? OU_EVENT_UTILITY_RESTORED :
                   OU_EVENT_UTILITY_LOST,now_ms);
        if(history.last_battery_low!=(report->battery_percent<=20U))
            record(report->battery_percent<=20U ? OU_EVENT_BATTERY_LOW :
                   OU_EVENT_BATTERY_RECOVERED,now_ms);
    }
    history.state_known=1U;
    history.last_utility_present=report->utility_present;
    history.last_battery_low=(uint8_t)(report->battery_percent<=20U);
    return 0;
}

void ou_native_service_source_lost_at(uint32_t now_ms) {
    ou_telemetry_guard_source_lost(&source);
    if(history.source_online)
        record(OU_EVENT_TELEMETRY_OFFLINE,now_ms);
    history.source_online=0U;
    history.state_known=0U;
}

void ou_native_service_source_lost(void) {
    /* Compatibility caller lacks the event time. Use the last accepted
     * frame's timestamp, never invent a new monotonic reading. */
    ou_native_service_source_lost_at(source.last_received_ms);
}

int ou_native_service_http(const uint8_t *req,size_t req_len,uint32_t now_ms,
                           char *response,size_t capacity,size_t *response_len) {
    ou_snapshot sample=ou_unavailable_snapshot();
    (void)ou_telemetry_guard_read(&source,now_ms,&sample);
    return ou_http_respond_events(req,req_len,&sample,&history,
                                   response,capacity,response_len);
}
