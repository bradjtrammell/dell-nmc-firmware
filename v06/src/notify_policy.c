#include "ou_notify_policy.h"

static void put16(uint8_t *p,uint16_t value){
    p[0]=(uint8_t)(value>>8U);
    p[1]=(uint8_t)value;
}
static void put32(uint8_t *p,uint32_t value){
    p[0]=(uint8_t)(value>>24U);p[1]=(uint8_t)(value>>16U);
    p[2]=(uint8_t)(value>>8U);p[3]=(uint8_t)value;
}
static uint16_t read16(const uint8_t *p){
    return (uint16_t)(((uint16_t)p[0]<<8U)|p[1]);
}
static uint32_t read32(const uint8_t *p){
    return ((uint32_t)p[0]<<24U)|((uint32_t)p[1]<<16U)|
           ((uint32_t)p[2]<<8U)|p[3];
}
static int valid(const ou_notify_rule *rule){
    if(!rule||rule->enabled>1U||
       rule->transport<OU_NOTIFY_EMAIL||
       rule->transport>OU_NOTIFY_SNMP_TRAP||
       rule->destination>=8U||rule->cooldown_ms>3600000U||
       (rule->event_mask&~UINT16_C(255))!=0U)return 0;
    return !rule->enabled||rule->event_mask!=0U;
}
static int count_rules(const ou_notify_engine *engine,size_t *result){
    size_t i,count=0U;
    int empty=0;
    if(!engine||engine->initialized!=1U||!result)return -1;
    for(i=0U;i<OU_NOTIFY_RULES;++i){
        const ou_notify_rule *r=&engine->rules[i];
        if(r->transport==0U){
            if(r->enabled!=0U||r->event_mask!=0U||
               r->cooldown_ms!=0U||r->destination!=0U)return -1;
            empty=1;
        } else {
            if(empty||!valid(r))return -1;
            ++count;
        }
    }
    *result=count;
    return 0;
}
int ou_notify_policy_encode(const ou_notify_engine *engine,
                            uint8_t *out,size_t capacity){
    size_t i,count,at=8U;
    if(!out||capacity<OU_NOTIFY_POLICY_BYTES||
       count_rules(engine,&count)!=0)return -1;
    for(i=0U;i<OU_NOTIFY_POLICY_BYTES;++i)out[i]=0U;
    out[0]='O';out[1]='6';out[2]='N';out[3]='R';
    out[4]=OU_NOTIFY_POLICY_VERSION;
    out[5]=(uint8_t)count;
    for(i=0U;i<count;++i){
        const ou_notify_rule *r=&engine->rules[i];
        out[at]=r->enabled;
        out[at+1U]=r->transport;
        out[at+2U]=r->destination;
        put16(out+at+4U,r->event_mask);
        put32(out+at+8U,r->cooldown_ms);
        at+=12U;
    }
    return OU_NOTIFY_POLICY_BYTES;
}
int ou_notify_policy_decode(ou_notify_engine *engine,
                            const uint8_t *data,size_t length){
    ou_notify_engine candidate;
    ou_notify_rule rules[OU_NOTIFY_RULES];
    size_t at=8U,i,count;
    if(!engine||engine->initialized!=1U||!data||
       length!=OU_NOTIFY_POLICY_BYTES||
       data[0]!='O'||data[1]!='6'||data[2]!='N'||data[3]!='R'||
       data[4]!=OU_NOTIFY_POLICY_VERSION||
       data[5]>OU_NOTIFY_RULES||data[6]!=0U||data[7]!=0U)return -1;
    count=data[5];
    for(i=0U;i<OU_NOTIFY_RULES;++i){
        const uint8_t *r=data+at;
        size_t j;
        rules[i]=(ou_notify_rule){0};
        if(i<count){
            if(r[3]!=0U||r[6]!=0U||r[7]!=0U)return -1;
            rules[i].enabled=r[0];
            rules[i].transport=r[1];
            rules[i].destination=r[2];
            rules[i].event_mask=read16(r+4U);
            rules[i].cooldown_ms=read32(r+8U);
            if(!valid(&rules[i]))return -1;
        }else{
            for(j=0U;j<12U;++j)if(r[j]!=0U)return -1;
        }
        at+=12U;
    }
    candidate=*engine;
    if(ou_notify_set_rules(&candidate,rules,count,1)!=0)return -1;
    *engine=candidate;
    return 0;
}
int ou_notify_policy_checkpoint(const ou_notify_engine *engine,
                        const ou_journal_io *dedicated_notifications,
                        ou_notify_policy_authorize_fn authorize,
                        void *context,ou_journal_record *record){
    uint8_t image[OU_NOTIFY_POLICY_BYTES];
    if(record)*record=(ou_journal_record){0};
    if(!record||!dedicated_notifications||!authorize||
       authorize(context)!=1 ||
       ou_notify_policy_encode(engine,image,sizeof(image))<0)return -1;
    return ou_journal_commit(dedicated_notifications,image,sizeof(image),record);
}
int ou_notify_policy_restore(ou_notify_engine *engine,
                        const ou_journal_io *dedicated_notifications,
                        ou_journal_record *record){
    uint8_t image[OU_NOTIFY_POLICY_BYTES];
    ou_journal_record meta;
    ou_notify_engine candidate;
    size_t length=0U;
    if(record)*record=(ou_journal_record){0};
    if(!engine||!record||!dedicated_notifications||
       engine->initialized!=1U ||
       ou_journal_load(dedicated_notifications,image,sizeof(image),
                       &length,&meta)!=0 ||
       length!=OU_NOTIFY_POLICY_BYTES)return -1;
    candidate=*engine;
    if(ou_notify_policy_decode(&candidate,image,length)!=0)return -1;
    *engine=candidate;
    *record=meta;
    return 0;
}
