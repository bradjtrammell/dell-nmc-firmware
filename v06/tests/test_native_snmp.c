#include "ou_native_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t request[]={
  0x30,0x31,0x02,0x01,0x01,0x04,0x0f,
  'p','r','i','v','a','t','e','-','m','o','n','i','t','o','r',
  0xa0,0x1b,0x02,0x01,0x37,0x02,0x01,0x00,0x02,0x01,0x00,
  0x30,0x10,0x30,0x0e,0x06,0x0a,0x2b,0x06,0x01,0x02,
  0x01,0x21,0x01,0x02,0x01,0x00,0x05,0x00
};
static int auth(void *context,const uint8_t *s,size_t n){
    (void)context;
    return n==15U&&memcmp(s,"private-monitor",15U)==0;
}
static ou_snmp_access access_policy(void){
    ou_snmp_access a={0};
    a.authorize=auth;
    a.source_acl_qualified=1U;
    a.community_store_qualified=1U;
    a.isolated_network_qualified=1U;
    return a;
}
static ou_driver_report report(void){
    ou_driver_report r={0};
    r.version=OU_REPORT_VERSION;
    r.verified=1U;
    r.received_monotonic_ms=1000U;
    r.sequence=1U;r.uptime_seconds=1U;
    r.runtime_seconds=1200U;r.battery_percent=70U;
    r.load_percent=25U;r.input_decivolts=1200U;
    r.output_decivolts=1200U;r.utility_present=1U;r.battery_present=1U;
    return r;
}
static int has(const uint8_t *buffer,size_t length,const uint8_t *pattern,size_t count){
    size_t i;
    for(i=0U;i+count<=length;++i)
        if(memcmp(buffer+i,pattern,count)==0)return 1;
    return 0;
}
int main(void){
    ou_snmp_access a=access_policy();
    uint8_t response[OU_SNMP_MAX_PACKET],invalid[sizeof(request)];
    size_t written=123U;
    ou_driver_report r;
    static const uint8_t no_instance[]={0x81U,0x00U};
    static const uint8_t battery_normal[]={0x02U,0x01U,0x02U};
    ou_native_service_init();
    /* Device source is unavailable before its first validated driver frame.
     * SNMP must return noSuchInstance rather than simulated UPS metrics. */
    assert(ou_native_service_snmp(&a,request,sizeof(request),1000U,
                                  response,sizeof(response),&written)==0);
    assert(has(response,written,no_instance,sizeof(no_instance)));
    a.source_acl_qualified=0U;
    assert(ou_native_service_snmp(&a,request,sizeof(request),1000U,
                                  response,sizeof(response),&written)==1);
    assert(written==0U);
    a.source_acl_qualified=1U;
    r=report();
    assert(ou_native_service_driver_report(&r,1000U)==0);
    assert(ou_native_service_snmp(&a,request,sizeof(request),1001U,
                                  response,sizeof(response),&written)==0);
    assert(has(response,written,battery_normal,sizeof(battery_normal)));
    assert(!has(response,written,no_instance,sizeof(no_instance)));
    ou_native_service_source_lost_at(2000U);
    assert(ou_native_service_snmp(&a,request,sizeof(request),2001U,
                                  response,sizeof(response),&written)==0);
    assert(has(response,written,no_instance,sizeof(no_instance)));
    memcpy(invalid,request,sizeof(request));
    invalid[0]=0x31U;
    assert(ou_native_service_snmp(&a,invalid,sizeof(invalid),2000U,
                                  response,sizeof(response),&written)==-1);
    assert(written==0U);
    puts("PASS: native SNMP telemetry isolation, live verified GET, offline withdrawal, strict ACL and malformed rejection");
    return 0;
}
