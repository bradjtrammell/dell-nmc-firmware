#include "ou_native_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t clock_ms=1000U;
static char response[OU_HTTP_MAX_RESPONSE+1U];

static void http(const char *method,const char *path,int code,
                 const char *must_have,const char *must_not) {
    uint8_t request[256];char prefix[32];
    size_t written=0U;
    int len=snprintf((char *)request,sizeof(request),
                     "%s %s HTTP/1.0\r\n\r\n",method,path);
    assert(len>0&&(size_t)len<sizeof(request));
    assert(ou_native_service_http(request,(size_t)len,clock_ms,response,
            sizeof(response)-1U,&written)==0);
    assert(written<sizeof(response));
    response[written]='\0';
    snprintf(prefix,sizeof(prefix),"HTTP/1.1 %d",code);
    assert(strstr(response,prefix)==response);
    if(must_have)assert(strstr(response,must_have)!=0);
    if(must_not)assert(strstr(response,must_not)==0);
}
static ou_driver_report report(uint32_t sequence,uint8_t ac,
                                uint16_t battery) {
    ou_driver_report r={0};
    r.version=OU_REPORT_VERSION;r.verified=1U;
    r.sequence=sequence;
    r.received_monotonic_ms=clock_ms;
    r.uptime_seconds=clock_ms/1000U;
    r.utility_present=ac;
    r.battery_present=1U;
    r.battery_percent=battery;
    r.runtime_seconds=1800U;
    r.load_percent=40U;
    r.input_decivolts=1190U;
    r.output_decivolts=1200U;
    r.segment_1_on=1U;
    return r;
}
static void state_edges_and_loss(void) {
    ou_driver_report r;
    ou_native_service_init();
    http("GET","/api/v1/ups/status",503,"TELEMETRY_UNAVAILABLE",0);
    http("GET","/api/v1/events",200,"\"code\":\"SERVICE_START\"",
         "EVENT_DRIVER_UNAVAILABLE");
    http("GET","/api/v1/events",200,"\"persistent\":false",
         "\"simulated\":true");
    r=report(1U,1U,80U);
    assert(ou_native_service_driver_report(&r,clock_ms)==0);
    http("GET","/api/v1/events",200,"\"code\":\"TELEMETRY_ONLINE\"",0);
    assert(strstr(response,"\"code\":\"UTILITY_LOST\"")==0);
    clock_ms+=100U;r=report(2U,0U,19U);
    assert(ou_native_service_driver_report(&r,clock_ms)==0);
    http("GET","/api/v1/events",200,"\"code\":\"UTILITY_LOST\"",
         0);
    assert(strstr(response,"\"code\":\"BATTERY_LOW\"")!=0);
    clock_ms+=100U;r=report(3U,0U,15U);
    assert(ou_native_service_driver_report(&r,clock_ms)==0);
    http("GET","/api/v1/events",200,"\"stored\":4",0);
    clock_ms+=100U;r=report(4U,1U,36U);
    assert(ou_native_service_driver_report(&r,clock_ms)==0);
    http("GET","/api/v1/events",200,"\"code\":\"UTILITY_RESTORED\"",0);
    assert(strstr(response,"\"code\":\"BATTERY_RECOVERED\"")!=0);
    ou_native_service_source_lost_at(++clock_ms);
    http("GET","/api/v1/ups/status",503,"TELEMETRY_UNAVAILABLE",0);
    http("GET","/api/v1/events",200,"\"code\":\"TELEMETRY_OFFLINE\"",0);
    assert(strstr(response,"\"monotonicMs\":1301")!=0);
    ou_native_service_source_lost_at(clock_ms+1U);
    http("GET","/api/v1/events",200,"\"stored\":7",0);
    assert(ou_native_service_driver_report(&r,clock_ms+2U)==-1);
    http("GET","/api/v1/events",200,"\"code\":\"REPORT_REJECTED\"",0);
    clock_ms+=3U;r=report(5U,1U,45U);
    assert(ou_native_service_driver_report(&r,clock_ms)==0);
    http("GET","/api/v1/events",200,"\"code\":\"TELEMETRY_ONLINE\"",0);
    http("GET","/api/v1/ups/status",200,"\"batteryPercent\":45",0);
}
static void ring_and_pagination(void) {
    unsigned i;
    ou_driver_report r;
    for(i=0U;i<50U;++i) {
        clock_ms+=1000U;
        r=report(6U+i,(uint8_t)(i&1U),44U);
        assert(ou_native_service_driver_report(&r,clock_ms)==0);
    }
    http("GET","/api/v1/events",200,"\"stored\":32",0);
    assert(strstr(response,"\"overwritten\":")!=0);
    assert(strstr(response,"\"page\":0")!=0);
    assert(strstr(response,"\"pages\":4")!=0);
    assert(strstr(response,"\"code\":\"SERVICE_START\"")==0);
    http("GET","/api/v1/events/1",200,"\"page\":1",0);
    http("GET","/api/v1/events/2",200,"\"page\":2",0);
    http("GET","/api/v1/events/3",200,"\"page\":3",0);
    http("GET","/api/v1/events/4",404,"PAGE_NOT_FOUND",0);
    http("GET","/api/v1/events/01",404,"PAGE_NOT_FOUND",0);
    http("GET","/api/v1/events/-1",404,"PAGE_NOT_FOUND",0);
    http("POST","/api/v1/events",405,"READ_ONLY",0);
    http("HEAD","/api/v1/events",200,"Content-Length:",0);
    assert(strstr(response,"\"events\"")==0);
}
int main(void) {
    state_edges_and_loss();
    ring_and_pagination();
    puts("PASS: native typed event history, UPS transition edges, loss, paging, wrap, GET/HEAD and read-only");
    return 0;
}
