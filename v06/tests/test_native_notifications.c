#include "ou_native_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int authorize(void *context){return *(int *)context;}
static int sent=0;
static ou_notify_job latest;
static int send(void *context,const ou_notify_job *job){
    (void)context;
    latest=*job;
    ++sent;
    return 1;
}
static void request(const char *verb,const char *path,
                    uint32_t now,int code,const char *needle){
    uint8_t req[180U];
    char response[OU_HTTP_MAX_RESPONSE+1U];
    char start[24U];
    size_t length=0U;
    int n=snprintf((char *)req,sizeof(req),"%s %s HTTP/1.0\r\n\r\n",
                   verb,path);
    assert(n>0&&(size_t)n<sizeof(req));
    assert(ou_native_service_http(req,(size_t)n,now,response,
                                  sizeof(response)-1U,&length)==0);
    response[length]='\0';
    (void)snprintf(start,sizeof(start),"HTTP/1.1 %d",code);
    assert(strstr(response,start)==response);
    if(needle)assert(strstr(response,needle)!=0);
}
static ou_driver_report report(uint32_t seq,uint32_t ms,int utility){
    ou_driver_report r={0};
    r.version=OU_REPORT_VERSION;r.verified=1U;r.sequence=seq;
    r.received_monotonic_ms=ms;r.uptime_seconds=ms/1000U;
    r.battery_percent=70U;r.load_percent=20U;
    r.runtime_seconds=1200U;
    r.input_decivolts=1200U;r.output_decivolts=1200U;
    r.battery_present=1U;r.utility_present=(uint8_t)utility;
    return r;
}
int main(void){
    ou_notify_rule rules[2]={{0}};
    ou_notify_sender sink={0};
    ou_driver_report r;
    int authenticated=0;
    ou_native_service_init();
    {
        ou_journal_record record={0};
        /* Service links the policy journal, but unqualified flash must fail. */
        assert(ou_native_service_notifications_checkpoint(NULL,NULL,NULL,
                                                           &record)==-1);
        assert(ou_native_service_notifications_restore(NULL,&record)==-1);
    }

    request("GET","/api/v1/notifications/status",1U,200,"\"activeRules\":0");
    request("GET","/api/v1/notifications/policy",1U,200,"\"maximumRules\":8");
    assert(ou_native_service_notify_configure(rules,2U,authorize,&authenticated)==-1);
    assert(ou_native_service_notify_configure(rules,2U,0,0)==-1);
    rules[0].enabled=1U;
    rules[0].transport=OU_NOTIFY_EMAIL;
    rules[0].destination=0U;
    rules[0].event_mask=(uint16_t)(1U<<(OU_EVENT_UTILITY_LOST-1U));
    rules[1].enabled=1U;
    rules[1].transport=OU_NOTIFY_WEBHOOK;
    rules[1].destination=1U;
    rules[1].event_mask=(uint16_t)(1U<<(OU_EVENT_BATTERY_LOW-1U));
    authenticated=1;
    assert(ou_native_service_notify_configure(rules,2U,authorize,&authenticated)==0);
    r=report(1U,1000U,1);
    assert(ou_native_service_driver_report(&r,1000U)==0);
    request("GET","/api/v1/notifications/status",1000U,200,"\"pending\":0");
    r=report(2U,2000U,0);
    assert(ou_native_service_driver_report(&r,2000U)==0);
    request("GET","/api/v1/notifications/status",2000U,200,"\"pending\":1");
    request("GET","/api/v1/notifications/status",2000U,200,"\"enqueued\":1");
    sink.send=send;
    sink.transport_qualified=1U;
    sink.destination_store_qualified=1U;
    sink.transport_security_verified=0U;
    assert(ou_native_service_notify_step(&sink,2000U)==0);
    assert(sent==0);
    sink.transport_security_verified=1U;
    assert(ou_native_service_notify_step(&sink,2000U)==1);
    assert(sent==1&&latest.code==OU_EVENT_UTILITY_LOST);
    assert(latest.transport==OU_NOTIFY_EMAIL);
    request("GET","/api/v1/notifications/status",2000U,200,"\"delivered\":1");
    request("GET","/api/v1/notifications/status",2000U,200,"\"pending\":0");
    request("POST","/api/v1/notifications/policy",2000U,405,"READ_ONLY");
    request("POST","/api/v1/notifications/status",2000U,405,"READ_ONLY");
    /* Invalid frame records a rejection event but cannot queue unrelated
       power changes or leak any raw report fields into queued notifications. */
    r=report(3U,3000U,0);r.verified=0U;
    assert(ou_native_service_driver_report(&r,3000U)==-1);
    request("GET","/api/v1/notifications/status",3000U,200,"\"pending\":0");
    ou_native_service_init();
    request("GET","/api/v1/notifications/status",1U,200,"\"pending\":0");
    request("GET","/api/v1/notifications/status",1U,200,"\"delivered\":0");
    puts("PASS: native event to notification queue to qualified sender, live status API, denial and reboot");
    return 0;
}
