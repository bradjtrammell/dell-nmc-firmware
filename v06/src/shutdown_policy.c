#include "ou_shutdown_policy.h"

static void put16(uint8_t *p,uint16_t x){
    p[0]=(uint8_t)(x>>8U);p[1]=(uint8_t)x;
}
static void put32(uint8_t *p,uint32_t x){
    p[0]=(uint8_t)(x>>24U);p[1]=(uint8_t)(x>>16U);
    p[2]=(uint8_t)(x>>8U);p[3]=(uint8_t)x;
}
static uint16_t get16(const uint8_t *p){
    return (uint16_t)(((uint16_t)p[0]<<8U)|p[1]);
}
static uint32_t get32(const uint8_t *p){
    return ((uint32_t)p[0]<<24U)|((uint32_t)p[1]<<16U)|
           ((uint32_t)p[2]<<8U)|p[3];
}
void ou_shutdown_policy_init(ou_shutdown_policy *p){
    unsigned i;
    if(!p)return;
    *p=(ou_shutdown_policy){0};
    p->shutdown_duration_seconds=120U;
    for(i=0U;i<OU_SHUTDOWN_SEGMENTS;++i)
        p->segments[i].duration_seconds=120U;
}
int ou_shutdown_policy_validate(const ou_shutdown_policy *p){
    unsigned i;
    if(!p||p->shutdown_duration_seconds<120U||
       p->shutdown_duration_seconds>9999U||
       p->shutdown_timer_selected>1U||p->forced_reboot>1U||
       p->shutdown_timer_seconds>5999940U||
       p->shutdown_timer_seconds%60U!=0U||
       p->runtime_limit_seconds>99999U||
       p->capacity_limit_percent>100U||
       p->restart_level_percent>100U)return -1;
    for(i=0U;i<OU_SHUTDOWN_SEGMENTS;++i){
        const ou_shutdown_segment *g=&p->segments[i];
        if(g->duration_seconds<120U||g->duration_seconds>9999U||
           g->shutdown_delay_seconds>99999U||
           g->startup_delay_seconds>65535U||
           g->capacity_limit_percent>100U)return -1;
    }
    return 0;
}
int ou_shutdown_assess(const ou_shutdown_policy *p,const ou_snapshot *s,
                       ou_shutdown_advisory *out){
    ou_shutdown_advisory advisory={0};
    if(!out||!s||ou_shutdown_policy_validate(p)!=0||
       s->origin!=OU_ORIGIN_NATIVE||s->sample_ready!=1U||
       s->battery_present!=1U||s->battery_percent>100U||
       s->output_load_percent>100U)return -1;
    if(s->ac_present==0U){
        advisory.flags=OU_SHUTDOWN_UTILITY_LOST;
        if(p->runtime_limit_seconds>0U &&
           s->estimated_runtime_seconds<=p->runtime_limit_seconds)
            advisory.flags|=OU_SHUTDOWN_LOW_RUNTIME;
        if(p->capacity_limit_percent>0U &&
           s->battery_percent<=p->capacity_limit_percent)
            advisory.flags|=OU_SHUTDOWN_LOW_CAPACITY;
        if(p->segments[0].capacity_limit_percent>0U &&
           s->battery_percent<=p->segments[0].capacity_limit_percent)
            advisory.flags|=OU_SHUTDOWN_SEGMENT_2_LIMIT;
        if(p->segments[1].capacity_limit_percent>0U &&
           s->battery_percent<=p->segments[1].capacity_limit_percent)
            advisory.flags|=OU_SHUTDOWN_SEGMENT_3_LIMIT;
        advisory.recommend_agent_warning=(uint8_t)(
          (advisory.flags&~((uint32_t)OU_SHUTDOWN_UTILITY_LOST))!=0U);
    }else if(p->restart_level_percent>0U &&
             s->battery_percent>=p->restart_level_percent){
        advisory.flags=OU_SHUTDOWN_RESTART_CAPACITY_READY;
    }
    advisory.power_commands_enabled=0U;
    *out=advisory;
    return 0;
}
int ou_shutdown_encode(const ou_shutdown_policy *p,uint8_t *out,size_t len){
    unsigned i;size_t at=24U;
    if(!out||len<OU_SHUTDOWN_RECORD_BYTES||
       ou_shutdown_policy_validate(p)!=0)return -1;
    for(i=0U;i<OU_SHUTDOWN_RECORD_BYTES;++i)out[i]=0U;
    out[0]='O';out[1]='6';out[2]='S';out[3]='D';
    out[4]=1U;out[5]=3U;
    put16(out+8U,p->shutdown_duration_seconds);
    out[10]=p->shutdown_timer_selected;
    out[11]=p->forced_reboot;
    put32(out+12U,p->shutdown_timer_seconds);
    put32(out+16U,p->runtime_limit_seconds);
    out[20]=p->capacity_limit_percent;
    out[21]=p->restart_level_percent;
    for(i=0U;i<OU_SHUTDOWN_SEGMENTS;++i){
        const ou_shutdown_segment *g=&p->segments[i];
        put16(out+at,g->duration_seconds);
        put32(out+at+2U,g->shutdown_delay_seconds);
        put32(out+at+6U,g->startup_delay_seconds);
        out[at+10U]=g->capacity_limit_percent;
        at+=12U;
    }
    return OU_SHUTDOWN_RECORD_BYTES;
}
int ou_shutdown_decode(ou_shutdown_policy *p,const uint8_t *bytes,size_t len){
    ou_shutdown_policy decoded;
    unsigned i;size_t at=24U;
    if(!p||!bytes||len!=OU_SHUTDOWN_RECORD_BYTES||
       bytes[0]!='O'||bytes[1]!='6'||bytes[2]!='S'||bytes[3]!='D'||
       bytes[4]!=1U||bytes[5]!=3U||bytes[6]!=0U||bytes[7]!=0U||
       bytes[22]!=0U||bytes[23]!=0U)return -1;
    for(i=48U;i<OU_SHUTDOWN_RECORD_BYTES;++i)
        if(bytes[i]!=0U)return -1;
    decoded=(ou_shutdown_policy){0};
    decoded.shutdown_duration_seconds=get16(bytes+8U);
    decoded.shutdown_timer_selected=bytes[10];
    decoded.forced_reboot=bytes[11];
    decoded.shutdown_timer_seconds=get32(bytes+12U);
    decoded.runtime_limit_seconds=get32(bytes+16U);
    decoded.capacity_limit_percent=bytes[20];
    decoded.restart_level_percent=bytes[21];
    for(i=0U;i<OU_SHUTDOWN_SEGMENTS;++i){
        ou_shutdown_segment *g=&decoded.segments[i];
        if(bytes[at+11U]!=0U)return -1;
        g->duration_seconds=get16(bytes+at);
        g->shutdown_delay_seconds=get32(bytes+at+2U);
        g->startup_delay_seconds=get32(bytes+at+6U);
        g->capacity_limit_percent=bytes[at+10U];
        at+=12U;
    }
    if(ou_shutdown_policy_validate(&decoded)!=0)return -1;
    *p=decoded;
    return 0;
}
int ou_shutdown_checkpoint(const ou_shutdown_policy *p,
            const ou_journal_io *io,ou_shutdown_authorize_fn authorize,
            void *ctx,ou_journal_record *record){
    uint8_t raw[OU_SHUTDOWN_RECORD_BYTES];
    if(record)*record=(ou_journal_record){0};
    if(!record||!authorize||authorize(ctx)!=1||
       ou_shutdown_encode(p,raw,sizeof(raw))<0)return -1;
    return ou_journal_commit(io,raw,sizeof(raw),record);
}
int ou_shutdown_restore(ou_shutdown_policy *p,const ou_journal_io *io,
                         ou_journal_record *record){
    ou_shutdown_policy candidate;
    ou_journal_record result;
    uint8_t raw[OU_SHUTDOWN_RECORD_BYTES];
    size_t count=0U;
    if(record)*record=(ou_journal_record){0};
    if(!p||!record||!io||
       ou_journal_load(io,raw,sizeof(raw),&count,&result)!=0||
       ou_shutdown_decode(&candidate,raw,count)!=0)return -1;
    *p=candidate;
    *record=result;
    return 0;
}
