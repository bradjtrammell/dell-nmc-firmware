#include "ou_native_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void request(const char *method,const char *path,uint32_t now,
                    int expected,const char *contains){
    char q[192U],reply[OU_HTTP_MAX_RESPONSE+1U],first[25U];
    size_t written=0U;
    int n=snprintf(q,sizeof(q),"%s %s HTTP/1.0\r\n\r\n",method,path);
    assert(n>0&&(size_t)n<sizeof(q));
    assert(ou_native_service_http((const uint8_t *)q,(size_t)n,now,reply,
                                  sizeof(reply)-1U,&written)==0);
    reply[written]=0U;
    (void)snprintf(first,sizeof(first),"HTTP/1.1 %d",expected);
    assert(strstr(reply,first)==reply);
    assert(strstr(reply,contains)!=0);
}
static ou_driver_report report(uint32_t sequence,uint32_t ms,uint8_t utility,
                               uint16_t battery,uint32_t runtime){
    ou_driver_report r={0};
    r.version=OU_REPORT_VERSION;r.verified=1U;
    r.sequence=sequence;r.received_monotonic_ms=ms;
    r.uptime_seconds=ms/1000U;r.runtime_seconds=runtime;
    r.battery_percent=battery;r.load_percent=25U;
    r.input_decivolts=1200U;r.output_decivolts=1200U;
    r.utility_present=utility;r.battery_present=1U;
    return r;
}
int main(void){
    ou_shutdown_policy p;
    ou_driver_report r;
    ou_native_service_init();
    request("GET","/api/v1/shutdown/schema",0U,200,"\"outlets\":3");
    request("GET","/api/v1/shutdown/advisory",0U,503,
            "SHUTDOWN_POLICY_OR_TELEMETRY_UNAVAILABLE");
    ou_shutdown_policy_init(&p);
    p.runtime_limit_seconds=1800U;
    p.capacity_limit_percent=40U;
    p.restart_level_percent=70U;
    p.segments[0].capacity_limit_percent=30U;
    assert(ou_native_service_shutdown_bind(&p,0,1)==-1);
    assert(ou_native_service_shutdown_bind(&p,1,0)==-1);
    assert(ou_native_service_shutdown_bind(&p,1,1)==0);
    request("GET","/api/v1/shutdown/advisory",1U,503,
            "SHUTDOWN_POLICY_OR_TELEMETRY_UNAVAILABLE");
    r=report(1U,1000U,0U,35U,900U);
    assert(ou_native_service_driver_report(&r,1000U)==0);
    request("GET","/api/v1/shutdown/advisory",1001U,200,
            "\"recommendAgentWarning\":true");
    request("GET","/api/v1/shutdown/advisory",1001U,200,
            "\"flags\":7");
    request("GET","/api/v1/shutdown/advisory",1001U,200,
            "\"powerCommandsEnabled\":false");
    request("POST","/api/v1/shutdown/advisory",1001U,405,"READ_ONLY");
    r=report(2U,2000U,1U,80U,900U);
    assert(ou_native_service_driver_report(&r,2000U)==0);
    request("GET","/api/v1/shutdown/advisory",2001U,200,
            "\"flags\":32");
    request("GET","/api/v1/shutdown/advisory",2001U,200,
            "\"recommendAgentWarning\":false");
    ou_native_service_source_lost_at(3000U);
    request("GET","/api/v1/shutdown/advisory",3001U,503,
            "SHUTDOWN_POLICY_OR_TELEMETRY_UNAVAILABLE");
    ou_native_service_init();
    request("GET","/api/v1/shutdown/advisory",3001U,503,
            "SHUTDOWN_POLICY_OR_TELEMETRY_UNAVAILABLE");
    puts("PASS: native shutdown advisory binds approved policy only, uses trusted telemetry, withdraws offline and never controls power");
    return 0;
}
