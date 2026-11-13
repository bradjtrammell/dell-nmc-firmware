#include "ou_native_service.h"
#include "ou_telemetry_guard.h"
#include "ou_events.h"
#include "ou_event_archive.h"
#include "ou_history.h"
#include "ou_notify.h"
#include "ou_snmp.h"
#include "ou_agents.h"
#include "ou_time_sync.h"
#include "ou_shutdown_policy.h"

/* One service instance, zero allocation; native events remain available
 * independently of UPS telemetry and are intentionally volatile for now. */
static ou_telemetry_guard source;
static ou_event_log history;
static ou_history_log measurements;
static ou_notify_engine notifications;
static ou_agent_registry agents;
static ou_time_sync clock_service;
static ou_shutdown_policy shutdown_model;
static uint8_t shutdown_policy_bound;

static void record(uint8_t code,uint32_t now_ms) {
    if(ou_events_add(&history,code,now_ms)==0){
        ou_event_record observed;
        if(ou_events_recent(&history,0U,&observed)==0)
            (void)ou_notify_route(&notifications,&observed,now_ms);
    }
}

void ou_native_service_init(void) {
    ou_telemetry_guard_reset(&source);
    ou_events_init(&history);
    ou_history_init(&measurements);
    ou_notify_init(&notifications);
    ou_agents_init(&agents);
    clock_service=(ou_time_sync){0};
    ou_shutdown_policy_init(&shutdown_model);
    shutdown_policy_bound=0U;
    record(OU_EVENT_SERVICE_START,0U);
}

int ou_native_service_events_restore(const ou_journal_io *dedicated_events) {
    if(source.source_seen || history.source_online)return -1;
    if(ou_event_archive_restore(&history,dedicated_events)!=0)return -1;
    /* Current boot's service-start marker has its own timestamp epoch,
     * distinct from the archive. The previous boot's event IDs remain. */
    return ou_events_add(&history,OU_EVENT_SERVICE_START,0U);
}

int ou_native_service_events_checkpoint(const ou_journal_io *dedicated_events,
                                         ou_journal_record *record) {
    return ou_event_archive_checkpoint(&history,dedicated_events,record);
}

int ou_native_service_notify_configure(const ou_notify_rule *rules,
                      size_t count,int (*authorize)(void *),void *context) {
    if(!authorize || authorize(context)!=1)return -1;
    return ou_notify_set_rules(&notifications,rules,count,1);
}
int ou_native_service_notify_step(const ou_notify_sender *sender,
                                   uint32_t now_ms){
    return ou_notify_step(&notifications,sender,now_ms);
}

int ou_native_service_driver_report(const ou_driver_report *report,uint32_t now_ms) {
    int accepted=ou_telemetry_guard_accept(&source,report,now_ms);
    if(accepted!=0) {
        ou_history_source_lost(&measurements);
        record(OU_EVENT_REPORT_REJECTED,now_ms);
        if(history.source_online) {
            record(OU_EVENT_TELEMETRY_OFFLINE,now_ms);
            history.source_online=0U;
            history.state_known=0U;
        }
        return -1;
    }
    if(ou_history_accept(&measurements,&source.current,
                         report->received_monotonic_ms)<0) {
        ou_telemetry_guard_source_lost(&source);
        ou_history_source_lost(&measurements);
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
    ou_history_source_lost(&measurements);
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

int ou_native_service_snmp(const ou_snmp_access *access,
               const uint8_t *request,size_t request_len,uint32_t now_ms,
               uint8_t *response,size_t capacity,size_t *written) {
    ou_snapshot sample=ou_unavailable_snapshot();
    (void)ou_telemetry_guard_read(&source,now_ms,&sample);
    return ou_snmp_respond(access,&sample,request,request_len,
                           response,capacity,written);
}

int ou_native_service_agent_enroll(uint32_t id,const char *label,
                                    uint32_t lease_ms,int admin_authorized){
    return ou_agents_enroll(&agents,id,label,lease_ms,admin_authorized);
}
int ou_native_service_agent_heartbeat(uint32_t id,uint32_t sequence,
                          uint32_t now_ms,int peer_identity_verified){
    return ou_agents_heartbeat(&agents,id,sequence,now_ms,peer_identity_verified);
}
int ou_native_service_agent_warning(uint32_t now_ms,uint32_t warning_ms,
                          int operator_authorized,int verified_intent){
    return ou_agents_begin_warning(&agents,now_ms,warning_ms,
                                    operator_authorized,verified_intent);
}
int ou_native_service_agent_notify(const ou_agent_sender *sender,uint32_t now_ms){
    return ou_agents_deliver_one(&agents,sender,now_ms);
}
int ou_native_service_agent_ack(uint32_t id,uint32_t transaction,
                                uint32_t now_ms,int peer_identity_verified){
    return ou_agents_ack(&agents,id,transaction,now_ms,peer_identity_verified);
}
int ou_native_service_agent_step(uint32_t now_ms){
    return ou_agents_step(&agents,now_ms);
}
int ou_native_service_time_bind(const ou_time_source *source) {
    return ou_time_init(&clock_service,source);
}
int ou_native_service_time_request(uint32_t now_ms,uint8_t wire[OU_NTP_WIRE_BYTES]){
    return ou_time_make_request(&clock_service,now_ms,wire);
}
int ou_native_service_time_response(const uint8_t *wire,size_t length,
                            uint32_t now_ms,int source_address_verified){
    return ou_time_accept(&clock_service,wire,length,now_ms,source_address_verified);
}
int ou_native_service_shutdown_bind(const ou_shutdown_policy *policy,
                             int admin_authorized,int qualified_storage){
    if(admin_authorized!=1||qualified_storage!=1||
       ou_shutdown_policy_validate(policy)!=0)return -1;
    shutdown_model=*policy;
    shutdown_policy_bound=1U;
    return 0;
}
int ou_native_service_http(const uint8_t *req,size_t req_len,uint32_t now_ms,
                           char *response,size_t capacity,size_t *response_len) {
    ou_snapshot sample=ou_unavailable_snapshot();
    ou_agent_status agent_summary;
    ou_time_sample current_time;
    const ou_time_sample *time_pointer=0;
    ou_shutdown_advisory advisory;
    const ou_shutdown_advisory *shutdown_pointer=0;
    if(ou_time_now(&clock_service,now_ms,&current_time)==0)
        time_pointer=&current_time;
    (void)ou_agents_status(&agents,now_ms,&agent_summary);
    (void)ou_telemetry_guard_read(&source,now_ms,&sample);
    if(shutdown_policy_bound==1U &&
       ou_shutdown_assess(&shutdown_model,&sample,&advisory)==0)
        shutdown_pointer=&advisory;
    return ou_http_respond_managed(req,req_len,&sample,&history,
                                   &measurements,&notifications,&agent_summary,time_pointer,shutdown_pointer,
                                   response,capacity,response_len);
}
