#include "ou_notify.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct { unsigned calls; int mode; ou_notify_job latest; } transport;
static int send_job(void *ctx,const ou_notify_job *job){
    (void)ctx;
    ++transport.calls;
    transport.latest=*job;
    return transport.mode;
}
static ou_notify_sender sender(void){
    ou_notify_sender s={0};
    s.send=send_job;
    s.transport_qualified=1U;
    s.destination_store_qualified=1U;
    s.transport_security_verified=1U;
    return s;
}
static ou_event_record event(uint32_t id,uint8_t code) {
    ou_event_record r={0};
    r.id=id;r.boot_generation=1U;r.code=code;r.monotonic_ms=id*1000U;
    return r;
}
static void route_and_retry(void){
    ou_notify_engine n;
    ou_notify_rule r[3]={{0}};
    ou_notify_sender s=sender();
    ou_event_record e;
    ou_notify_init(&n);
    r[0].enabled=1U;r[0].event_mask=1U<<(OU_EVENT_UTILITY_LOST-1U);
    r[0].transport=OU_NOTIFY_EMAIL;r[0].destination=2U;r[0].cooldown_ms=30000U;
    r[1]=r[0];r[1].transport=OU_NOTIFY_WEBHOOK;r[1].destination=0U;
    r[2]=r[0];r[2].event_mask=1U<<(OU_EVENT_BATTERY_LOW-1U);
    r[2].transport=OU_NOTIFY_SNMP_TRAP;r[2].destination=1U;
    assert(ou_notify_set_rules(&n,r,3U,0)==-1);
    assert(ou_notify_set_rules(&n,r,3U,1)==0);
    e=event(1U,OU_EVENT_UTILITY_LOST);
    assert(ou_notify_route(&n,&e,1000U)==2);
    assert(ou_notify_route(&n,&e,1001U)==0);
    assert(ou_notify_pending(&n)==2U);
    transport.mode=0U;
    assert(ou_notify_step(&n,&s,1000U)==0);
    assert(transport.calls==1U&&n.retried==1U);
    assert(transport.latest.transport==OU_NOTIFY_EMAIL);
    assert(transport.latest.destination==2U);
    assert(ou_notify_step(&n,&s,1001U)==0);
    assert(transport.calls==2U);
    assert(transport.latest.transport==OU_NOTIFY_WEBHOOK);
    assert(ou_notify_step(&n,&s,2999U)==0);
    assert(transport.calls==2U);
    transport.mode=1;
    assert(ou_notify_step(&n,&s,3000U)==1);
    assert(ou_notify_step(&n,&s,3000U)==1);
    assert(n.delivered==2U&&ou_notify_pending(&n)==0U);
    assert(ou_notify_route(&n,&e,40000U)==0);
    e=event(2U,OU_EVENT_UTILITY_LOST);
    assert(ou_notify_route(&n,&e,40001U)==2);
    e=event(3U,OU_EVENT_BATTERY_LOW);
    assert(ou_notify_route(&n,&e,40002U)==1);
    assert(ou_notify_pending(&n)==3U);
    s.transport_security_verified=0U;
    assert(ou_notify_step(&n,&s,40002U)==0);
    assert(ou_notify_pending(&n)==3U);
    s.transport_security_verified=1U;
    transport.mode=-1;
    assert(ou_notify_step(&n,&s,40002U)==-1);
    assert(n.failed==1U&&ou_notify_pending(&n)==2U);
    assert(ou_notify_set_rules(&n,r,3U,1)==0);
    assert(ou_notify_pending(&n)==0U&&n.dropped==2U);
}
static void cooldown_and_overflow(void){
    ou_notify_engine n;
    ou_notify_rule r[OU_NOTIFY_RULES]={{0}};
    ou_event_record e;
    size_t i;
    ou_notify_init(&n);
    r[0].enabled=1U;r[0].event_mask=1U<<(OU_EVENT_UTILITY_LOST-1U);
    r[0].transport=OU_NOTIFY_WEBHOOK;r[0].destination=0U;
    r[0].cooldown_ms=10000U;
    assert(ou_notify_set_rules(&n,r,1U,1)==0);
    e=event(1U,OU_EVENT_UTILITY_LOST);
    assert(ou_notify_route(&n,&e,UINT32_MAX-1000U)==1);
    e=event(2U,OU_EVENT_UTILITY_LOST);
    assert(ou_notify_route(&n,&e,2000U)==0);
    assert(ou_notify_route(&n,&e,10000U)==1);
    assert(ou_notify_pending(&n)==2U);
    for(i=0U;i<OU_NOTIFY_RULES;++i){
        r[i]=r[0];r[i].cooldown_ms=0U;
        r[i].destination=(uint8_t)i;
    }
    assert(ou_notify_set_rules(&n,r,OU_NOTIFY_RULES,1)==0);
    for(i=1U;i<=4U;++i) {
        e=event((uint32_t)i,OU_EVENT_UTILITY_LOST);
        assert(ou_notify_route(&n,&e,10000U+(uint32_t)i)==(int)(i<4U?8U:0U));
    }
    assert(ou_notify_pending(&n)==OU_NOTIFY_QUEUE);
    assert(n.dropped>=8U);
    for(i=0U;i<OU_NOTIFY_RULES;++i)r[i].transport=0U;
    assert(ou_notify_set_rules(&n,r,OU_NOTIFY_RULES,1)==-1);
    assert(ou_notify_pending(&n)==OU_NOTIFY_QUEUE);
}
static void retry_exhaustion_and_inputs(void){
    ou_notify_engine n;
    ou_notify_rule r={0};
    ou_notify_sender s=sender();
    ou_event_record e=event(1U,OU_EVENT_TELEMETRY_OFFLINE);
    unsigned i;
    ou_notify_init(&n);
    r.enabled=1;r.transport=OU_NOTIFY_EMAIL;
    r.event_mask=1U<<(OU_EVENT_TELEMETRY_OFFLINE-1U);
    assert(ou_notify_set_rules(&n,&r,1U,1)==0);
    assert(ou_notify_route(&n,&e,1000U)==1);
    transport.mode=0;
    for(i=0U;i<OU_NOTIFY_MAX_ATTEMPTS;++i){
        uint32_t now=1000U+(i==0U?0U:(2000U*((1U<<i)-1U)));
        assert(ou_notify_step(&n,&s,now)==(i+1U==OU_NOTIFY_MAX_ATTEMPTS?-1:0));
    }
    assert(n.failed==1U&&n.retried==4U);
    assert(ou_notify_pending(&n)==0U);
    e.code=99U;
    assert(ou_notify_route(&n,&e,1000U)==-1);
    e=event(1U,OU_EVENT_TELEMETRY_OFFLINE);
    e.id=0U;
    assert(ou_notify_route(&n,&e,1000U)==-1);
}
int main(void){
    route_and_retry();
    cooldown_and_overflow();
    retry_exhaustion_and_inputs();
    puts("PASS: native event notification routing, multi-channel fanout, cooldown, dedupe, retries, qualified transports and overflow");
    return 0;
}
