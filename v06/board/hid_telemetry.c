#include "ou_hid_telemetry.h"

static const uint32_t limits[OU_HID_TELEMETRY_METRICS]={
    100U,200U,604800U,3000U,3000U,1U,1U,1U,1U
};
static void clear_samples(ou_hid_telemetry *s) {
    size_t i;
    s->seen=0U;
    for(i=0U;i<OU_HID_TELEMETRY_METRICS;++i){
        s->metrics[i]=0U;s->sampled_ms[i]=0U;
    }
}
void ou_hid_telemetry_revoke(ou_hid_telemetry *s) {
    if(s)*s=(ou_hid_telemetry){0};
}
static int reject(ou_hid_telemetry *s) {
    if(s)clear_samples(s);
    return -1;
}
static int forward(uint32_t newer,uint32_t older) {
    uint32_t d=newer-older;
    return d!=0U&&d<UINT32_C(0x80000000);
}
static int source_ok(const ou_hid_source_evidence *e) {
    return e&&e->installed_ups_identity_verified==1U&&
           e->electrical_link_verified==1U&&
           e->transaction_origin_verified==1U&&
           e->descriptor_origin_verified==1U;
}
static int bindings_ok(const ou_hid_feature_map *map,
                       const ou_hid_metric_binding *b) {
    size_t i,j,k;
    for(i=0U;i<OU_HID_TELEMETRY_METRICS;++i){
        unsigned matches=0U;
        if((b[i].usage_page!=0x84U&&b[i].usage_page!=0x85U)||
           !b[i].usage||!b[i].multiply||b[i].multiply>100U||
           !b[i].divide||b[i].divide>100U)return 0;
        for(k=0U;k<map->field_count;++k){
            const ou_hid_feature_field *f=&map->fields[k];
            if(f->usage_page==b[i].usage_page&&f->usage==b[i].usage&&
               f->report_id==b[i].report_id)++matches;
        }
        if(matches!=1U)return 0;
        for(j=0U;j<i;++j)
            if(b[i].usage_page==b[j].usage_page&&
               b[i].usage==b[j].usage&&b[i].report_id==b[j].report_id)
                return 0;
    }
    return 1;
}
int ou_hid_telemetry_init(ou_hid_telemetry *s,const uint8_t *descriptor,
                          size_t descriptor_size,
                          const ou_hid_metric_binding bindings[OU_HID_TELEMETRY_METRICS],
                          const ou_hid_source_evidence *evidence) {
    size_t i;
    if(!s)return -1;
    ou_hid_telemetry_revoke(s);
    if(!bindings||!source_ok(evidence)||
       ou_hid_feature_parse(descriptor,descriptor_size,&s->map)!=0||
       !bindings_ok(&s->map,bindings)){
        ou_hid_telemetry_revoke(s);
        return -1;
    }
    for(i=0U;i<OU_HID_TELEMETRY_METRICS;++i)s->binding[i]=bindings[i];
    s->initialized=1U;
    return 0;
}
static int scale(const ou_hid_metric_binding *b,int32_t raw,
                 uint32_t max,uint32_t *result) {
    uint32_t val;
    if(raw<0)return 0;
    val=(uint32_t)raw;
    if(val>UINT32_MAX/(uint32_t)b->multiply)return 0;
    val*=b->multiply;
    if(val%b->divide)return 0;
    val/=b->divide;
    if(val>max)return 0;
    *result=val;
    return 1;
}
int ou_hid_telemetry_ingest(ou_hid_telemetry *s,
                            const ou_shut_rx_message *m,uint32_t now_ms) {
    ou_hid_feature_value values[OU_HID_VALUES_CAP];
    uint32_t staged[OU_HID_TELEMETRY_METRICS]={0};
    uint16_t changed=0U;
    size_t i,j,n=0U;
    if(!s||!s->initialized||!m)return reject(s);
    if(m->packet_type!=OU_SHUT_RX_RESPONSE||m->telemetry_verified!=0U||
       !m->length||m->length>OU_SHUT_RX_MESSAGE_CAP||
       now_ms-m->monotonic_ms>OU_SAMPLE_MAX_AGE_MS||
       (s->received&&!forward(m->monotonic_ms,s->last_report_ms))||
       ou_hid_feature_decode(&s->map,m->payload,m->length,
                             values,OU_HID_VALUES_CAP,&n)!=0)return reject(s);
    for(i=0U;i<OU_HID_TELEMETRY_METRICS;++i) {
        unsigned matched=0U;
        const ou_hid_metric_binding *b=&s->binding[i];
        for(j=0U;j<n;++j) {
            if(values[j].usage_page==b->usage_page&&
               values[j].usage==b->usage&&values[j].report_id==b->report_id){
                if(matched||!scale(b,values[j].value,limits[i],&staged[i]))
                    return reject(s);
                ++matched;
            }
        }
        if(matched)changed|=(uint16_t)(1U<<i);
    }
    if(!changed)return reject(s);
    for(i=0U;i<OU_HID_TELEMETRY_METRICS;++i)
        if(changed&(uint16_t)(1U<<i)){
            s->metrics[i]=staged[i];
            s->sampled_ms[i]=m->monotonic_ms;
        }
    s->seen|=changed;
    s->last_report_ms=m->monotonic_ms;
    s->received=1U;
    return 0;
}
int ou_hid_telemetry_take(ou_hid_telemetry *s,uint32_t now_ms,
                           ou_driver_report *out) {
    ou_driver_report r={0};
    ou_snapshot checked;
    size_t i;
    if(out)*out=(ou_driver_report){0};
    if(!s||!out||!s->initialized)return -1;
    if(s->seen!=OU_HID_TELEMETRY_FULL_MASK)return -1;
    for(i=0U;i<OU_HID_TELEMETRY_METRICS;++i)
        if(now_ms-s->sampled_ms[i]>OU_HID_TELEMETRY_SKEW_MS)
            return reject(s);
    if(s->sequence==UINT32_MAX)return reject(s);
    r.version=OU_REPORT_VERSION;r.verified=1U;
    r.sequence=s->sequence+1U;
    r.received_monotonic_ms=now_ms;r.uptime_seconds=now_ms/1000U;
    r.battery_percent=(uint16_t)s->metrics[OU_HID_BATTERY_PERCENT];
    r.load_percent=(uint16_t)s->metrics[OU_HID_LOAD_PERCENT];
    r.runtime_seconds=s->metrics[OU_HID_RUNTIME_SECONDS];
    r.input_decivolts=(uint16_t)s->metrics[OU_HID_INPUT_DECIVOLTS];
    r.output_decivolts=(uint16_t)s->metrics[OU_HID_OUTPUT_DECIVOLTS];
    r.utility_present=(uint8_t)s->metrics[OU_HID_AC_PRESENT];
    r.battery_present=(uint8_t)s->metrics[OU_HID_BATTERY_PRESENT];
    r.segment_1_on=(uint8_t)s->metrics[OU_HID_SEGMENT_1_ON];
    r.segment_2_on=(uint8_t)s->metrics[OU_HID_SEGMENT_2_ON];
    if(ou_provider_accept(&r,now_ms,&checked)!=0)return reject(s);
    s->sequence=r.sequence;
    clear_samples(s);
    *out=r;
    return 0;
}
