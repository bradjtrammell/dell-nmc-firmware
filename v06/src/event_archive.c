#include "ou_event_archive.h"

#define OU_EVENT_ARCHIVE_MAGIC UINT32_C(0x4f554556) /* OUEV */
static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0]<<24U)|((uint32_t)p[1]<<16U)|
           ((uint32_t)p[2]<<8U)|(uint32_t)p[3];
}
static void put32(uint8_t *p,uint32_t value) {
    p[0]=(uint8_t)(value>>24U);
    p[1]=(uint8_t)(value>>16U);
    p[2]=(uint8_t)(value>>8U);
    p[3]=(uint8_t)value;
}
static int valid_ring(const ou_event_log *log) {
    uint32_t start;
    size_t i;
    if(!log || !log->next_id || log->next_id==UINT32_MAX ||
       !log->boot_generation || log->count>OU_EVENTS_CAPACITY ||
       log->next_slot>=OU_EVENTS_CAPACITY ||
       log->next_id<=(uint32_t)log->count)return 0;
    if(log->count<OU_EVENTS_CAPACITY &&
       log->next_slot!=log->count)return 0;
    if(log->count==OU_EVENTS_CAPACITY &&
       log->next_slot!=(log->next_id-1U)%OU_EVENTS_CAPACITY)return 0;
    if(log->overwritten!=log->next_id-1U-(uint32_t)log->count)return 0;
    start=log->next_id-(uint32_t)log->count;
    for(i=0U;i<log->count;++i){
        size_t slot=((size_t)log->next_slot+OU_EVENTS_CAPACITY-
                        log->count+i)%OU_EVENTS_CAPACITY;
        const ou_event_record *entry=&log->records[slot];
        if(entry->id!=start+(uint32_t)i ||
           entry->code<OU_EVENT_FIRST || entry->code>OU_EVENT_LAST ||
           !entry->boot_generation ||
           entry->boot_generation>log->boot_generation)return 0;
    }
    return 1;
}
int ou_event_archive_encode(const ou_event_log *log,uint8_t *bytes,
                            size_t capacity){
    size_t i,offset;
    if(!bytes || capacity<OU_EVENT_ARCHIVE_BYTES || !valid_ring(log))
        return -1;
    for(i=0U;i<OU_EVENT_ARCHIVE_BYTES;++i)bytes[i]=0U;
    put32(bytes,OU_EVENT_ARCHIVE_MAGIC);
    put32(bytes+4U,OU_EVENT_ARCHIVE_VERSION);
    put32(bytes+8U,log->next_id);
    put32(bytes+12U,log->boot_generation);
    put32(bytes+16U,log->overwritten);
    bytes[20]=log->count;
    bytes[21]=log->next_slot;
    for(i=0U;i<OU_EVENTS_CAPACITY;++i) {
        const ou_event_record *r=&log->records[i];
        offset=OU_EVENT_ARCHIVE_HEADER+i*OU_EVENT_ARCHIVE_ROW_BYTES;
        put32(bytes+offset,r->id);
        put32(bytes+offset+4U,r->monotonic_ms);
        put32(bytes+offset+8U,r->boot_generation);
        bytes[offset+12U]=r->code;
    }
    return (int)OU_EVENT_ARCHIVE_BYTES;
}
int ou_event_archive_decode(ou_event_log *log,const uint8_t *bytes,
                            size_t length) {
    ou_event_log candidate={0};
    size_t i,offset;
    if(!log || !bytes || length!=OU_EVENT_ARCHIVE_BYTES ||
       be32(bytes)!=OU_EVENT_ARCHIVE_MAGIC ||
       be32(bytes+4U)!=OU_EVENT_ARCHIVE_VERSION ||
       bytes[22]!=0U || bytes[23]!=0U)return -1;
    candidate.next_id=be32(bytes+8U);
    candidate.boot_generation=be32(bytes+12U);
    candidate.overwritten=be32(bytes+16U);
    candidate.count=bytes[20];
    candidate.next_slot=bytes[21];
    for(i=0U;i<OU_EVENTS_CAPACITY;++i){
        ou_event_record *r=&candidate.records[i];
        offset=OU_EVENT_ARCHIVE_HEADER+i*OU_EVENT_ARCHIVE_ROW_BYTES;
        r->id=be32(bytes+offset);
        r->monotonic_ms=be32(bytes+offset+4U);
        r->boot_generation=be32(bytes+offset+8U);
        r->code=bytes[offset+12U];
    }
    if(!valid_ring(&candidate))return -1;
    /* Any inactive slot must be zero to reject malformed hidden records. */
    if(candidate.count<OU_EVENTS_CAPACITY)
        for(i=candidate.count;i<OU_EVENTS_CAPACITY;++i) {
            const ou_event_record *r=&candidate.records[i];
            if(r->id||r->monotonic_ms||r->boot_generation||r->code)
                return -1;
        }
    *log=candidate;
    return 0;
}
int ou_event_archive_restore(ou_event_log *log,
                             const ou_journal_io *dedicated_io){
    uint8_t bytes[OU_EVENT_ARCHIVE_BYTES];
    ou_event_log loaded;
    ou_journal_record record;
    size_t length=0U;
    if(!log||!dedicated_io ||
       ou_journal_load(dedicated_io,bytes,sizeof(bytes),&length,&record)!=0 ||
       ou_event_archive_decode(&loaded,bytes,length)!=0 ||
       loaded.boot_generation==UINT32_MAX)return -1;
    ++loaded.boot_generation;
    /* An archive cannot attest that the physical source survived reboot. */
    loaded.source_online=0U;
    loaded.state_known=0U;
    loaded.last_utility_present=0U;
    loaded.last_battery_low=0U;
    *log=loaded;
    return 0;
}
int ou_event_archive_checkpoint(const ou_event_log *log,
                                const ou_journal_io *dedicated_io,
                                ou_journal_record *record) {
    uint8_t bytes[OU_EVENT_ARCHIVE_BYTES];
    if(!record)return -1;
    *record=(ou_journal_record){0};
    if(!dedicated_io || ou_event_archive_encode(log,bytes,sizeof(bytes))<0)
        return -1;
    return ou_journal_commit(dedicated_io,bytes,sizeof(bytes),record);
}
