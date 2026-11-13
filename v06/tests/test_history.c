#include "ou_history.h"
#include <assert.h>
#include <stdio.h>
static ou_snapshot sample(uint32_t seq,uint32_t battery,
                          uint32_t ac) {
    ou_snapshot s={0};
    s.origin=OU_ORIGIN_NATIVE;s.sample_ready=1U;
    s.sequence=seq;s.battery_percent=battery;s.output_load_percent=44U;
    s.estimated_runtime_seconds=1800U;
    s.input_voltage_decivolts=1180U;
    s.output_voltage_decivolts=1200U;s.ac_present=(uint8_t)ac;
    s.battery_present=1U;
    return s;
}
int main(void) {
    ou_history_log log;
    ou_history_sample row;
    ou_snapshot s=sample(1U,90U,1U);
    unsigned i;
    ou_history_init(&log);
    assert(ou_history_recent(&log,0U,&row)==-1);
    s.origin=OU_ORIGIN_SIMULATOR;
    assert(ou_history_accept(&log,&s,0U)==-1);
    s.origin=OU_ORIGIN_NATIVE;s.sample_ready=0U;
    assert(ou_history_accept(&log,&s,0U)==-1);
    s.sample_ready=1U;
    assert(ou_history_accept(&log,&s,1000U)==1);
    assert(log.count==1U&&log.latest_sequence==1U);
    s.sequence=2U;
    assert(ou_history_accept(&log,&s,2000U)==0);
    assert(ou_history_accept(&log,&s,3000U)==-1);
    s.sequence=3U;s.ac_present=0U;
    assert(ou_history_accept(&log,&s,3000U)==1);
    assert(log.count==2U);
    s.sequence=4U;s.battery_percent=19U;
    assert(ou_history_accept(&log,&s,4000U)==1);
    s.sequence=5U;s.battery_percent=18U;
    assert(ou_history_accept(&log,&s,5000U)==0);
    ou_history_source_lost(&log);
    s.sequence=6U;
    assert(ou_history_accept(&log,&s,6000U)==1);
    assert(ou_history_recent(&log,0U,&row)==0);
    assert(row.gap_before==1U&&row.sequence==6U);
    s.sequence=7U;
    assert(ou_history_accept(&log,&s,66000U)==1);
    assert(ou_history_recent(&log,0U,&row)==0&&row.gap_before==0U);
    assert(ou_history_accept(&log,&s,67000U)==-1);
    s.sequence=8U;s.battery_percent=101U;
    assert(ou_history_accept(&log,&s,67000U)==-1);
    s.battery_percent=30U;
    assert(ou_history_accept(&log,&s,67000U)==1);
    assert(log.count==6U);
    for(i=0U;i<OU_HISTORY_CAPACITY+10U;++i){
        s.sequence=9U+i;
        s.battery_percent=30U;
        assert(ou_history_accept(&log,&s,128000U+i*61000U)==1);
    }
    assert(log.count==OU_HISTORY_CAPACITY);
    assert(log.overwritten==16U);
    assert(ou_history_recent(&log,0U,&row)==0);
    assert(row.sequence==8U+OU_HISTORY_CAPACITY+10U);
    assert(ou_history_recent(&log,OU_HISTORY_CAPACITY,&row)==-1);
    assert(ou_history_accept(0,&s,0U)==-1);
    assert(ou_history_recent(&log,0U,0)==-1);
    ou_history_init(&log);
    s=sample(1U,80U,1U);
    assert(ou_history_accept(&log,&s,UINT32_MAX-30000U)==1);
    s.sequence=2U;
    assert(ou_history_accept(&log,&s,30000U)==1);
    assert(log.count==2U);
    puts("PASS: native verified history, 60s cadence, AC/low-battery edge, gap, rollover, reject, wrap");
    return 0;
}
