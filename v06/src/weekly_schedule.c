#include "ou_weekly_schedule.h"

static int valid_time(int32_t seconds) {
    return seconds==OU_WEEK_DISABLED ||
           (seconds>=0 && seconds<86400 &&
            seconds%(int32_t)OU_WEEK_SLOT_SECONDS==0);
}
static void put32(uint8_t *p,uint32_t v) {
    p[0]=(uint8_t)(v>>24U);p[1]=(uint8_t)(v>>16U);
    p[2]=(uint8_t)(v>>8U);p[3]=(uint8_t)v;
}
static uint32_t get32(const uint8_t *p){
    return ((uint32_t)p[0]<<24U)|((uint32_t)p[1]<<16U)|
           ((uint32_t)p[2]<<8U)|(uint32_t)p[3];
}
void ou_weekly_schedule_init(ou_weekly_schedule *schedule) {
    unsigned day;
    if(!schedule)return;
    for(day=0U;day<OU_WEEK_DAYS;++day) {
        schedule->off[day]=OU_WEEK_DISABLED;
        schedule->on[day]=OU_WEEK_DISABLED;
    }
}
int ou_weekly_schedule_validate(const ou_weekly_schedule *schedule) {
    unsigned day;
    if(!schedule)return -1;
    for(day=0U;day<OU_WEEK_DAYS;++day){
        if(!valid_time(schedule->off[day]) ||
           !valid_time(schedule->on[day]) ||
           (schedule->off[day]!=OU_WEEK_DISABLED &&
            schedule->off[day]==schedule->on[day]))return -1;
    }
    return 0;
}
int ou_weekly_schedule_next(const ou_weekly_schedule *schedule,
                            uint8_t weekday,uint32_t seconds_of_day,
                            ou_weekly_occurrence *next) {
    uint32_t best=UINT32_MAX;
    ou_weekly_occurrence found={0};
    uint32_t day_offset;
    unsigned event_type;
    if(!next||weekday>=OU_WEEK_DAYS||
       seconds_of_day>=OU_WEEK_DAY_SECONDS||
       ou_weekly_schedule_validate(schedule)!=0)return -1;
    for(day_offset=0U;day_offset<=OU_WEEK_DAYS;++day_offset){
        uint8_t candidate_day=(uint8_t)((weekday+day_offset)%OU_WEEK_DAYS);
        for(event_type=0U;event_type<2U;++event_type){
            int32_t time=event_type?schedule->on[candidate_day]:
                                    schedule->off[candidate_day];
            uint32_t delta;
            if(time==OU_WEEK_DISABLED)continue;
            if(day_offset==0U&&(uint32_t)time<seconds_of_day)continue;
            delta=day_offset*OU_WEEK_DAY_SECONDS+(uint32_t)time-
                  seconds_of_day;
            if(delta<best){
                best=delta;
                found.weekday=candidate_day;
                found.power_on=(uint8_t)event_type;
                found.seconds_of_day=(uint32_t)time;
                found.seconds_until=delta;
            }
        }
    }
    if(best==UINT32_MAX)return 0;
    *next=found;
    return 1;
}
int ou_weekly_schedule_encode(const ou_weekly_schedule *schedule,
                              uint8_t *bytes,size_t capacity) {
    unsigned day;size_t offset;
    if(!bytes||capacity<OU_WEEK_RECORD_BYTES||
       ou_weekly_schedule_validate(schedule)!=0)return -1;
    for(offset=0U;offset<OU_WEEK_RECORD_BYTES;++offset)bytes[offset]=0U;
    bytes[0]='O';bytes[1]='6';bytes[2]='S';bytes[3]='C';
    bytes[4]=OU_WEEK_RECORD_VERSION;
    bytes[5]=OU_WEEK_DAYS;
    offset=8U;
    for(day=0U;day<OU_WEEK_DAYS;++day){
        put32(bytes+offset,(uint32_t)schedule->off[day]);offset+=4U;
        put32(bytes+offset,(uint32_t)schedule->on[day]);offset+=4U;
    }
    return offset==OU_WEEK_RECORD_BYTES?(int)offset:-1;
}
int ou_weekly_schedule_decode(ou_weekly_schedule *schedule,
                              const uint8_t *bytes,size_t length) {
    ou_weekly_schedule loaded;
    unsigned day;size_t offset=8U;
    if(!schedule||!bytes||length!=OU_WEEK_RECORD_BYTES||
       bytes[0]!='O'||bytes[1]!='6'||bytes[2]!='S'||bytes[3]!='C'||
       bytes[4]!=OU_WEEK_RECORD_VERSION||bytes[5]!=OU_WEEK_DAYS||
       bytes[6]!=0U||bytes[7]!=0U)return -1;
    for(day=0U;day<OU_WEEK_DAYS;++day){
        uint32_t off=get32(bytes+offset);
        uint32_t on=get32(bytes+offset+4U);
        offset+=8U;
        if((off!=UINT32_MAX&&off>84600U)||
           (on!=UINT32_MAX&&on>84600U))return -1;
        loaded.off[day]=off==UINT32_MAX?-1:(int32_t)off;
        loaded.on[day]=on==UINT32_MAX?-1:(int32_t)on;
    }
    if(ou_weekly_schedule_validate(&loaded)!=0)return -1;
    *schedule=loaded;
    return 0;
}
int ou_weekly_schedule_checkpoint(const ou_weekly_schedule *schedule,
                                  const ou_journal_io *dedicated_schedule,
                                  ou_week_authorize_fn authorize,
                                  void *auth_context,
                                  ou_journal_record *record) {
    uint8_t bytes[OU_WEEK_RECORD_BYTES];
    if(record)*record=(ou_journal_record){0};
    if(!record||!authorize||authorize(auth_context)!=1 ||
       !dedicated_schedule ||
       ou_weekly_schedule_encode(schedule,bytes,sizeof(bytes))<0)
        return -1;
    return ou_journal_commit(dedicated_schedule,bytes,sizeof(bytes),record);
}
int ou_weekly_schedule_restore(ou_weekly_schedule *schedule,
                               const ou_journal_io *dedicated_schedule,
                               ou_journal_record *record) {
    uint8_t bytes[OU_WEEK_RECORD_BYTES];
    ou_journal_record loaded_record;
    ou_weekly_schedule loaded;
    size_t length=0U;
    if(record)*record=(ou_journal_record){0};
    if(!schedule||!record||!dedicated_schedule||
       ou_journal_load(dedicated_schedule,bytes,sizeof(bytes),
                       &length,&loaded_record)!=0 ||
       ou_weekly_schedule_decode(&loaded,bytes,length)!=0)return -1;
    *schedule=loaded;*record=loaded_record;
    return 0;
}
