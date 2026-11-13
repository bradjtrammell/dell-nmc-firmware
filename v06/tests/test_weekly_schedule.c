#include "ou_weekly_schedule.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SLOT 8192U
static struct {
    uint8_t storage[2][SLOT];
    int fail_after;
    unsigned writes,erases;
} nor;
static int read_slot(void *ctx,unsigned slot,uint32_t offset,uint8_t *out,size_t len) {
    (void)ctx;
    if(slot>1U||offset>SLOT||len>SLOT-offset)return -1;
    memcpy(out,nor.storage[slot]+offset,len);return 0;
}
static int write_slot(void *ctx,unsigned slot,uint32_t offset,
                      const uint8_t *bytes,size_t len){
    size_t i;(void)ctx;
    if(slot>1U||offset>SLOT||len>SLOT-offset)return -1;
    for(i=0U;i<len;++i) {
        if(nor.fail_after==0)return -1;
        if(((uint8_t)~nor.storage[slot][offset+i]&bytes[i])!=0U)return -1;
        nor.storage[slot][offset+i]&=bytes[i];
        ++nor.writes;
        if(nor.fail_after>0)--nor.fail_after;
    }
    return 0;
}
static int erase_slot(void *ctx,unsigned slot){
    (void)ctx;
    if(slot>1U)return -1;
    memset(nor.storage[slot],0xff,SLOT);
    ++nor.erases;return 0;
}
static ou_journal_io io(void){
    ou_journal_io j={0};
    j.read=read_slot;j.program=write_slot;j.erase=erase_slot;
    j.slot_bytes=SLOT;j.erase_block_bytes=SLOT;
    j.geometry_verified=1U;j.dedicated_slots_verified=1U;
    j.writes_enabled=1U;return j;
}
static int auth(void *context){return *(int *)context;}
static void blank(void){
    memset(&nor,0,sizeof(nor));
    memset(nor.storage,0xff,sizeof(nor.storage));
    nor.fail_after=-1;
}
static void planner(void){
    ou_weekly_schedule weekly;
    ou_weekly_occurrence next={0};
    ou_weekly_schedule_init(&weekly);
    assert(ou_weekly_schedule_validate(&weekly)==0);
    assert(ou_weekly_schedule_next(&weekly,0U,0U,&next)==0);
    weekly.off[0]=8*3600;
    weekly.on[0]=18*3600;
    weekly.off[1]=0;
    assert(ou_weekly_schedule_validate(&weekly)==0);
    assert(ou_weekly_schedule_next(&weekly,0U,7*3600,&next)==1);
    assert(next.weekday==0U&&next.power_on==0U);
    assert(next.seconds_of_day==28800U && next.seconds_until==3600U);
    assert(ou_weekly_schedule_next(&weekly,0U,28800U,&next)==1);
    assert(next.seconds_until==0U);
    assert(ou_weekly_schedule_next(&weekly,0U,17*3600,&next)==1);
    assert(next.power_on==1U&&next.seconds_until==3600U);
    assert(ou_weekly_schedule_next(&weekly,0U,20*3600,&next)==1);
    assert(next.weekday==1U&&next.seconds_until==14400U);
    assert(ou_weekly_schedule_next(&weekly,1U,1U,&next)==1);
    assert(next.weekday==0U&&next.seconds_until==
           6U*86400U+28800U-1U);
    assert(ou_weekly_schedule_next(&weekly,7U,0U,&next)==-1);
    assert(ou_weekly_schedule_next(&weekly,1U,86400U,&next)==-1);
    weekly.on[0]=weekly.off[0];
    assert(ou_weekly_schedule_validate(&weekly)==-1);
    weekly.on[0]=84600;
    assert(ou_weekly_schedule_validate(&weekly)==0);
    weekly.on[0]=86399;
    assert(ou_weekly_schedule_validate(&weekly)==-1);
    weekly.on[0]=900;
    assert(ou_weekly_schedule_validate(&weekly)==-1);
    weekly.on[0]=-2;
    assert(ou_weekly_schedule_validate(&weekly)==-1);
}
static void serialization(void){
    ou_weekly_schedule s,loaded,before;
    uint8_t bytes[OU_WEEK_RECORD_BYTES];
    ou_weekly_schedule_init(&s);
    s.off[0]=0;s.on[0]=84600;
    s.off[2]=43200;s.on[2]=0;
    assert(ou_weekly_schedule_encode(&s,bytes,sizeof(bytes))==(int)sizeof(bytes));
    assert(bytes[0]=='O'&&bytes[1]=='6'&&bytes[2]=='S'&&bytes[3]=='C');
    assert(bytes[8]==0U && bytes[9]==0U);
    assert(bytes[16]==0xffU&&bytes[17]==0xffU&&bytes[18]==0xffU&&bytes[19]==0xffU);
    assert(ou_weekly_schedule_decode(&loaded,bytes,sizeof(bytes))==0);
    assert(memcmp(&loaded,&s,sizeof(s))==0);
    before=loaded;
    bytes[4]=99U;
    assert(ou_weekly_schedule_decode(&loaded,bytes,sizeof(bytes))==-1);
    assert(memcmp(&loaded,&before,sizeof(loaded))==0);
    bytes[4]=OU_WEEK_RECORD_VERSION;
    bytes[8]=1U;
    assert(ou_weekly_schedule_decode(&loaded,bytes,sizeof(bytes))==-1);
    assert(memcmp(&loaded,&before,sizeof(loaded))==0);
    bytes[8]=0U;
    bytes[6]=1U;
    assert(ou_weekly_schedule_decode(&loaded,bytes,sizeof(bytes))==-1);
    bytes[6]=0U;
    assert(ou_weekly_schedule_decode(&loaded,bytes,sizeof(bytes)-1U)==-1);
    assert(ou_weekly_schedule_encode(&s,bytes,12U)==-1);
    assert(ou_weekly_schedule_next(&s,0,0,0)==-1);
}
static void nor_recovery(void){
    ou_weekly_schedule s,loaded,old;
    ou_journal_record meta;
    ou_journal_io storage;
    int authorized=0;
    unsigned writes,erases;
    blank();storage=io();
    ou_weekly_schedule_init(&s);
    s.off[0]=28800;s.on[0]=64800;
    old=s;
    writes=nor.writes;erases=nor.erases;
    assert(ou_weekly_schedule_checkpoint(&s,&storage,auth,&authorized,&meta)==-1);
    assert(nor.writes==writes&&nor.erases==erases);
    authorized=1;
    storage.writes_enabled=0U;
    assert(ou_weekly_schedule_checkpoint(&s,&storage,auth,&authorized,&meta)==-1);
    storage.writes_enabled=1U;
    storage.geometry_verified=0U;
    assert(ou_weekly_schedule_checkpoint(&s,&storage,auth,&authorized,&meta)==-1);
    storage.geometry_verified=1U;
    storage.dedicated_slots_verified=0U;
    assert(ou_weekly_schedule_checkpoint(&s,&storage,auth,&authorized,&meta)==-1);
    assert(nor.writes==writes&&nor.erases==erases);
    storage.dedicated_slots_verified=1U;
    assert(ou_weekly_schedule_checkpoint(&s,&storage,auth,&authorized,&meta)==0);
    assert(meta.valid&&meta.generation==1U&&meta.slot==0U);
    assert(ou_weekly_schedule_restore(&loaded,&storage,&meta)==0);
    assert(memcmp(&loaded,&s,sizeof(s))==0);
    s.off[0]=36000;
    nor.fail_after=40;
    assert(ou_weekly_schedule_checkpoint(&s,&storage,auth,&authorized,&meta)==-1);
    nor.fail_after=-1;
    assert(ou_weekly_schedule_restore(&loaded,&storage,&meta)==0);
    assert(memcmp(&loaded,&old,sizeof(old))==0);
    assert(ou_weekly_schedule_checkpoint(&s,&storage,auth,&authorized,&meta)==0);
    assert(meta.valid&&meta.generation==2U&&meta.slot==1U);
    assert(ou_weekly_schedule_restore(&loaded,&storage,&meta)==0);
    assert(memcmp(&loaded,&s,sizeof(s))==0);
    nor.storage[1][OU_JOURNAL_HEADER_BYTES+16U]^=1U;
    assert(ou_weekly_schedule_restore(&loaded,&storage,&meta)==0);
    assert(memcmp(&loaded,&old,sizeof(old))==0);
    nor.storage[0][OU_JOURNAL_HEADER_BYTES+16U]^=1U;
    loaded=s;
    assert(ou_weekly_schedule_restore(&loaded,&storage,&meta)==-1);
    assert(memcmp(&loaded,&s,sizeof(s))==0);
}
int main(void){
    planner();
    serialization();
    nor_recovery();
    puts("PASS: seven-day 30-minute ON/OFF scheduling, validation, next-event planning, canonical journal and power-loss recovery");
    return 0;
}
