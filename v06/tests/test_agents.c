#include "ou_agents.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct {uint32_t notices;int result;ou_agent_notice last;} network;
static int notify(void *context,const ou_agent_notice *notice){
    (void)context;
    ++network.notices;
    network.last=*notice;
    return network.result;
}
static ou_agent_sender transport(void){
    ou_agent_sender a={0};
    a.send_notice=notify;
    a.tls_peer_verified=1U;
    a.destination_identity_qualified=1U;
    a.network_qualified=1U;
    return a;
}
static void enrollment_and_access(void){
    ou_agent_registry r;
    ou_agent_status s;
    unsigned i;
    ou_agents_init(&r);
    assert(ou_agents_enroll(&r,11U,"Server 1",30000U,0)==-1);
    assert(ou_agents_enroll(&r,11U,"Server 1",30000U,1)==0);
    assert(ou_agents_enroll(&r,11U,"Another",30000U,1)==-1);
    assert(ou_agents_enroll(&r,0U,"Invalid",30000U,1)==-1);
    assert(ou_agents_enroll(&r,12U,"Admin<script>",30000U,1)==-1);
    assert(ou_agents_enroll(&r,12U,"Server 2",3000U,1)==-1);
    assert(ou_agents_enroll(&r,12U,"Server 2",500000U,1)==-1);
    assert(ou_agents_enroll(&r,12U,"Server 2",30000U,1)==0);
    assert(ou_agents_heartbeat(&r,11U,1U,1000U,0)==-1);
    assert(ou_agents_heartbeat(&r,11U,1U,1000U,1)==0);
    assert(ou_agents_heartbeat(&r,11U,1U,1001U,1)==-1);
    assert(ou_agents_heartbeat(&r,11U,0U,1001U,1)==-1);
    assert(ou_agents_heartbeat(&r,12U,1U,1000U,1)==0);
    assert(ou_agents_status(&r,2000U,&s)==0);
    assert(s.enrolled==2U&&s.online==2U);
    assert(ou_agents_status(&r,33000U,&s)==0);
    assert(s.enrolled==2U&&s.online==0U);
    assert(ou_agents_remove(&r,12U,0)==-1);
    assert(ou_agents_remove(&r,12U,1)==0);
    assert(ou_agents_status(&r,2000U,&s)==0);
    assert(s.enrolled==1U);
    for(i=0U;i<OU_AGENT_MAX-1U;++i){
        char name[32]="Worker";
        name[6]=(char)('A'+(int)i);
        name[7]=0;
        assert(ou_agents_enroll(&r,100U+i,name,60000U,1)==0);
    }
    assert(ou_agents_enroll(&r,222U,"Overflow",60000U,1)==-1);
}
static void warning_and_acks(void){
    ou_agent_registry r;
    ou_agent_sender sender=transport();
    ou_agent_status status;
    ou_agents_init(&r);
    memset(&network,0,sizeof(network));
    network.result=1;
    assert(ou_agents_enroll(&r,11U,"Windows file server",60000U,1)==0);
    assert(ou_agents_enroll(&r,22U,"Linux host",60000U,1)==0);
    assert(ou_agents_heartbeat(&r,11U,1U,1000U,1)==0);
    assert(ou_agents_heartbeat(&r,22U,1U,1000U,1)==0);
    assert(ou_agents_begin_warning(&r,2000U,50000U,0,1)==-1);
    assert(ou_agents_begin_warning(&r,2000U,50000U,1,0)==-1);
    assert(ou_agents_begin_warning(&r,2000U,100U,1,1)==-1);
    assert(ou_agents_begin_warning(&r,2000U,50000U,1,1)==0);
    assert(r.transaction==1U);
    assert(ou_agents_begin_warning(&r,2000U,50000U,1,1)==-1);
    assert(ou_agents_remove(&r,11U,1)==-1);
    assert(ou_agents_deliver_one(&r,0,3000U)==-1);
    assert(network.notices==0U);
    sender.network_qualified=0U;
    assert(ou_agents_deliver_one(&r,&sender,3000U)==-1);
    assert(network.notices==0U);
    sender.network_qualified=1U;
    assert(ou_agents_deliver_one(&r,&sender,3000U)==1);
    assert(network.notices==1U && network.last.agent_id==11U);
    assert(network.last.transaction==1U && network.last.warning_deadline_ms==52000U);
    assert(ou_agents_ack(&r,11U,0U,3100U,1)==-1);
    assert(ou_agents_ack(&r,11U,1U,3100U,0)==-1);
    assert(ou_agents_ack(&r,11U,1U,3100U,1)==0);
    assert(ou_agents_ack(&r,11U,1U,3101U,1)==-1);
    assert(ou_agents_step(&r,3200U)==0);
    assert(ou_agents_deliver_one(&r,&sender,3300U)==1);
    assert(network.last.agent_id==22U);
    assert(ou_agents_ack(&r,22U,2U,3400U,1)==-1);
    assert(ou_agents_ack(&r,22U,1U,3400U,1)==0);
    assert(ou_agents_status(&r,3500U,&status)==0);
    assert(status.enrolled==2U&&status.online==2U);
    assert(status.acknowledged==2U&&status.waiting==0U);
    assert(ou_agents_step(&r,3600U)==1);
    assert(r.warning_active==0U);
    assert(ou_agents_step(&r,3601U)==-1);
    assert(ou_agents_begin_warning(&r,4000U,30000U,1,1)==0);
    assert(r.transaction==2U);
}
static void deadlines_and_fail_closed(void){
    ou_agent_registry r;
    ou_agent_sender sender=transport();
    ou_agent_status status;
    ou_agents_init(&r);
    memset(&network,0,sizeof(network));
    network.result=0;
    assert(ou_agents_enroll(&r,7U,"Archive server",30000U,1)==0);
    assert(ou_agents_begin_warning(&r,0U,30000U,1,1)==-1);
    assert(ou_agents_heartbeat(&r,7U,1U,UINT32_MAX-2000U,1)==0);
    assert(ou_agents_begin_warning(&r,UINT32_MAX-1000U,30000U,1,1)==0);
    assert(ou_agents_deliver_one(&r,&sender,1000U)==0);
    assert(ou_agents_status(&r,1000U,&status)==0);
    assert(status.waiting==1U&&status.acknowledged==0U);
    assert(ou_agents_step(&r,1000U)==0);
    assert(ou_agents_step(&r,29000U)==-1);
    assert(ou_agents_status(&r,29000U,&status)==0);
    assert(status.warning_active==0U&&status.unreachable==1U);
    assert(r.missed==1U);
    assert(ou_agents_ack(&r,7U,1U,30000U,1)==-1);
    assert(ou_agents_heartbeat(&r,7U,2U,30000U,1)==0);
    assert(ou_agents_begin_warning(&r,30001U,40000U,1,1)==0);
    network.result=-1;
    assert(ou_agents_deliver_one(&r,&sender,30100U)==-1);
    assert(ou_agents_step(&r,30110U)==-1);
}
int main(void){
    enrollment_and_access();
    warning_and_acks();
    deadlines_and_fail_closed();
    puts("PASS: 12-client shutdown-agent registry, TTL heartbeats, verified warnings/ACKs, replay safety, timeouts and denial");
    return 0;
}
