#include "ou_history.h"

void ou_history_init(ou_history_log *log) {
    if(log)*log=(ou_history_log){0};
}
void ou_history_source_lost(ou_history_log *log){
    if(log)log->gap_pending=1U;
}
int ou_history_recent(const ou_history_log *log,size_t index,
                      ou_history_sample *sample){
    size_t slot;
    if(!log || !sample || log->count>OU_HISTORY_CAPACITY ||
       log->next_slot>=OU_HISTORY_CAPACITY || index>=log->count)return -1;
    slot=((size_t)log->next_slot+OU_HISTORY_CAPACITY-1U-index)%
          OU_HISTORY_CAPACITY;
    *sample=log->samples[slot];
    return sample->sequence?0:-1;
}
int ou_history_accept(ou_history_log *log,const ou_snapshot *s,
                      uint32_t now_ms){
    ou_history_sample *slot;
    uint32_t age;
    uint8_t capture;
    const ou_history_sample *prior;
    if(!log||!s||s->origin!=OU_ORIGIN_NATIVE||s->sample_ready!=1U||
       s->sequence==0U||s->battery_percent>100U||
       s->output_load_percent>100U||s->ac_present>1U||
       s->input_voltage_decivolts>65535U||
       s->output_voltage_decivolts>65535U||
       log->count>OU_HISTORY_CAPACITY||
       log->next_slot>=OU_HISTORY_CAPACITY)return -1;
    if(log->latest_sequence!=0U){
        uint32_t seq_delta=s->sequence-log->latest_sequence;
        if(seq_delta==0U||seq_delta>=UINT32_C(0x80000000))return -1;
    }
    capture=(uint8_t)(log->count==0U||log->gap_pending!=0U);
    if(log->count) {
        prior=&log->samples[((size_t)log->next_slot+
              OU_HISTORY_CAPACITY-1U)%OU_HISTORY_CAPACITY];
        age=now_ms-log->last_capture_ms;
        if(age>=UINT32_C(0x80000000))return -1;
        if(age>=OU_HISTORY_INTERVAL_MS ||
           s->ac_present!=prior->ac_present ||
           (s->battery_percent<=20U)!=(prior->battery_percent<=20U))
            capture=1U;
    }
    /* Always advance watermark even when the sample is rate-limited. */
    log->latest_sequence=s->sequence;
    if(!capture)return 0;
    slot=&log->samples[log->next_slot];
    slot->sequence=s->sequence;
    slot->monotonic_ms=now_ms;
    slot->runtime_seconds=s->estimated_runtime_seconds;
    slot->input_decivolts=(uint16_t)s->input_voltage_decivolts;
    slot->output_decivolts=(uint16_t)s->output_voltage_decivolts;
    slot->battery_percent=(uint8_t)s->battery_percent;
    slot->load_percent=(uint8_t)s->output_load_percent;
    slot->ac_present=s->ac_present;
    slot->gap_before=(uint8_t)(log->gap_pending && log->count>0U);
    log->gap_pending=0U;
    log->last_capture_ms=now_ms;
    log->next_slot=(uint8_t)((log->next_slot+1U)%OU_HISTORY_CAPACITY);
    if(log->count<OU_HISTORY_CAPACITY)++log->count;
    else if(log->overwritten!=UINT32_MAX)++log->overwritten;
    return 1;
}
