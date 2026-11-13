#include "ou_shutdown_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SLOT_SIZE 8192U
static struct {uint8_t data[2][SLOT_SIZE];int fail_after;unsigned writes;} store;
static int read_io(void *ctx,unsigned slot,uint32_t at,uint8_t *out,size_t len){
    (void)ctx;
    if(slot>1U||at>SLOT_SIZE||len>SLOT_SIZE-at)return -1;
    memcpy(out,store.data[slot]+at,len);return 0;
}
static int program_io(void *ctx,unsigned slot,uint32_t at,const uint8_t *src,size_t len){
    size_t i;(void)ctx;
    if(slot>1U||at>SLOT_SIZE||len>SLOT_SIZE-at)return -1;
    for(i=0U;i<len;++i){
        if(store.fail_after==0)return -1;
        if(((uint8_t)~store.data[slot][at+i]&src[i])!=0U)return -1;
        store.data[slot][at+i]&=src[i];
        if(store.fail_after>0)--store.fail_after;
        ++store.writes;
    }
    return 0;
}
static int erase_io(void *ctx,unsigned slot){
    (void)ctx;
    if(slot>1U)return -1;
    memset(store.data[slot],0xff,SLOT_SIZE);return 0;
}
static ou_journal_io journal(void){
    ou_journal_io io={0};
    io.read=read_io;io.program=program_io;io.erase=erase_io;
    io.slot_bytes=SLOT_SIZE;io.erase_block_bytes=SLOT_SIZE;
    io.geometry_verified=1U;io.dedicated_slots_verified=1U;
    io.writes_enabled=1U;
    return io;
}
static int allow(void *ctx){return *(int *)ctx;}
static ou_snapshot sample(uint8_t utility,uint32_t runtime,uint32_t charge){
    ou_snapshot s={0};
    s.origin=OU_ORIGIN_NATIVE;s.sample_ready=1U;
    s.battery_present=1U;s.ac_present=utility;
    s.battery_percent=charge;s.estimated_runtime_seconds=runtime;
    s.output_load_percent=25U;
    return s;
}
static void policy_limits(void){
    ou_shutdown_policy p;
    ou_shutdown_advisory a={0};
    ou_snapshot reading;
    ou_shutdown_policy_init(&p);
    assert(ou_shutdown_policy_validate(&p)==0);
    reading=sample(0U,600U,30U);
    assert(ou_shutdown_assess(&p,&reading,&a)==0);
    assert(a.flags==OU_SHUTDOWN_UTILITY_LOST);
    assert(a.recommend_agent_warning==0U);
    p.shutdown_timer_selected=1U;
    p.shutdown_timer_seconds=120U;
    p.runtime_limit_seconds=900U;
    p.capacity_limit_percent=40U;
    p.restart_level_percent=80U;
    p.forced_reboot=1U;
    p.segments[0].capacity_limit_percent=35U;
    p.segments[1].capacity_limit_percent=20U;
    p.segments[0].startup_delay_seconds=65535U;
    p.segments[1].shutdown_delay_seconds=99999U;
    assert(ou_shutdown_policy_validate(&p)==0);
    assert(ou_shutdown_assess(&p,&reading,&a)==0);
    assert((a.flags&OU_SHUTDOWN_LOW_RUNTIME)!=0U);
    assert((a.flags&OU_SHUTDOWN_LOW_CAPACITY)!=0U);
    assert((a.flags&OU_SHUTDOWN_SEGMENT_2_LIMIT)!=0U);
    assert((a.flags&OU_SHUTDOWN_SEGMENT_3_LIMIT)==0U);
    assert(a.recommend_agent_warning==1U);
    assert(a.power_commands_enabled==0U);
    reading=sample(1U,600U,100U);
    assert(ou_shutdown_assess(&p,&reading,&a)==0);
    assert(a.flags==OU_SHUTDOWN_RESTART_CAPACITY_READY);
    assert(a.recommend_agent_warning==0U);
    assert(a.power_commands_enabled==0U);
    reading.origin=OU_ORIGIN_SIMULATOR;
    a.flags=12345U;
    assert(ou_shutdown_assess(&p,&reading,&a)==-1);
    assert(a.flags==12345U);
    reading=sample(0U,600U,30U);
    reading.sample_ready=0U;
    assert(ou_shutdown_assess(&p,&reading,&a)==-1);
    p.shutdown_duration_seconds=119U;
    assert(ou_shutdown_policy_validate(&p)==-1);
    p.shutdown_duration_seconds=120U;p.shutdown_timer_seconds=121U;
    assert(ou_shutdown_policy_validate(&p)==-1);
    p.shutdown_timer_seconds=5999940U;
    assert(ou_shutdown_policy_validate(&p)==0);
    p.shutdown_timer_seconds=5999941U;
    assert(ou_shutdown_policy_validate(&p)==-1);
    p.shutdown_timer_seconds=0U;
    p.segments[0].duration_seconds=10000U;
    assert(ou_shutdown_policy_validate(&p)==-1);
    p.segments[0].duration_seconds=120U;
    p.segments[0].startup_delay_seconds=65536U;
    assert(ou_shutdown_policy_validate(&p)==-1);
    p.segments[0].startup_delay_seconds=0U;
    p.capacity_limit_percent=101U;
    assert(ou_shutdown_policy_validate(&p)==-1);
}
static void codec_and_recovery(void){
    ou_shutdown_policy p,old,recovered;
    ou_journal_io io=journal();
    ou_journal_record meta;
    uint8_t raw[OU_SHUTDOWN_RECORD_BYTES];
    int authorized=0;
    unsigned prev;
    memset(&store,0,sizeof(store));
    memset(store.data,0xff,sizeof(store.data));
    store.fail_after=-1;
    ou_shutdown_policy_init(&p);
    p.runtime_limit_seconds=1800U;
    p.capacity_limit_percent=40U;
    p.restart_level_percent=75U;
    p.segments[0].shutdown_delay_seconds=300U;
    p.segments[1].startup_delay_seconds=60U;
    old=p;
    assert(ou_shutdown_encode(&p,raw,sizeof(raw))==64);
    assert(raw[0]=='O'&&raw[1]=='6'&&raw[2]=='S'&&raw[3]=='D');
    assert(ou_shutdown_decode(&recovered,raw,sizeof(raw))==0);
    assert(recovered.runtime_limit_seconds==1800U);
    assert(recovered.segments[1].startup_delay_seconds==60U);
    raw[4]=99U;
    assert(ou_shutdown_decode(&recovered,raw,sizeof(raw))==-1);
    assert(recovered.runtime_limit_seconds==1800U);
    raw[4]=1U;raw[48]=1U;
    assert(ou_shutdown_decode(&recovered,raw,sizeof(raw))==-1);
    raw[48]=0U;
    assert(ou_shutdown_decode(&recovered,raw,sizeof(raw)-1U)==-1);
    prev=store.writes;
    assert(ou_shutdown_checkpoint(&p,&io,allow,&authorized,&meta)==-1);
    assert(store.writes==prev);
    authorized=1;
    io.writes_enabled=0U;
    assert(ou_shutdown_checkpoint(&p,&io,allow,&authorized,&meta)==-1);
    io.writes_enabled=1U;
    io.geometry_verified=0U;
    assert(ou_shutdown_checkpoint(&p,&io,allow,&authorized,&meta)==-1);
    io.geometry_verified=1U;
    io.dedicated_slots_verified=0U;
    assert(ou_shutdown_checkpoint(&p,&io,allow,&authorized,&meta)==-1);
    io.dedicated_slots_verified=1U;
    assert(store.writes==prev);
    assert(ou_shutdown_checkpoint(&p,&io,allow,&authorized,&meta)==0);
    assert(meta.valid&&meta.generation==1U);
    assert(ou_shutdown_restore(&recovered,&io,&meta)==0);
    assert(recovered.capacity_limit_percent==40U);
    p.capacity_limit_percent=25U;
    store.fail_after=41;
    assert(ou_shutdown_checkpoint(&p,&io,allow,&authorized,&meta)==-1);
    store.fail_after=-1;
    assert(ou_shutdown_restore(&recovered,&io,&meta)==0);
    assert(recovered.capacity_limit_percent==old.capacity_limit_percent);
    assert(ou_shutdown_checkpoint(&p,&io,allow,&authorized,&meta)==0);
    assert(meta.generation==2U);
    assert(ou_shutdown_restore(&recovered,&io,&meta)==0);
    assert(recovered.capacity_limit_percent==25U);
    store.data[1][OU_JOURNAL_HEADER_BYTES+16U]^=1U;
    assert(ou_shutdown_restore(&recovered,&io,&meta)==0);
    assert(recovered.capacity_limit_percent==40U);
}
int main(void){
    policy_limits();
    codec_and_recovery();
    puts("PASS: OEM-range UPS/segment shutdown policy, read-only threshold advisories and atomic protected journal recovery");
    return 0;
}
