#include "openh910p.h"
#include "ou_schema.h"
#include "ou_history.h"
#include "ou_notify.h"
#include "ou_agents.h"
#include "ou_time_sync.h"
#include "ou_shutdown_policy.h"

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
    int error;
} json_writer;

static int same(const char *left, const char *right) {
    if (!left || !right) return 0;
    while (*left && *right && *left == *right) { left++; right++; }
    return *left == '\0' && *right == '\0';
}

static void put_char(json_writer *writer, char ch) {
    if (writer->error) return;
    if (writer->length + 1U >= writer->capacity) { writer->error = 1; return; }
    writer->data[writer->length++] = ch;
}

static void put_str(json_writer *writer, const char *value) {
    if (!value) { writer->error = 1; return; }
    while (*value) put_char(writer, *value++);
}

static void put_uint(json_writer *writer, uint32_t value) {
    char temporary[10];
    unsigned index = 0;
    do {
        temporary[index++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value && index < sizeof(temporary));
    while (index) put_char(writer, temporary[--index]);
}

static void put_bool(json_writer *writer, uint8_t value) {
    put_str(writer, value ? "true" : "false");
}

/* Kept in the freestanding core for eventual hardware-provided strings. */
static void put_quoted(json_writer *writer, const char *value) {
    static const char digits[] = "0123456789abcdef";
    if (!value) { writer->error = 1; return; }
    put_char(writer, '"');
    for (; *value; value++) {
        unsigned char c = (unsigned char)*value;
        if (c == '"' || c == '\\') {
            put_char(writer, '\\'); put_char(writer, (char)c);
        } else if (c < 0x20U) {
            put_str(writer, "\\u00");
            put_char(writer, digits[c >> 4]);
            put_char(writer, digits[c & 15U]);
        } else {
            put_char(writer, (char)c);
        }
    }
    put_char(writer, '"');
}

static void property_uint(json_writer *writer, const char *key,
                          uint32_t value, int comma) {
    if (comma) put_char(writer, ',');
    put_quoted(writer, key); put_char(writer, ':'); put_uint(writer, value);
}

static void property_bool(json_writer *writer, const char *key,
                          uint8_t value, int comma) {
    if (comma) put_char(writer, ',');
    put_quoted(writer, key); put_char(writer, ':'); put_bool(writer, value);
}

static void property_string(json_writer *writer, const char *key,
                            const char *value, int comma) {
    if (comma) put_char(writer, ',');
    put_quoted(writer, key); put_char(writer, ':'); put_quoted(writer, value);
}

static void common(json_writer *w, const char *resource, const ou_snapshot *snapshot) {
    put_char(w, '{');
    property_uint(w, "schemaVersion", OU_API_VERSION, 0);
    property_string(w, "resource", resource, 1);
    property_string(w, "source", snapshot->origin == OU_ORIGIN_SIMULATOR ? "simulator" :
        snapshot->origin == OU_ORIGIN_NATIVE ? "native" : "unavailable", 1);
    property_bool(w, "simulated", snapshot->origin == OU_ORIGIN_SIMULATOR, 1);
    property_uint(w, "sequence", snapshot->sequence, 1);
}

static void one_outlet(json_writer *w, uint32_t id,
                       const char *name, uint8_t on, int comma) {
    if (comma) put_char(w, ',');
    put_char(w, '{');
    property_uint(w, "id", id, 0);
    property_string(w, "label", name, 1);
    property_string(w, "state", on ? "on" : "off", 1);
    property_bool(w, "switchable", id != 1U, 1);
    property_bool(w, "controlEnabled", 0, 1);
    put_char(w, '}');
}

/* Returns 1 for a valid schema page, -1 for a malformed page, or 0 for
 * unrelated paths. Keep pagination independent of POSIX/HTTP implementations. */
/* Events use explicit path segments because the strict HTTP parser rejects
 * query strings. /events shows page 0; /events/0..3 are stable page IDs. */
static int history_page(const char *path,uint32_t *page) {
    const char prefix[]="/api/v1/ups/history/";
    const char *p=prefix,*s=path;
    uint32_t n=0U;
    if(same(path,"/api/v1/ups/history")){*page=0U;return 1;}
    while(*p&&*s==*p){++p;++s;}
    if(*p)return 0;
    if(!*s || (*s=='0'&&s[1]))return -1;
    while(*s) {
        if(*s<'0'||*s>'9'||n>=OU_HISTORY_CAPACITY)return -1;
        n=n*10U+(uint32_t)(*s++-'0');
    }
    if(n>=OU_HISTORY_CAPACITY/OU_HISTORY_PAGE_SIZE)return -1;
    *page=n;
    return 1;
}
static void history_page_response(json_writer *w,
                                  const ou_history_log *history,
                                  const ou_snapshot *snapshot,
                                  uint32_t page) {
    size_t i,start,end;
    common(w,"ups.history",snapshot);
    property_bool(w,"persistent",0U,1);
    property_bool(w,"live",snapshot->origin==OU_ORIGIN_NATIVE &&
                       snapshot->sample_ready,1);
    property_string(w,"sampleSource","verified-native-only",1);
    property_uint(w,"sampleIntervalMs",OU_HISTORY_INTERVAL_MS,1);
    property_uint(w,"capacity",OU_HISTORY_CAPACITY,1);
    property_uint(w,"stored",history->count,1);
    property_uint(w,"overwritten",history->overwritten,1);
    property_uint(w,"latestSequence",history->latest_sequence,1);
    property_uint(w,"latestRecordSequence",
       history->count?history->samples[
          ((size_t)history->next_slot+OU_HISTORY_CAPACITY-1U)%
           OU_HISTORY_CAPACITY].sequence:0U,1);
    property_uint(w,"page",page,1);
    property_uint(w,"pageSize",OU_HISTORY_PAGE_SIZE,1);
    property_uint(w,"pages",(history->count+OU_HISTORY_PAGE_SIZE-1U)/
                              OU_HISTORY_PAGE_SIZE,1);
    put_str(w,",\"samples\":[");
    start=(size_t)page*OU_HISTORY_PAGE_SIZE;
    end=start+OU_HISTORY_PAGE_SIZE;
    if(end>history->count)end=history->count;
    for(i=start;i<end;++i){
        size_t slot=((size_t)history->next_slot+
                      OU_HISTORY_CAPACITY-1U-i)%OU_HISTORY_CAPACITY;
        const ou_history_sample *sample=&history->samples[slot];
        if(i!=start)put_char(w,',');
        put_char(w,'{');
        property_uint(w,"sequence",sample->sequence,0);
        property_uint(w,"monotonicMs",sample->monotonic_ms,1);
        property_uint(w,"batteryPercent",sample->battery_percent,1);
        property_uint(w,"loadPercent",sample->load_percent,1);
        property_uint(w,"runtimeSeconds",sample->runtime_seconds,1);
        property_uint(w,"inputDecivolts",sample->input_decivolts,1);
        property_uint(w,"outputDecivolts",sample->output_decivolts,1);
        property_bool(w,"acPresent",sample->ac_present,1);
        property_bool(w,"gapBefore",sample->gap_before,1);
        put_char(w,'}');
    }
    put_char(w,']');
}
static int event_page(const char *path, uint32_t *page) {
    const char prefix[]="/api/v1/events/";
    const char *p=prefix;
    const char *s=path;
    uint32_t n=0U;
    if(same(path,"/api/v1/events")) {
        *page=0U;
        return 1;
    }
    while(*p && *s==*p){++s;++p;}
    if(*p)return 0;
    if(!*s||(*s=='0'&&s[1]))return -1;
    while(*s) {
        if(*s<'0'||*s>'9'||n>100U)return -1;
        n=n*10U+(uint32_t)(*s++-'0');
    }
    if(n>=OU_EVENTS_CAPACITY/OU_EVENTS_PAGE_SIZE)return -1;
    *page=n;
    return 1;
}

static const char *event_code_string(uint8_t code) {
    switch(code) {
        case OU_EVENT_SERVICE_START: return "SERVICE_START";
        case OU_EVENT_TELEMETRY_ONLINE: return "TELEMETRY_ONLINE";
        case OU_EVENT_TELEMETRY_OFFLINE: return "TELEMETRY_OFFLINE";
        case OU_EVENT_REPORT_REJECTED: return "REPORT_REJECTED";
        case OU_EVENT_UTILITY_LOST: return "UTILITY_LOST";
        case OU_EVENT_UTILITY_RESTORED: return "UTILITY_RESTORED";
        case OU_EVENT_BATTERY_LOW: return "BATTERY_LOW";
        case OU_EVENT_BATTERY_RECOVERED: return "BATTERY_RECOVERED";
        default: return "UNKNOWN";
    }
}
static const char *event_message(uint8_t code) {
    switch(code) {
        case OU_EVENT_SERVICE_START: return "Native service initialized";
        case OU_EVENT_TELEMETRY_ONLINE: return "Verified UPS measurements available";
        case OU_EVENT_TELEMETRY_OFFLINE: return "UPS telemetry unavailable";
        case OU_EVENT_REPORT_REJECTED: return "UPS measurement rejected by validation";
        case OU_EVENT_UTILITY_LOST: return "Utility power lost";
        case OU_EVENT_UTILITY_RESTORED: return "Utility power restored";
        case OU_EVENT_BATTERY_LOW: return "Battery charge below 20 percent";
        case OU_EVENT_BATTERY_RECOVERED: return "Battery charge recovered above 20 percent";
        default: return "Unknown event";
    }
}
static const char *event_severity(uint8_t code) {
    return code==OU_EVENT_UTILITY_LOST || code==OU_EVENT_BATTERY_LOW
        ? "warning"
        : code==OU_EVENT_TELEMETRY_OFFLINE || code==OU_EVENT_REPORT_REJECTED
        ? "error" : "info";
}

/* Bounded summary of the current in-memory event ring; explicitly excludes
 * overwritten history. No allocations, external clocks, or flash access. */
static void event_stats(json_writer *w, const ou_event_log *events,
                        const ou_snapshot *snapshot) {
    uint32_t info=0U,warning=0U,error=0U;
    uint32_t codes[OU_EVENT_LAST+1U]={0U};
    size_t i;
    for(i=0U;i<events->count;++i) {
        size_t slot=((size_t)events->next_slot+OU_EVENTS_CAPACITY-1U-i)
                    %OU_EVENTS_CAPACITY;
        uint8_t code=events->records[slot].code;
        if(code>=OU_EVENT_FIRST && code<=OU_EVENT_LAST) {
            const char *severity=event_severity(code);
            ++codes[code];
            if(same(severity,"info"))++info;
            else if(same(severity,"warning"))++warning;
            else ++error;
        }
    }
    common(w,"events.stats",snapshot);
    property_uint(w,"capacity",OU_EVENTS_CAPACITY,1);
    property_uint(w,"stored",events->count,1);
    property_uint(w,"overwritten",events->overwritten,1);
    property_uint(w,"pageSize",OU_EVENTS_PAGE_SIZE,1);
    property_uint(w,"pages",
        (uint32_t)((events->count+OU_EVENTS_PAGE_SIZE-1U)/OU_EVENTS_PAGE_SIZE),1);
    property_uint(w,"currentBootGeneration",events->boot_generation,1);
    property_uint(w,"latestEventId",events->count?events->next_id-1U:0U,1);
    property_uint(w,"oldestEventId",
        events->count?events->next_id-events->count:0U,1);
    property_bool(w,"persistent",0U,1);
    property_string(w,"retention","volatile-until-explicit-checkpoint",1);
    property_uint(w,"info",info,1);
    property_uint(w,"warning",warning,1);
    property_uint(w,"error",error,1);
    put_str(w,",\"byCode\":{");
    for(i=OU_EVENT_FIRST;i<=OU_EVENT_LAST;++i) {
        property_uint(w,event_code_string((uint8_t)i),codes[i],i!=OU_EVENT_FIRST);
    }
    put_char(w,'}');
}

static int parse_schema_page(const char *path, uint32_t *page) {
    const char prefix[] = "/api/v1/configuration/fields/";
    const char *cursor = path;
    const char *start = prefix;
    uint32_t value = 0;
    while (*start && *cursor == *start) { cursor++; start++; }
    if (*start) return 0;
    if (*cursor < '0' || *cursor > '9') return -1;
    if (*cursor == '0' && cursor[1] != '\0') return -1;
    for (; *cursor; cursor++) {
        if (*cursor < '0' || *cursor > '9' || value > 100000U) return -1;
        value = value * 10U + (uint32_t)(*cursor - '0');
    }
    if (value >= (uint32_t)((ou_schema_field_count + 15U) / 16U)) return -1;
    *page = value;
    return 1;
}

ou_snapshot ou_simulated_snapshot(void) {
    ou_snapshot state;
    state.sequence = 1U;
    state.uptime_seconds = 3486U;
    state.battery_percent = 97U;
    state.estimated_runtime_seconds = 1560U;
    state.input_voltage_decivolts = 1190U;
    state.output_voltage_decivolts = 1200U;
    state.output_load_percent = 57U;
    state.ac_present = 1U;
    state.battery_present = 1U;
    state.segment_1_on = 1U;
    state.segment_2_on = 1U;
    state.origin = OU_ORIGIN_SIMULATOR;
    state.sample_ready = 1U;
    return state;
}

ou_snapshot ou_unavailable_snapshot(void) {
    ou_snapshot state = {0};
    state.origin = OU_ORIGIN_UNAVAILABLE;
    state.sample_ready = 0U;
    return state;
}

int ou_dispatch_managed(const char *method, const char *path,
                const ou_snapshot *snapshot, const ou_event_log *events,
                const ou_history_log *history,
                const ou_notify_engine *notifications,
                const ou_agent_status *agents,
                const ou_time_sample *clock,
                const ou_shutdown_advisory *shutdown,
                char *body, size_t capacity, ou_reply *reply) {
    json_writer w;
    uint32_t field_page = 0, events_page = 0, history_index = 0;
    int page_status, events_path, history_path;
    if (!method || !path || !snapshot || !body || !reply || capacity < 2U)
        return -1;
    if (snapshot->origin > OU_ORIGIN_NATIVE || snapshot->sample_ready > 1U ||
        (snapshot->origin == OU_ORIGIN_UNAVAILABLE && snapshot->sample_ready))
        return -1;
    page_status = parse_schema_page(path, &field_page);
    events_path = event_page(path, &events_page);
    history_path = history_page(path,&history_index);
    w.data = body;
    w.capacity = capacity;
    w.length = 0U;
    w.error = 0;
    reply->status = 200;
    reply->content_type = "application/json; charset=utf-8";
    reply->body_length = 0U;

    if (!same(method, "GET") && !same(method, "HEAD")) {
        reply->status = 405;
        common(&w, "error", snapshot);
        property_string(&w, "code", "READ_ONLY", 1);
        property_string(&w, "message", "Firmware development simulator refuses all mutations", 1);
    } else if (same(path, "/api/v1/health")) {
        common(&w, "health", snapshot);
        property_string(&w, "status", "ok", 1);
        property_string(&w, "hardware", snapshot->origin == OU_ORIGIN_NATIVE && snapshot->sample_ready ? "connected" : "not-connected", 1);
        property_bool(&w, "upsDataAvailable", snapshot->sample_ready, 1);
        property_uint(&w, "uptimeSeconds", snapshot->uptime_seconds, 1);
    } else if (same(path, "/api/v1/device")) {
        common(&w, "device", snapshot);
        property_string(&w, "name", "OpenH910P", 1);
        property_string(&w, "firmwareVersion", OU_FIRMWARE_VERSION, 1);
        property_string(&w, "platform", "H910P ARM7TDMI target", 1);
        property_string(&w, "hardwareConnection", "none", 1);
        property_string(&w, "manufacturer", "Independent development project", 1);
    } else if (same(path, "/api/v1/capabilities")) {
        common(&w, "capabilities", snapshot);
        property_bool(&w, "upsTelemetry", snapshot->sample_ready, 1);
        property_bool(&w, "physicalTelemetry", snapshot->origin == OU_ORIGIN_NATIVE && snapshot->sample_ready, 1);
        property_bool(&w, "configurationWritable", 0, 1);
        property_bool(&w, "outletControl", 0, 1);
        property_bool(&w, "firmwareInstall", 0, 1);
        property_bool(&w, "smtpDelivery", 0, 1);
        property_string(&w, "transport", "native-json-c", 1);
        put_str(&w, ",\"legacyOemInventory\":{\"configurationGroups\":24,\"configurationItems\":302,\"objectNames\":420}");
    } else if(history_path!=0) {
        if(history_path<0) {
            reply->status=404;
            common(&w,"error",snapshot);
            property_string(&w,"code","PAGE_NOT_FOUND",1);
        } else if(!history || history->count>OU_HISTORY_CAPACITY ||
           history->next_slot>=OU_HISTORY_CAPACITY) {
            reply->status=503;
            common(&w,"error",snapshot);
            property_string(&w,"code","HISTORY_UNAVAILABLE",1);
        } else {
            history_page_response(&w,history,snapshot,history_index);
        }
    } else if (events && same(path,"/api/v1/events/stats")) {
        if(events->count>OU_EVENTS_CAPACITY ||
           events->next_slot>=OU_EVENTS_CAPACITY ||
           events->next_id==0U || events->boot_generation==0U ||
           (uint32_t)events->count>=events->next_id) {
            reply->status=503;
            common(&w,"error",snapshot);
            property_string(&w,"code","EVENT_LOG_INVALID",1);
        } else {
            event_stats(&w,events,snapshot);
        }
    } else if (events && events_path!=0) {
        if (events_path<0 || events->count>OU_EVENTS_CAPACITY ||
            events->next_slot>=OU_EVENTS_CAPACITY) {
            reply->status=404;
            common(&w,"error",snapshot);
            property_string(&w,"code","PAGE_NOT_FOUND",1);
        } else {
            size_t index = (size_t)events_page*OU_EVENTS_PAGE_SIZE;
            size_t end = index+OU_EVENTS_PAGE_SIZE, i;
            if (end>events->count)end=events->count;
            common(&w,"events",snapshot);
            property_bool(&w,"persistent",0U,1);
            property_string(&w,"retention","volatile-until-explicit-checkpoint",1);
            property_uint(&w,"currentBootGeneration",events->boot_generation,1);
            property_uint(&w,"capacity",OU_EVENTS_CAPACITY,1);
            property_uint(&w,"stored",events->count,1);
            property_uint(&w,"overwritten",events->overwritten,1);
            property_uint(&w,"latestEventId",
                      events->count?events->next_id-1U:0U,1);
            property_uint(&w,"page",events_page,1);
            property_uint(&w,"pageSize",OU_EVENTS_PAGE_SIZE,1);
            property_uint(&w,"pages",
                   (uint32_t)((events->count+OU_EVENTS_PAGE_SIZE-1U)/
                              OU_EVENTS_PAGE_SIZE),1);
            put_str(&w,",\"events\":[");
            for(i=index;i<end;++i) {
                size_t offset = ((size_t)events->next_slot+
                                 OU_EVENTS_CAPACITY-1U-i)%OU_EVENTS_CAPACITY;
                const ou_event_record *entry=&events->records[offset];
                if(i>index)put_char(&w,',');
                put_char(&w,'{');
                property_uint(&w,"id",entry->id,0);
                property_uint(&w,"monotonicMs",entry->monotonic_ms,1);
                property_uint(&w,"bootGeneration",entry->boot_generation,1);
                property_string(&w,"code",event_code_string(entry->code),1);
                property_string(&w,"severity",event_severity(entry->code),1);
                property_string(&w,"message",event_message(entry->code),1);
                put_char(&w,'}');
            }
            put_char(&w,']');
        }
    } else if (same(path, "/api/v1/shutdown/schema")) {
        common(&w,"shutdown.schema",snapshot);
        property_bool(&w,"policyModelCompiled",1U,1);
        property_bool(&w,"policyWritable",0U,1);
        property_bool(&w,"activePolicyAvailable",0U,1);
        property_bool(&w,"outputControlQualified",0U,1);
        property_uint(&w,"outlets",3U,1);
        property_uint(&w,"minimumShutdownDurationSeconds",120U,1);
        property_uint(&w,"maximumShutdownDurationSeconds",9999U,1);
        property_uint(&w,"maximumMasterTimerSeconds",5999940U,1);
        property_uint(&w,"masterTimerStepSeconds",60U,1);
        property_uint(&w,"maximumRuntimeThresholdSeconds",99999U,1);
        property_uint(&w,"maximumOutletTimerSeconds",99999U,1);
        property_uint(&w,"maximumOutletStartupSeconds",65535U,1);
        property_uint(&w,"maximumCapacityPercent",100U,1);
        property_string(&w,"mode","local-review-only",1);
    } else if (same(path, "/api/v1/shutdown/advisory")) {
        if(!shutdown){
            reply->status=503;
            common(&w,"error",snapshot);
            property_string(&w,"code","SHUTDOWN_POLICY_OR_TELEMETRY_UNAVAILABLE",1);
        }else {
            common(&w,"shutdown.advisory",snapshot);
            property_uint(&w,"flags",shutdown->flags,1);
            property_bool(&w,"recommendAgentWarning",shutdown->recommend_agent_warning,1);
            property_bool(&w,"powerCommandsEnabled",0U,1);
            property_bool(&w,"physicalShutdownExecuted",0U,1);
            property_string(&w,"basis","verified-native-telemetry-and-qualified-policy",1);
        }
    } else if (same(path, "/api/v1/time/capabilities")) {
        common(&w,"time.capabilities",snapshot);
        property_bool(&w,"ntpWireClientCompiled",1U,1);
        property_bool(&w,"trustedLocalNtpSourceConfigured",0U,1);
        property_bool(&w,"secureNonceProviderQualified",0U,1);
        property_bool(&w,"monotonicTimerQualified",0U,1);
        property_bool(&w,"ntpSourceAclQualified",0U,1);
        property_bool(&w,"networkTimeSecurityQualified",0U,1);
        property_bool(&w,"rtcWritable",0U,1);
        property_bool(&w,"timezoneConfigured",0U,1);
        property_bool(&w,"dstPolicyQualified",0U,1);
        property_uint(&w,"maximumResponseDelayMs",OU_NTP_MAX_RTT_MS,1);
        property_uint(&w,"maximumSampleAgeSeconds",OU_NTP_MAX_AGE_MS/1000U,1);
        property_string(&w,"timeBasis","UTC",1);
        property_string(&w,"mode","trusted-lan-ntp-bootstrap-only",1);
    } else if (same(path, "/api/v1/time/status")) {
        if(!clock || clock->source_verified!=1U){
            reply->status=503;
            common(&w,"error",snapshot);
            property_string(&w,"code","TIME_NOT_SYNCHRONIZED",1);
        }else{
            common(&w,"time.status",snapshot);
            property_uint(&w,"utcUnixSeconds",(uint32_t)clock->utc_unix_seconds,1);
            property_uint(&w,"utcMilliseconds",clock->utc_millisecond,1);
            property_uint(&w,"roundTripMs",clock->round_trip_ms,1);
            property_uint(&w,"uncertaintyMs",clock->uncertainty_ms,1);
            property_bool(&w,"sourceVerified",1U,1);
            property_bool(&w,"rtcSet",0U,1);
            property_string(&w,"timezone","UTC",1);
        }
    } else if (same(path, "/api/v1/agents/capabilities")) {
        common(&w,"agents.capabilities",snapshot);
        property_bool(&w,"coordinatorCompiled",1U,1);
        property_bool(&w,"registrationWritable",0U,1);
        property_bool(&w,"notificationTransportQualified",0U,1);
        property_bool(&w,"mutualTlsQualified",0U,1);
        property_bool(&w,"trustedShutdownIntentAvailable",0U,1);
        property_bool(&w,"physicalPowerControl",0U,1);
        property_uint(&w,"maximumClients",OU_AGENT_MAX,1);
        property_uint(&w,"minimumLeaseSeconds",OU_AGENT_MIN_LEASE_MS/1000U,1);
        property_uint(&w,"maximumLeaseSeconds",OU_AGENT_MAX_LEASE_MS/1000U,1);
        property_uint(&w,"minimumWarningSeconds",OU_AGENT_MIN_WARNING_MS/1000U,1);
        property_uint(&w,"maximumWarningSeconds",OU_AGENT_MAX_WARNING_MS/1000U,1);
        property_string(&w,"mode","local-review-only",1);
    } else if (same(path, "/api/v1/agents/status")) {
        if(!agents){
            reply->status=503;
            common(&w,"error",snapshot);
            property_string(&w,"code","AGENT_COORDINATOR_UNAVAILABLE",1);
        }else {
            common(&w,"agents.status",snapshot);
            property_uint(&w,"enrolled",agents->enrolled,1);
            property_uint(&w,"online",agents->online,1);
            property_uint(&w,"waiting",agents->waiting,1);
            property_uint(&w,"acknowledged",agents->acknowledged,1);
            property_uint(&w,"unreachable",agents->unreachable,1);
            property_bool(&w,"warningActive",agents->warning_active,1);
            property_bool(&w,"persistent",0U,1);
        }
    } else if (same(path, "/api/v1/snmp/capabilities")) {
        common(&w,"snmp.capabilities",snapshot);
        property_bool(&w,"readOnlyAgentCompiled",1U,1);
        property_bool(&w,"listenerEnabled",0U,1);
        property_bool(&w,"v2cPacketCodec",1U,1);
        property_bool(&w,"getBulkSupported",1U,1);
        property_uint(&w,"maximumBulkResponses",8U,1);
        property_bool(&w,"v3UsmAuthPrivQualified",0U,1);
        property_bool(&w,"communityProvisioned",0U,1);
        property_bool(&w,"sourceAclQualified",0U,1);
        property_bool(&w,"verifiedNativeTelemetryRequired",1U,1);
        property_bool(&w,"setOperationsAllowed",0U,1);
        property_uint(&w,"maximumVarbinds",8U,1);
        property_uint(&w,"supportedOids",7U,1);
        property_string(&w,"transport","qualified-udp-adapter-required",1);
        put_str(&w,",\"oids\":[");
        put_str(&w,"{\"oid\":\"1.3.6.1.2.1.33.1.2.1.0\",\"label\":\"Battery status\"},");
        put_str(&w,"{\"oid\":\"1.3.6.1.2.1.33.1.2.3.0\",\"label\":\"Runtime remaining (minutes)\"},");
        put_str(&w,"{\"oid\":\"1.3.6.1.2.1.33.1.2.4.0\",\"label\":\"Battery charge (%)\"},");
        put_str(&w,"{\"oid\":\"1.3.6.1.2.1.33.1.3.3.1.3.1\",\"label\":\"Input voltage (V)\"},");
        put_str(&w,"{\"oid\":\"1.3.6.1.2.1.33.1.4.1.0\",\"label\":\"Output power source\"},");
        put_str(&w,"{\"oid\":\"1.3.6.1.2.1.33.1.4.4.1.2.1\",\"label\":\"Output voltage (V)\"},");
        put_str(&w,"{\"oid\":\"1.3.6.1.2.1.33.1.4.4.1.5.1\",\"label\":\"Output load (%)\"}]");
    } else if (same(path, "/api/v1/backup/capabilities")) {
        common(&w,"backup.capabilities",snapshot);
        property_bool(&w,"nativePortableCodecCompiled",1U,1);
        property_bool(&w,"readOnlyPreviewAvailable",1U,1);
        property_bool(&w,"liveDeviceBackupAvailable",0U,1);
        property_bool(&w,"restoreAvailable",0U,1);
        property_bool(&w,"multiPartitionCommitQualified",0U,1);
        property_bool(&w,"credentialsIncluded",0U,1);
        property_bool(&w,"secretsIncluded",0U,1);
        property_bool(&w,"checksumIsSignature",0U,1);
        property_uint(&w,"formatVersion",1U,1);
        property_uint(&w,"maximumBytes",1536U,1);
        put_str(&w,",\"sections\":[\"configuration\",\"shutdown\",\"weekly\"]");
        property_string(&w,"mode","local-workspace-export-import-only",1);
    } else if (same(path, "/api/v1/firmware/update-policy")) {
        common(&w,"firmware.update-policy",snapshot);
        property_string(&w,"releaseProvider","github",1);
        property_string(&w,"repository","bradjtrammell/dell-nmc-firmware",1);
        property_string(&w,"runningVersion",OU_FIRMWARE_VERSION,1);
        property_string(&w,"hardwareTarget","H910P",1);
        property_bool(&w,"releaseCheckEnabled",1U,1);
        property_bool(&w,"nativeSha256Compiled",1U,1);
        property_bool(&w,"signedManifestGateCompiled",1U,1);
        property_bool(&w,"signatureKeysQualified",0U,1);
        property_bool(&w,"flashLayoutQualified",0U,1);
        property_bool(&w,"recoveryQualified",0U,1);
        property_bool(&w,"deviceHttpsQualified",0U,1);
        property_bool(&w,"installEnabled",0U,1);
        property_bool(&w,"rollbackEnabled",0U,1);
        property_bool(&w,"automaticUpdateEnabled",0U,1);
        property_string(&w,"mode","release-check-only",1);
    } else if (same(path, "/api/v1/notifications/policy")) {
        common(&w,"notifications.policy",snapshot);
        property_bool(&w,"routerCompiled",1U,1);
        property_bool(&w,"deliveryAvailable",0U,1);
        property_bool(&w,"rulesWritable",0U,1);
        property_bool(&w,"destinationsProvisioned",0U,1);
        property_bool(&w,"transportQualified",0U,1);
        property_bool(&w,"credentialsConfigured",0U,1);
        property_uint(&w,"maximumRules",8U,1);
        property_uint(&w,"queueCapacity",24U,1);
        property_uint(&w,"maximumAttempts",5U,1);
        property_uint(&w,"maximumCooldownSeconds",3600U,1);
        property_string(&w,"mode","local-draft-export-only",1);
        put_str(&w,",\"transports\":[\"email\",\"webhook\",\"snmp-trap\"]");
        put_str(&w,",\"events\":[");
        put_str(&w,"\"SERVICE_START\",\"TELEMETRY_ONLINE\",\"TELEMETRY_OFFLINE\",");
        put_str(&w,"\"REPORT_REJECTED\",\"UTILITY_LOST\",\"UTILITY_RESTORED\",");
        put_str(&w,"\"BATTERY_LOW\",\"BATTERY_RECOVERED\"]");
    } else if (same(path, "/api/v1/notifications/status")) {
        if(!notifications || notifications->initialized!=1U) {
            reply->status=503;
            common(&w,"error",snapshot);
            property_string(&w,"code","NOTIFICATION_ENGINE_UNAVAILABLE",1);
        } else {
            size_t index;
            uint32_t pending=0U,activeRules=0U;
            for(index=0U;index<OU_NOTIFY_QUEUE;++index)
                if(notifications->used[index]==1U)++pending;
            for(index=0U;index<OU_NOTIFY_RULES;++index)
                if(notifications->rules[index].enabled==1U)++activeRules;
            common(&w,"notifications.status",snapshot);
            property_uint(&w,"pending",pending,1);
            property_uint(&w,"activeRules",activeRules,1);
            property_uint(&w,"enqueued",notifications->enqueued,1);
            property_uint(&w,"delivered",notifications->delivered,1);
            property_uint(&w,"failed",notifications->failed,1);
            property_uint(&w,"retried",notifications->retried,1);
            property_uint(&w,"dropped",notifications->dropped,1);
            property_bool(&w,"deliveryQualified",0U,1);
            property_string(&w,"retention","volatile",1);
        }
    } else if (same(path, "/api/v1/schedule/schema")) {
        common(&w,"schedule.schema",snapshot);
        property_bool(&w,"weeklyPlannerCompiled",1U,1);
        property_bool(&w,"scheduleWritable",0U,1);
        property_bool(&w,"activeScheduleAvailable",0U,1);
        property_bool(&w,"rtcQualified",0U,1);
        property_bool(&w,"powerControlQualified",0U,1);
        property_bool(&w,"persistent",0U,1);
        property_uint(&w,"dayCount",7U,1);
        property_uint(&w,"slotSeconds",1800U,1);
        property_uint(&w,"slotsPerDay",48U,1);
        property_string(&w,"disabledValue","-1",1);
        property_string(&w,"mode","local-draft-export-only",1);
    } else if (same(path, "/api/v1/security/capabilities")) {
        /* The authentication engine is compiled and tested, but no verified
         * TLS, credential verifier, clock or CSPRNG exists on H910P yet.
         * Do not expose pretend logins or unprotected credentials. */
        common(&w,"security.capabilities",snapshot);
        property_bool(&w,"sessionEngineCompiled",1U,1);
        property_bool(&w,"authenticationAvailable",0U,1);
        property_bool(&w,"loginEnabled",0U,1);
        property_bool(&w,"tlsQualified",0U,1);
        property_bool(&w,"entropyQualified",0U,1);
        property_bool(&w,"credentialStoreQualified",0U,1);
        property_bool(&w,"mutationsAuthorized",0U,1);
        property_uint(&w,"maximumSessions",4U,1);
        property_uint(&w,"idleTimeoutSeconds",900U,1);
        property_uint(&w,"absoluteTimeoutSeconds",28800U,1);
        property_uint(&w,"maximumFailedAttempts",5U,1);
        property_uint(&w,"lockoutSeconds",60U,1);
        property_bool(&w,"csrfRequiredForWrites",1U,1);
        property_bool(&w,"increasingRequestIdRequired",1U,1);
        property_string(&w,"status","provider-unqualified",1);
    } else if (same(path, "/api/v1/configuration/staging")) {
        /* Never return current network addresses or unprotected secrets.
         * No credential- or flash-qualification available to HTTP yet. */
        common(&w,"configuration.staging",snapshot);
        property_bool(&w,"available",1U,1);
        property_bool(&w,"configurationWritable",0U,1);
        property_bool(&w,"authorizationAvailable",0U,1);
        property_bool(&w,"storageQualified",0U,1);
        property_bool(&w,"activeValuesAvailable",0U,1);
        property_string(&w,"mode","local-draft-export-only",1);
        property_uint(&w,"supportedFields",9U,1);
        put_str(&w,",\"fields\":[");
        put_str(&w,"{\"name\":\"System.Network.HostName\",\"label\":\"Host name\",\"type\":\"hostname\",\"max\":49},");
        put_str(&w,"{\"name\":\"System.Network.DomainName\",\"label\":\"Domain name\",\"type\":\"domain\",\"max\":49},");
        put_str(&w,"{\"name\":\"System.Network.DHCP\",\"label\":\"IPv4 assignment\",\"type\":\"dhcp\"},");
        put_str(&w,"{\"name\":\"System.Network.IPAddress\",\"label\":\"Static IPv4 address\",\"type\":\"ipv4\"},");
        put_str(&w,"{\"name\":\"System.Network.IPMask\",\"label\":\"IPv4 subnet mask\",\"type\":\"netmask\"},");
        put_str(&w,"{\"name\":\"System.Network.IPGateway\",\"label\":\"IPv4 default gateway\",\"type\":\"ipv4\"},");
        put_str(&w,"{\"name\":\"System.Contact\",\"label\":\"System contact\",\"type\":\"string\",\"max\":49},");
        put_str(&w,"{\"name\":\"System.Location\",\"label\":\"Physical location\",\"type\":\"string\",\"max\":31},");
        put_str(&w,"{\"name\":\"System.Language\",\"label\":\"Interface language\",\"type\":\"language\"}");
        put_char(&w,']');
    } else if (same(path, "/api/v1/configuration/fields")) {
        common(&w, "configuration.fields", snapshot);
        property_uint(&w, "total", (uint32_t)ou_schema_field_count, 1);
        property_uint(&w, "pageSize", 16U, 1);
        property_uint(&w, "pages", (uint32_t)((ou_schema_field_count + 15U) / 16U), 1);
        property_bool(&w, "configurationWritable", 0, 1);
        property_string(&w, "dataClassification", "metadata-only", 1);
    } else if ((same(path, "/api/v1/ups/status") ||
                same(path, "/api/v1/ups/measurements") ||
                same(path, "/api/v1/ups/outlets") ||
                same(path, "/api/v1/events")) && !snapshot->sample_ready) {
        reply->status = 503;
        common(&w, "error", snapshot);
        property_string(&w, "code", "TELEMETRY_UNAVAILABLE", 1);
        property_string(&w, "message", "No verified UPS driver sample available", 1);
    } else if (same(path, "/api/v1/ups/status")) {
        common(&w, "ups.status", snapshot);
        property_string(&w, "powerSource", snapshot->ac_present ? "utility" : "battery", 1);
        property_bool(&w, "batteryPresent", snapshot->battery_present, 1);
        property_uint(&w, "batteryPercent", snapshot->battery_percent, 1);
        property_uint(&w, "estimatedRuntimeSeconds", snapshot->estimated_runtime_seconds, 1);
        property_uint(&w, "loadPercent", snapshot->output_load_percent, 1);
    } else if (same(path, "/api/v1/ups/measurements")) {
        common(&w, "ups.measurements", snapshot);
        put_str(&w, ",\"inputVoltage\":{");
        property_uint(&w, "valueDecivolts", snapshot->input_voltage_decivolts, 0);
        property_string(&w, "unit", "dV", 1);
        put_str(&w, "},\"outputVoltage\":{");
        property_uint(&w, "valueDecivolts", snapshot->output_voltage_decivolts, 0);
        property_string(&w, "unit", "dV", 1);
        put_char(&w, '}');
    } else if (same(path, "/api/v1/ups/outlets")) {
        common(&w, "ups.outlets", snapshot);
        put_str(&w, ",\"outlets\":[");
        one_outlet(&w, 1U, "UPS output", 1U, 0);
        one_outlet(&w, 2U, "Load segment 1", snapshot->segment_1_on, 1);
        one_outlet(&w, 3U, "Load segment 2", snapshot->segment_2_on, 1);
        put_char(&w, ']');
    } else if (same(path, "/api/v1/events") && snapshot->origin == OU_ORIGIN_NATIVE) {
        reply->status = 503;
        common(&w, "error", snapshot);
        property_string(&w, "code", "EVENT_DRIVER_UNAVAILABLE", 1);
        property_string(&w, "message", "No native UPS event driver attached", 1);
    } else if (same(path, "/api/v1/events")) {
        common(&w, "events", snapshot);
        put_str(&w, ",\"events\":[");
        put_str(&w, "{\"id\":1,\"severity\":\"info\",\"message\":\"Simulator started; no UPS commands are enabled\"},");
        put_str(&w, "{\"id\":2,\"severity\":\"info\",\"message\":\"Native JSON API available in development mode\"}]");
    } else if (page_status != 0) {
        if (page_status < 0) {
            reply->status = 404;
            common(&w, "error", snapshot);
            property_string(&w, "code", "PAGE_NOT_FOUND", 1);
        } else {
            common(&w, "configuration.fields", snapshot);
            property_uint(&w, "total", (uint32_t)ou_schema_field_count, 1);
            property_uint(&w, "page", field_page, 1);
            property_uint(&w, "pageSize", 16U, 1);
            property_bool(&w, "configurationWritable", 0, 1);
            put_str(&w, ",\"items\":[");
            size_t start = (size_t)field_page * 16U;
            size_t end = start + 16U;
            if (end > ou_schema_field_count) end = ou_schema_field_count;
            for (size_t i = start; i < end; i++) {
                const ou_schema_field *field = &ou_schema_fields[i];
                if (i != start) put_char(&w, ',');
                put_char(&w, '{');
                property_string(&w, "group", field->group, 0);
                property_string(&w, "name", field->name, 1);
                property_string(&w, "type", field->type_expression, 1);
                property_string(&w, "access", field->access, 1);
                property_string(&w, "unit", field->unit, 1);
                property_bool(&w, "requiresRestart", field->requires_restart, 1);
                property_bool(&w, "hiddenInOEM", field->hidden_in_oem, 1);
                put_char(&w, '}');
            }
            put_char(&w, ']');
        }
    } else {
        reply->status = 404;
        common(&w, "error", snapshot);
        property_string(&w, "code", "NOT_FOUND", 1);
        property_string(&w, "message", "Unknown API resource", 1);
    }
    put_char(&w, '}');
    if (w.error) { body[0] = '\0'; return -1; }
    body[w.length] = '\0';
    reply->body_length = w.length;
    return 0;
}

int ou_dispatch_observed(const char *method, const char *path,
                const ou_snapshot *snapshot, const ou_event_log *events,
                const ou_history_log *history,
                char *body, size_t capacity, ou_reply *reply) {
    return ou_dispatch_managed(method,path,snapshot,events,history,0,0,0,0,
                               body,capacity,reply);
}

int ou_dispatch_events(const char *method, const char *path,
                const ou_snapshot *snapshot, const ou_event_log *events,
                char *body, size_t capacity, ou_reply *reply) {
    return ou_dispatch_observed(method,path,snapshot,events,0,
                                body,capacity,reply);
}

int ou_dispatch(const char *method, const char *path,
                const ou_snapshot *snapshot, char *body,
                size_t capacity, ou_reply *reply) {
    return ou_dispatch_events(method,path,snapshot,0,body,capacity,reply);
}
