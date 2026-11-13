#include "ou_event_archive.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SLOT_BYTES 8192U
typedef struct {
    uint8_t slots[2][SLOT_BYTES];
    int fail_after;
    unsigned writes,erases;
} memory_nor;
static memory_nor chip;
static void blank(void) {
    memset(&chip,0,sizeof(chip));
    memset(chip.slots,0xff,sizeof(chip.slots));
    chip.fail_after=-1;
}
static int read_slot(void *ctx,unsigned slot,uint32_t off,
                     uint8_t *out,size_t count) {
    memory_nor *m=ctx;
    if(slot>1U || off>SLOT_BYTES || count>SLOT_BYTES-off)return -1;
    memcpy(out,m->slots[slot]+off,count);
    return 0;
}
static int write_slot(void *ctx,unsigned slot,uint32_t off,
                      const uint8_t *data,size_t count) {
    memory_nor *m=ctx;
    size_t i;
    if(slot>1U || off>SLOT_BYTES || count>SLOT_BYTES-off)return -1;
    for(i=0U;i<count;++i) {
        if(m->fail_after==0)return -1;
        if(((uint8_t)~m->slots[slot][off+i]&data[i])!=0U)return -1;
        m->slots[slot][off+i]&=data[i];
        if(m->fail_after>0)--m->fail_after;
        ++m->writes;
    }
    return 0;
}
static int erase_slot(void *ctx,unsigned slot){
    memory_nor *m=ctx;
    if(slot>1U)return -1;
    memset(m->slots[slot],0xff,SLOT_BYTES);
    ++m->erases;
    return 0;
}
static ou_journal_io io_for(memory_nor *m) {
    ou_journal_io io={0};
    io.context=m;
    io.read=read_slot;
    io.program=write_slot;
    io.erase=erase_slot;
    io.slot_bytes=SLOT_BYTES;
    io.erase_block_bytes=SLOT_BYTES;
    io.geometry_verified=1U; /* mock geometry, never H910P evidence */
    io.dedicated_slots_verified=1U; /* mock partition */
    io.writes_enabled=1U;
    return io;
}
static void fill(ou_event_log *log,unsigned count) {
    unsigned i;
    ou_events_init(log);
    for(i=0U;i<count;++i)
        assert(ou_events_add(log,(uint8_t)(OU_EVENT_FIRST+
          i%(OU_EVENT_LAST-OU_EVENT_FIRST+1U)),i*777U)==0);
}
static void roundtrip(void) {
    ou_event_log original,copy,unchanged;
    uint8_t bytes[OU_EVENT_ARCHIVE_BYTES];
    ou_event_record event;
    unsigned i;
    fill(&original,40U);
    assert(ou_event_archive_encode(&original,bytes,sizeof(bytes))==
           (int)sizeof(bytes));
    assert(ou_event_archive_decode(&copy,bytes,sizeof(bytes))==0);
    assert(copy.count==32U && copy.overwritten==8U);
    assert(copy.next_id==41U && copy.boot_generation==1U);
    for(i=0U;i<32U;++i) {
        assert(ou_events_recent(&copy,i,&event)==0);
        assert(event.id==40U-i && event.boot_generation==1U);
        assert(event.monotonic_ms==(39U-i)*777U);
    }
    unchanged=copy;
    bytes[22]=1U;
    assert(ou_event_archive_decode(&copy,bytes,sizeof(bytes))==-1);
    assert(memcmp(&copy,&unchanged,sizeof(copy))==0);
    bytes[22]=0U;
    bytes[OU_EVENT_ARCHIVE_HEADER+12U]=255U;
    assert(ou_event_archive_decode(&copy,bytes,sizeof(bytes))==-1);
    bytes[OU_EVENT_ARCHIVE_HEADER+12U]=9U; /* oldest retained uses code 9? */
    /* Recover exact serialized content from original. */
    assert(ou_event_archive_encode(&original,bytes,sizeof(bytes))==
           (int)sizeof(bytes));
    assert(ou_event_archive_decode(&copy,bytes,sizeof(bytes)-1U)==-1);
    assert(memcmp(&copy,&unchanged,sizeof(copy))==0);
    fill(&original,1U);
    assert(ou_event_archive_encode(&original,bytes,sizeof(bytes))==
           (int)sizeof(bytes));
    bytes[OU_EVENT_ARCHIVE_HEADER+OU_EVENT_ARCHIVE_ROW_BYTES+1U]=4U;
    assert(ou_event_archive_decode(&copy,bytes,sizeof(bytes))==-1);
    assert(memcmp(&copy,&unchanged,sizeof(copy))==0);
}
static void power_loss_and_restore(void){
    ou_event_log log,reloaded;
    ou_journal_io io;
    ou_journal_record result;
    ou_event_record newest;
    unsigned i;
    blank();io=io_for(&chip);
    fill(&log,37U);
    assert(ou_event_archive_checkpoint(&log,&io,&result)==0);
    assert(result.valid && result.generation==1U && result.slot==0U);
    fill(&log,40U);
    chip.fail_after=100;
    assert(ou_event_archive_checkpoint(&log,&io,&result)==-1);
    chip.fail_after=-1;
    ou_events_init(&reloaded);
    assert(ou_event_archive_restore(&reloaded,&io)==0);
    assert(reloaded.next_id==38U && reloaded.boot_generation==2U);
    assert(reloaded.source_online==0U && reloaded.state_known==0U);
    assert(ou_events_recent(&reloaded,0U,&newest)==0);
    assert(newest.id==37U && newest.boot_generation==1U);
    assert(ou_events_add(&reloaded,OU_EVENT_SERVICE_START,0U)==0);
    assert(ou_events_recent(&reloaded,0U,&newest)==0);
    assert(newest.id==38U && newest.boot_generation==2U);
    assert(newest.monotonic_ms==0U);
    assert(ou_event_archive_checkpoint(&reloaded,&io,&result)==0);
    assert(result.generation==2U && result.slot==1U);
    /* Corrupt latest committed page; journal restores older copy. */
    chip.slots[1][OU_JOURNAL_HEADER_BYTES+8U]^=1U;
    fill(&log,3U);
    assert(ou_event_archive_restore(&log,&io)==0);
    assert(log.next_id==38U && log.boot_generation==2U);
    /* Corrupt both; caller's state is untouched. */
    chip.slots[0][OU_JOURNAL_HEADER_BYTES+8U]^=1U;
    reloaded=log;
    assert(ou_event_archive_restore(&log,&io)==-1);
    assert(memcmp(&log,&reloaded,sizeof(log))==0);
    for(i=0U;i<32U;++i)
        assert(ou_events_recent(&log,i,&newest)==0);
}
static void no_unqualified_flash_writes(void) {
    ou_event_log log;
    ou_journal_record record;
    ou_journal_io io;
    unsigned writes,erases;
    blank();io=io_for(&chip);
    fill(&log,1U);
    writes=chip.writes;erases=chip.erases;
    io.geometry_verified=0U;
    assert(ou_event_archive_checkpoint(&log,&io,&record)==-1);
    io.geometry_verified=1U;
    io.dedicated_slots_verified=0U;
    assert(ou_event_archive_checkpoint(&log,&io,&record)==-1);
    io.dedicated_slots_verified=1U;
    io.writes_enabled=0U;
    assert(ou_event_archive_checkpoint(&log,&io,&record)==-1);
    assert(chip.writes==writes && chip.erases==erases);
    assert(ou_event_archive_checkpoint(&log,0,&record)==-1);
    assert(ou_event_archive_checkpoint(&log,&io,0)==-1);
    log.boot_generation=0U;
    io.writes_enabled=1U;
    assert(ou_event_archive_checkpoint(&log,&io,&record)==-1);
    assert(chip.writes==writes && chip.erases==erases);
}
int main(void) {
    roundtrip();
    power_loss_and_restore();
    no_unqualified_flash_writes();
    puts("PASS: event snapshot endian format, boot epochs, power failure, CRC fallback, write permission gates");
    return 0;
}
