#include "ou_agents.h"

static int elapsed(uint32_t now,uint32_t then,uint32_t *age) {
    *age=now-then;
    return *age<UINT32_C(0x80000000);
}
static int online(const ou_agent *agent,uint32_t now) {
    uint32_t age;
    return agent->occupied==1U && agent->heartbeat_seen==1U &&
           elapsed(now,agent->last_seen_ms,&age) && age<=agent->lease_ms;
}
static int valid_name(const char *name) {
    unsigned i;int text=0;
    if(!name)return 0;
    for(i=0U;i<OU_AGENT_NAME_BYTES;++i){
        unsigned char ch=(unsigned char)name[i];
        if(ch==0U)return text;
        if(ch<0x20U||ch>0x7eU||ch=='<'||ch=='>'||
           ch=='&'||ch=='"'||ch=='\\')return 0;
        if(ch!=' ')text=1;
    }
    return 0;
}
static ou_agent *find(ou_agent_registry *r,uint32_t id) {
    unsigned i;
    for(i=0U;i<OU_AGENT_MAX;++i)
        if(r->agents[i].occupied==1U&&r->agents[i].id==id)
            return &r->agents[i];
    return 0;
}
void ou_agents_init(ou_agent_registry *r) {
    if(!r)return;
    *r=(ou_agent_registry){0};
    r->initialized=1U;
}
int ou_agents_enroll(ou_agent_registry *r,uint32_t id,
                     const char *label,uint32_t lease_ms,int admin_authorized){
    unsigned i,j;
    ou_agent *a=0;
    if(!r||r->initialized!=1U||admin_authorized!=1||
       r->warning_active||id==0U||lease_ms<OU_AGENT_MIN_LEASE_MS||
       lease_ms>OU_AGENT_MAX_LEASE_MS||!valid_name(label)||
       find(r,id))return -1;
    for(i=0U;i<OU_AGENT_MAX;++i)
        if(!r->agents[i].occupied){a=&r->agents[i];break;}
    if(!a)return -1;
    *a=(ou_agent){0};
    a->id=id;
    a->lease_ms=lease_ms;
    a->occupied=1U;
    for(j=0U;label[j]!=0U&&j<OU_AGENT_NAME_BYTES-1U;++j)
        a->label[j]=label[j];
    a->label[j]=0;
    return 0;
}
int ou_agents_remove(ou_agent_registry *r,uint32_t id,int admin_authorized){
    ou_agent *a;
    if(!r||r->initialized!=1U||r->warning_active||
       admin_authorized!=1)return -1;
    a=find(r,id);
    if(!a)return -1;
    *a=(ou_agent){0};
    return 0;
}
int ou_agents_heartbeat(ou_agent_registry *r,uint32_t id,
                        uint32_t sequence,uint32_t now_ms,
                        int peer_identity_verified) {
    ou_agent *a;
    if(!r||r->initialized!=1U||peer_identity_verified!=1||
       sequence==0U)return -1;
    a=find(r,id);
    if(!a || (a->heartbeat_seen && sequence<=a->heartbeat_sequence))return -1;
    a->heartbeat_sequence=sequence;
    a->last_seen_ms=now_ms;
    a->heartbeat_seen=1U;
    return 0;
}
int ou_agents_begin_warning(ou_agent_registry *r,uint32_t now_ms,
                      uint32_t warning_ms,int operator_authorized,
                      int verified_shutdown_intent){
    unsigned i,count=0U;
    if(!r||r->initialized!=1U||r->warning_active||
       r->transaction==UINT32_MAX||
       operator_authorized!=1||verified_shutdown_intent!=1||
       warning_ms<OU_AGENT_MIN_WARNING_MS||
       warning_ms>OU_AGENT_MAX_WARNING_MS)return -1;
    for(i=0U;i<OU_AGENT_MAX;++i){
        if(!r->agents[i].occupied)continue;
        if(!online(&r->agents[i],now_ms))return -1;
        ++count;
    }
    if(count==0U)return -1;
    ++r->transaction;
    r->warning_start_ms=now_ms;
    r->warning_deadline_ms=now_ms+warning_ms;
    r->warning_active=1U;
    r->acknowledgements=0U;
    r->missed=0U;
    for(i=0U;i<OU_AGENT_MAX;++i)
        if(r->agents[i].occupied){
            r->agents[i].phase=OU_AGENT_WAITING_NOTICE;
            r->agents[i].ack_transaction=0U;
        }
    return 0;
}
int ou_agents_deliver_one(ou_agent_registry *r,
                      const ou_agent_sender *sender,uint32_t now_ms){
    unsigned i;int result;
    uint32_t age;
    if(!r||r->initialized!=1U||!r->warning_active||
       !sender||!sender->send_notice||
       sender->tls_peer_verified!=1U||
       sender->destination_identity_qualified!=1U||
       sender->network_qualified!=1U)return -1;
    if(!elapsed(now_ms,r->warning_start_ms,&age) ||
       age>=r->warning_deadline_ms-r->warning_start_ms)return -1;
    for(i=0U;i<OU_AGENT_MAX;++i){
        ou_agent *a=&r->agents[i];
        ou_agent_notice notice;
        if(!a->occupied||a->phase!=OU_AGENT_WAITING_NOTICE)continue;
        if(!online(a,now_ms)){
            a->phase=OU_AGENT_UNREACHABLE;
            ++r->missed;
            return -1;
        }
        notice.agent_id=a->id;
        notice.transaction=r->transaction;
        notice.warning_deadline_ms=r->warning_deadline_ms;
        result=sender->send_notice(sender->context,&notice);
        if(result==1){
            a->phase=OU_AGENT_NOTICE_SENT;
            return 1;
        }
        if(result<0){
            a->phase=OU_AGENT_UNREACHABLE;
            ++r->missed;
            return -1;
        }
        return 0;
    }
    return 0;
}
int ou_agents_ack(ou_agent_registry *r,uint32_t id,
                  uint32_t transaction,uint32_t now_ms,
                  int peer_identity_verified){
    ou_agent *a;uint32_t age;
    if(!r||r->initialized!=1U||peer_identity_verified!=1||
       !r->warning_active||transaction==0U||
       transaction!=r->transaction ||
       !elapsed(now_ms,r->warning_start_ms,&age)||
       age>=r->warning_deadline_ms-r->warning_start_ms)return -1;
    a=find(r,id);
    if(!a||a->phase!=OU_AGENT_NOTICE_SENT || !online(a,now_ms))return -1;
    a->phase=OU_AGENT_ACKNOWLEDGED;
    a->ack_transaction=transaction;
    ++r->acknowledgements;
    return 0;
}
int ou_agents_step(ou_agent_registry *r,uint32_t now_ms){
    unsigned i,count=0U,done=0U;
    uint32_t age;
    if(!r||r->initialized!=1U||!r->warning_active)return -1;
    if(!elapsed(now_ms,r->warning_start_ms,&age)||
       age>=r->warning_deadline_ms-r->warning_start_ms){
        for(i=0U;i<OU_AGENT_MAX;++i)
            if(r->agents[i].occupied &&
               r->agents[i].phase!=OU_AGENT_ACKNOWLEDGED &&
               r->agents[i].phase!=OU_AGENT_UNREACHABLE){
                r->agents[i].phase=OU_AGENT_UNREACHABLE;
                ++r->missed;
            }
        r->warning_active=0U;
        return -1;
    }
    for(i=0U;i<OU_AGENT_MAX;++i){
        ou_agent *a=&r->agents[i];
        if(!a->occupied)continue;
        ++count;
        if(a->phase==OU_AGENT_ACKNOWLEDGED){++done;continue;}
        if(a->phase==OU_AGENT_UNREACHABLE||!online(a,now_ms)){
            if(a->phase!=OU_AGENT_UNREACHABLE){
                a->phase=OU_AGENT_UNREACHABLE;
                ++r->missed;
            }
            r->warning_active=0U;
            return -1;
        }
    }
    if(count>0U&&done==count){
        r->warning_active=0U;
        return 1;
    }
    return 0;
}
int ou_agents_status(const ou_agent_registry *r,uint32_t now_ms,
                     ou_agent_status *status) {
    unsigned i;
    if(!r||r->initialized!=1U||!status)return -1;
    *status=(ou_agent_status){0};
    status->warning_active=r->warning_active;
    for(i=0U;i<OU_AGENT_MAX;++i){
        const ou_agent *a=&r->agents[i];
        if(!a->occupied)continue;
        ++status->enrolled;
        if(online(a,now_ms))++status->online;
        if(a->phase==OU_AGENT_WAITING_NOTICE ||
           a->phase==OU_AGENT_NOTICE_SENT)++status->waiting;
        if(a->phase==OU_AGENT_ACKNOWLEDGED)++status->acknowledged;
        if(a->phase==OU_AGENT_UNREACHABLE)++status->unreachable;
    }
    return 0;
}
