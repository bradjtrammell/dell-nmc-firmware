#include "ou_notify.h"

void ou_notify_init(ou_notify_engine *engine){
    if(engine){
        *engine=(ou_notify_engine){0};
        engine->initialized=1U;
    }
}
static int allowed(const ou_notify_rule *rule){
    if(!rule||rule->enabled>1U||rule->destination>=8U||
       rule->transport<OU_NOTIFY_EMAIL||
       rule->transport>OU_NOTIFY_SNMP_TRAP||
       rule->cooldown_ms>UINT32_C(3600000))return 0;
    if(rule->enabled &&
       (rule->event_mask==0U || (rule->event_mask&~UINT16_C(255))!=0U))
        return 0;
    return (rule->event_mask&~UINT16_C(255))==0U;
}
size_t ou_notify_pending(const ou_notify_engine *engine){
    size_t n=0U,i;
    if(!engine||engine->initialized!=1U)return 0U;
    for(i=0U;i<OU_NOTIFY_QUEUE;++i)n+=(engine->used[i]==1U);
    return n;
}
int ou_notify_set_rules(ou_notify_engine *engine,
                        const ou_notify_rule *rules,size_t count,
                        int authorized){
    size_t i;
    if(!engine||engine->initialized!=1U||authorized!=1||
       count>OU_NOTIFY_RULES||(count&&!rules))return -1;
    for(i=0U;i<count;++i)if(!allowed(&rules[i]))return -1;
    /* Changing recipients/rules withdraws queued notifications from old
     * destinations, rather than sending them under replacement policy. */
    engine->dropped+=(uint32_t)ou_notify_pending(engine);
    for(i=0U;i<OU_NOTIFY_QUEUE;++i){
        engine->used[i]=0U;
        engine->queue[i]=(ou_notify_job){0};
    }
    for(i=0U;i<OU_NOTIFY_RULES;++i){
        engine->rules[i]=i<count?rules[i]:(ou_notify_rule){0};
        engine->triggered[i]=0U;
        engine->last_trigger_ms[i]=0U;
        engine->last_event_id[i]=0U;
        engine->last_boot_generation[i]=0U;
    }
    return 0;
}
int ou_notify_route(ou_notify_engine *engine,
                    const ou_event_record *event,uint32_t now_ms){
    size_t i,j;
    int queued=0;
    if(!engine||engine->initialized!=1U||!event||
       event->id==0U||event->boot_generation==0U||
       event->code<OU_EVENT_FIRST||event->code>OU_EVENT_LAST)
        return -1;
    for(i=0U;i<OU_NOTIFY_RULES;++i){
        const ou_notify_rule *r=&engine->rules[i];
        uint32_t elapsed;
        size_t index=OU_NOTIFY_QUEUE;
        if(r->enabled!=1U ||
           (r->event_mask&(UINT16_C(1)<<(event->code-1U)))==0U)
            continue;
        if(engine->last_event_id[i]==event->id &&
           engine->last_boot_generation[i]==event->boot_generation)continue;
        if(engine->triggered[i]){
            elapsed=now_ms-engine->last_trigger_ms[i];
            if(elapsed>=UINT32_C(0x80000000) ||
               elapsed<r->cooldown_ms)continue;
        }
        for(j=0U;j<OU_NOTIFY_QUEUE;++j){
            if(engine->used[j]==0U){index=j;break;}
        }
        if(index==OU_NOTIFY_QUEUE) {
            if(engine->dropped<UINT32_MAX)++engine->dropped;
            continue;
        }
        engine->queue[index]=(ou_notify_job){0};
        engine->queue[index].event_id=event->id;
        engine->queue[index].boot_generation=event->boot_generation;
        engine->queue[index].created_ms=now_ms;
        engine->queue[index].due_ms=now_ms;
        engine->queue[index].code=event->code;
        engine->queue[index].destination=r->destination;
        engine->queue[index].transport=r->transport;
        engine->used[index]=1U;
        engine->triggered[i]=1U;
        engine->last_trigger_ms[i]=now_ms;
        engine->last_event_id[i]=event->id;
        engine->last_boot_generation[i]=event->boot_generation;
        if(engine->enqueued<UINT32_MAX)++engine->enqueued;
        ++queued;
    }
    return queued;
}
static void discard(ou_notify_engine *engine,size_t index){
    engine->used[index]=0U;
    engine->queue[index]=(ou_notify_job){0};
}
int ou_notify_step(ou_notify_engine *engine,const ou_notify_sender *sender,
                   uint32_t now_ms){
    size_t i;
    if(!engine||engine->initialized!=1U||!sender||
       !sender->send||sender->transport_qualified!=1U||
       sender->destination_store_qualified!=1U||
       sender->transport_security_verified!=1U)return 0;
    for(i=0U;i<OU_NOTIFY_QUEUE;++i){
        ou_notify_job *job=&engine->queue[i];
        uint32_t elapsed;
        int result;
        if(engine->used[i]!=1U)continue;
        elapsed=now_ms-job->due_ms;
        if(elapsed>=UINT32_C(0x80000000))continue;
        result=sender->send(sender->context,job);
        if(result==1){
            if(engine->delivered<UINT32_MAX)++engine->delivered;
            discard(engine,i);
            return 1;
        }
        if(result<0 || job->attempt+1U>=OU_NOTIFY_MAX_ATTEMPTS){
            if(engine->failed<UINT32_MAX)++engine->failed;
            discard(engine,i);
            return -1;
        }
        ++job->attempt;
        if(engine->retried<UINT32_MAX)++engine->retried;
        {
            uint32_t delay=OU_NOTIFY_BACKOFF_BASE_MS<<(job->attempt-1U);
            if(delay>OU_NOTIFY_BACKOFF_MAX_MS)delay=OU_NOTIFY_BACKOFF_MAX_MS;
            job->due_ms=now_ms+delay;
        }
        return 0;
    }
    return 0;
}
