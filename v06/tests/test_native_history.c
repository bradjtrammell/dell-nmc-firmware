#include "ou_native_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char response[OU_HTTP_MAX_RESPONSE+1U];
static void http(const char *method,const char *route,uint32_t now,
                 int status,const char *required) {
    uint8_t request[180U];
    size_t length=0U;
    char prefix[24U];
    int n=snprintf((char *)request,sizeof(request),
                   "%s %s HTTP/1.0\r\n\r\n",method,route);
    assert(n>0&&(size_t)n<sizeof(request));
    assert(ou_native_service_http(request,(size_t)n,now,response,
                                  sizeof(response)-1U,&length)==0);
    assert(length<sizeof(response));
    response[length]='\0';
    snprintf(prefix,sizeof(prefix),"HTTP/1.1 %d",status);
    assert(strstr(response,prefix)==response);
    if(required)assert(strstr(response,required)!=0);
}
static ou_driver_report make_report(uint32_t seq,uint32_t ms,
                                     uint8_t ac,uint8_t battery) {
    ou_driver_report r={0};
    r.version=OU_REPORT_VERSION;r.verified=1U;
    r.sequence=seq;r.received_monotonic_ms=ms;
    r.uptime_seconds=ms/1000U;r.utility_present=ac;r.battery_present=1U;
    r.battery_percent=battery;r.load_percent=40U;
    r.input_decivolts=1190U;r.output_decivolts=1200U;
    r.runtime_seconds=1800U;r.segment_1_on=1U;
    return r;
}
static void lifecycle(void) {
    ou_driver_report r;
    uint32_t seq;
    ou_native_service_init();
    http("GET","/api/v1/ups/history",10U,200,"\"stored\":0");
    assert(strstr(response,"\"live\":false")!=0);
    assert(strstr(response,"\"samples\":[]")!=0);
    http("GET","/api/v1/ups/history/8",10U,404,"PAGE_NOT_FOUND");
    http("GET","/api/v1/ups/history/01",10U,404,"PAGE_NOT_FOUND");
    http("GET","/api/v1/ups/history/foo",10U,404,"PAGE_NOT_FOUND");
    http("POST","/api/v1/ups/history",10U,405,"READ_ONLY");
    r=make_report(1U,1000U,1U,90U);
    assert(ou_native_service_driver_report(&r,1000U)==0);
    http("GET","/api/v1/ups/history",1000U,200,"\"stored\":1");
    assert(strstr(response,"\"live\":true")!=0);
    assert(strstr(response,"\"latestRecordSequence\":1")!=0);
    assert(strstr(response,"\"batteryPercent\":90")!=0);
    r=make_report(2U,2000U,1U,90U);
    assert(ou_native_service_driver_report(&r,2000U)==0);
    http("GET","/api/v1/ups/history",2000U,200,"\"stored\":1");
    assert(strstr(response,"\"latestSequence\":2")!=0);
    assert(strstr(response,"\"latestRecordSequence\":1")!=0);
    /* AC edge bypasses 60-second cadence and is never manufactured. */
    r=make_report(3U,3000U,0U,88U);
    assert(ou_native_service_driver_report(&r,3000U)==0);
    http("GET","/api/v1/ups/history",3000U,200,"\"stored\":2");
    assert(strstr(response,"\"acPresent\":false")!=0);
    ou_native_service_source_lost_at(3001U);
    http("GET","/api/v1/ups/status",3001U,503,"TELEMETRY_UNAVAILABLE");
    http("GET","/api/v1/ups/history",3001U,200,"\"stored\":2");
    assert(strstr(response,"\"live\":false")!=0);
    r=make_report(4U,4000U,0U,87U);
    assert(ou_native_service_driver_report(&r,4000U)==0);
    http("GET","/api/v1/ups/history",4000U,200,"\"gapBefore\":true");
    assert(strstr(response,"\"stored\":3")!=0);
    /* Invalid frame cannot enter native history, and flags a gap. */
    r=make_report(5U,5000U,0U,86U);
    r.verified=0U;
    assert(ou_native_service_driver_report(&r,5000U)==-1);
    http("GET","/api/v1/ups/history",5000U,200,"\"stored\":3");
    r=make_report(6U,6000U,1U,85U);
    assert(ou_native_service_driver_report(&r,6000U)==0);
    http("GET","/api/v1/ups/history",6000U,200,"\"stored\":4");
    for(seq=7U;seq<=76U;++seq){
        uint32_t ms=66000U+(seq-7U)*61000U;
        r=make_report(seq,ms,1U,(uint8_t)(90U-(seq%20U)));
        assert(ou_native_service_driver_report(&r,ms)==0);
    }
    http("GET","/api/v1/ups/history",5000000U,200,"\"stored\":64");
    assert(strstr(response,"\"overwritten\":10")!=0);
    assert(strstr(response,"\"pages\":8")!=0);
    assert(strstr(response,"\"live\":false")!=0);
    http("GET","/api/v1/ups/history/7",5000000U,200,"\"page\":7");
    http("HEAD","/api/v1/ups/history/7",5000000U,200,"Content-Length:");
    assert(strstr(response,"\"samples\"")==0);
    /* A reset invalidates the in-memory history; it is not persistent. */
    ou_native_service_init();
    http("GET","/api/v1/ups/history",100U,200,"\"stored\":0");
}
int main(void) {
    lifecycle();
    puts("PASS: native 64-sample history API, rate, outages, gaps, validation, 8 pages and 503 isolation");
    return 0;
}
