#include "ou_events.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    ou_event_log log={0};
    ou_event_record rec={0};
    unsigned i;
    ou_events_init(&log);
    assert(log.next_id==1U && !log.count);
    assert(ou_events_recent(&log,0U,&rec)==-1);
    assert(ou_events_add(&log,OU_EVENT_SERVICE_START,0U)==0);
    assert(ou_events_recent(&log,0U,&rec)==0);
    assert(rec.id==1U && rec.code==OU_EVENT_SERVICE_START);
    assert(ou_events_add(&log,0U,100U)==-1);
    assert(ou_events_add(&log,255U,100U)==-1);
    assert(log.count==1U && log.next_id==2U);
    for(i=1U;i<OU_EVENTS_CAPACITY+8U;++i)
        assert(ou_events_add(&log,OU_EVENT_TELEMETRY_ONLINE,i*100U)==0);
    assert(log.count==OU_EVENTS_CAPACITY);
    assert(log.overwritten==8U);
    for(i=0U;i<OU_EVENTS_CAPACITY;++i) {
        assert(ou_events_recent(&log,i,&rec)==0);
        assert(rec.id==OU_EVENTS_CAPACITY+8U-i);
        assert(rec.monotonic_ms==((OU_EVENTS_CAPACITY+8U-i)-1U)*100U);
    }
    assert(ou_events_recent(&log,OU_EVENTS_CAPACITY,&rec)==-1);
    log.next_id=UINT32_MAX;
    assert(ou_events_add(&log,OU_EVENT_REPORT_REJECTED,250U)==-1);
    assert(log.overwritten==8U && log.count==OU_EVENTS_CAPACITY);
    assert(ou_events_add(0,OU_EVENT_SERVICE_START,0)==-1);
    assert(ou_events_recent(0,0,&rec)==-1);
    assert(ou_events_recent(&log,0,0)==-1);
    puts("PASS: native event ring 32-slot overwrite, order, typed codes, replay IDs and boundaries");
    return 0;
}
