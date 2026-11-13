#include "ou_native_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t request[]="GET /api/v1/ups/status HTTP/1.0\r\n\r\n";
static void expect(uint32_t now,int status,const char *token) {
    char response[OU_HTTP_MAX_RESPONSE+1U],prefix[24];
    size_t length=0;
    assert(ou_native_service_http(request,sizeof(request)-1,now,response,
                                  sizeof(response)-1,&length)==0);
    assert(length<sizeof(response));response[length]='\0';
    snprintf(prefix,sizeof(prefix),"HTTP/1.1 %d",status);
    assert(strstr(response,prefix)==response);
    assert(strstr(response,token)!=NULL);
}
int main(void) {
    ou_driver_report frame={0};
    ou_native_service_init();
    expect(100U,503,"TELEMETRY_UNAVAILABLE");
    frame.version=OU_REPORT_VERSION;
    frame.verified=1U;
    frame.sequence=1U;
    frame.received_monotonic_ms=200U;
    frame.uptime_seconds=20U;
    frame.battery_percent=89U;
    frame.runtime_seconds=1100U;
    frame.load_percent=43U;
    frame.input_decivolts=1190U;
    frame.output_decivolts=1200U;
    frame.utility_present=1U;
    frame.battery_present=1U;
    assert(ou_native_service_driver_report(&frame,200U)==0);
    expect(201U,200,"\"batteryPercent\":89");
    assert(ou_native_service_driver_report(&frame,202U)==-1);
    expect(202U,503,"TELEMETRY_UNAVAILABLE");
    frame.sequence=2U;frame.received_monotonic_ms=203U;
    assert(ou_native_service_driver_report(&frame,203U)==0);
    expect(5203U,200,"\"batteryPercent\":89");
    /* A serial fault must hide an otherwise fresh sample immediately, not
     * leave a stale 200 response until the 5-second expiry. */
    ou_native_service_source_lost();
    expect(5203U,503,"TELEMETRY_UNAVAILABLE");
    assert(ou_native_service_driver_report(&frame,5203U)==-1);
    expect(5203U,503,"TELEMETRY_UNAVAILABLE");
    frame.sequence=3U;frame.received_monotonic_ms=5203U;
    assert(ou_native_service_driver_report(&frame,5203U)==0);
    expect(5203U,200,"\"batteryPercent\":89");
    expect(5204U,503,"TELEMETRY_UNAVAILABLE");
    frame.sequence=4U;frame.received_monotonic_ms=5300U;frame.verified=0U;
    assert(ou_native_service_driver_report(&frame,5300U)==-1);
    expect(5300U,503,"TELEMETRY_UNAVAILABLE");
    ou_native_service_init();
    expect(5300U,503,"TELEMETRY_UNAVAILABLE");
    puts("PASS: native ARM service telemetry/HTTP lifecycle, anti-replay and expiry");
    return 0;
}
