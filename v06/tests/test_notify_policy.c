#include "ou_notify_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SLOT_BYTES 8192U

static struct {
    uint8_t sectors[2][SLOT_BYTES];
    int fail_after;
    uint32_t writes,erases;
} flash;
static int read_sector(void *context,unsigned slot,uint32_t offset,
                       uint8_t *out,size_t length){
    (void)context;
    if(slot>1U||offset>SLOT_BYTES||length>SLOT_BYTES-offset)return -1;
    memcpy(out,flash.sectors[slot]+offset,length);return 0;
}
static int program_sector(void *context,unsigned slot,uint32_t offset,
                          const uint8_t *data,size_t length){
    size_t i;(void)context;
    if(slot>1U||offset>SLOT_BYTES||length>SLOT_BYTES-offset)return -1;
    for(i=0U;i<length;++i){
        if(flash.fail_after==0)return -1;
        if(((uint8_t)~flash.sectors[slot][offset+i]&data[i])!=0U)return -1;
        flash.sectors[slot][offset+i]&=data[i];
        if(flash.fail_after>0)--flash.fail_after;
        ++flash.writes;
    }
    return 0;
}
static int erase_sector(void *context,unsigned slot){
    (void)context;
    if(slot>1U)return -1;
    memset(flash.sectors[slot],0xff,SLOT_BYTES);
    ++flash.erases;return 0;
}
static ou_journal_io journal(void){
    ou_journal_io io={0};
    io.read=read_sector;io.program=program_sector;io.erase=erase_sector;
    io.slot_bytes=SLOT_BYTES;io.erase_block_bytes=SLOT_BYTES;
    io.geometry_verified=1U;io.dedicated_slots_verified=1U;
    io.writes_enabled=1U;
    return io;
}
static void init_flash(void){
    memset(&flash,0,sizeof(flash));
    memset(flash.sectors,0xff,sizeof(flash.sectors));
    flash.fail_after=-1;
}
static int authorize(void *context){return *(int *)context;}
static ou_notify_engine configured(void){
    ou_notify_engine n;
    ou_notify_rule rules[3]={{0}};
    ou_notify_init(&n);
    rules[0].enabled=1U;
    rules[0].transport=OU_NOTIFY_EMAIL;
    rules[0].destination=2U;
    rules[0].event_mask=(uint16_t)(1U<<(OU_EVENT_BATTERY_LOW-1U));
    rules[0].cooldown_ms=60000U;
    rules[1]=rules[0];
    rules[1].transport=OU_NOTIFY_WEBHOOK;
    rules[1].destination=3U;
    rules[1].cooldown_ms=30000U;
    rules[2]=rules[0];
    rules[2].enabled=0U;
    rules[2].transport=OU_NOTIFY_SNMP_TRAP;
    rules[2].destination=1U;
    assert(ou_notify_set_rules(&n,rules,3U,1)==0);
    return n;
}
static void canonical_and_corruption(void){
    ou_notify_engine original=configured(),recovered;
    uint8_t record[OU_NOTIFY_POLICY_BYTES],first[OU_NOTIFY_POLICY_BYTES];
    ou_event_record event={0};
    assert(ou_notify_policy_encode(&original,record,sizeof(record))==
           (int)sizeof(record));
    assert(record[0]=='O'&&record[1]=='6'&&record[2]=='N'&&record[3]=='R');
    assert(record[4]==1U&&record[5]==3U);
    memcpy(first,record,sizeof(record));
    ou_notify_init(&recovered);
    assert(ou_notify_policy_decode(&recovered,record,sizeof(record))==0);
    assert(recovered.rules[0].event_mask==original.rules[0].event_mask);
    assert(recovered.rules[0].cooldown_ms==60000U);
    assert(recovered.rules[1].destination==3U);
    assert(recovered.rules[2].enabled==0U);
    assert(recovered.rules[2].transport==OU_NOTIFY_SNMP_TRAP);
    assert(ou_notify_policy_encode(&recovered,record,sizeof(record))==
           OU_NOTIFY_POLICY_BYTES);
    assert(memcmp(first,record,sizeof(record))==0);
    /* Queue state is retained only until decoded new policy, which clears
     * previously pending jobs addressed under old recipient settings. */
    event.id=5U;event.boot_generation=1U;event.code=OU_EVENT_BATTERY_LOW;
    assert(ou_notify_route(&recovered,&event,10U)==2);
    assert(ou_notify_pending(&recovered)==2U);
    assert(ou_notify_policy_decode(&recovered,first,sizeof(first))==0);
    assert(ou_notify_pending(&recovered)==0U);
    assert(recovered.dropped==2U);
    record[5]=9U;
    assert(ou_notify_policy_decode(&recovered,record,sizeof(record))==-1);
    assert(recovered.dropped==2U);
    memcpy(record,first,sizeof(record));
    record[6]=1U;
    assert(ou_notify_policy_decode(&recovered,record,sizeof(record))==-1);
    memcpy(record,first,sizeof(record));
    record[8U+3U]=1U;
    assert(ou_notify_policy_decode(&recovered,record,sizeof(record))==-1);
    memcpy(record,first,sizeof(record));
    record[8U+4U]=0xffU;
    assert(ou_notify_policy_decode(&recovered,record,sizeof(record))==-1);
    memcpy(record,first,sizeof(record));
    record[8U+12U*3U]=1U;
    assert(ou_notify_policy_decode(&recovered,record,sizeof(record))==-1);
    assert(ou_notify_policy_decode(&recovered,first,sizeof(first)-1U)==-1);
    assert(ou_notify_policy_encode(&recovered,record,40U)==-1);
    assert(ou_notify_policy_decode(NULL,first,sizeof(first))==-1);
}
static void journal_recovery_and_gate(void){
    ou_notify_engine n=configured(),loaded;
    ou_notify_rule update[1]={{0}};
    ou_journal_record meta;
    ou_journal_io io;
    int allowed=0;
    uint32_t writes;
    init_flash();
    io=journal();
    writes=flash.writes;
    assert(ou_notify_policy_checkpoint(&n,&io,authorize,&allowed,&meta)==-1);
    assert(flash.writes==writes&&flash.erases==0U);
    allowed=1;
    io.geometry_verified=0U;
    assert(ou_notify_policy_checkpoint(&n,&io,authorize,&allowed,&meta)==-1);
    io.geometry_verified=1U;
    io.dedicated_slots_verified=0U;
    assert(ou_notify_policy_checkpoint(&n,&io,authorize,&allowed,&meta)==-1);
    io.dedicated_slots_verified=1U;
    io.writes_enabled=0U;
    assert(ou_notify_policy_checkpoint(&n,&io,authorize,&allowed,&meta)==-1);
    assert(flash.writes==writes&&flash.erases==0U);
    io.writes_enabled=1U;
    assert(ou_notify_policy_checkpoint(&n,&io,authorize,&allowed,&meta)==0);
    assert(meta.valid==1U&&meta.generation==1U&&meta.slot==0U);
    ou_notify_init(&loaded);
    assert(ou_notify_policy_restore(&loaded,&io,&meta)==0);
    assert(loaded.rules[0].transport==OU_NOTIFY_EMAIL);
    update[0].enabled=1U;
    update[0].transport=OU_NOTIFY_SNMP_TRAP;
    update[0].destination=4U;
    update[0].event_mask=(uint16_t)(1U<<(OU_EVENT_UTILITY_LOST-1U));
    assert(ou_notify_set_rules(&n,update,1U,1)==0);
    flash.fail_after=34;
    assert(ou_notify_policy_checkpoint(&n,&io,authorize,&allowed,&meta)==-1);
    flash.fail_after=-1;
    assert(ou_notify_policy_restore(&loaded,&io,&meta)==0);
    assert(loaded.rules[0].transport==OU_NOTIFY_EMAIL);
    assert(ou_notify_policy_checkpoint(&n,&io,authorize,&allowed,&meta)==0);
    assert(meta.generation==2U&&meta.slot==1U);
    assert(ou_notify_policy_restore(&loaded,&io,&meta)==0);
    assert(loaded.rules[0].transport==OU_NOTIFY_SNMP_TRAP);
    flash.sectors[1][OU_JOURNAL_HEADER_BYTES+7U]^=1U;
    assert(ou_notify_policy_restore(&loaded,&io,&meta)==0);
    assert(loaded.rules[0].transport==OU_NOTIFY_EMAIL);
    flash.sectors[0][OU_JOURNAL_HEADER_BYTES+7U]^=1U;
    assert(ou_notify_policy_restore(&loaded,&io,&meta)==-1);
    assert(loaded.rules[0].transport==OU_NOTIFY_EMAIL);
}
int main(void){
    canonical_and_corruption();
    journal_recovery_and_gate();
    puts("PASS: canonical notification policy, authorization, NOR journal rollback, corrupt record rejection and atomic restore");
    return 0;
}
