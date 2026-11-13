#include "ou_ups_monitor.h"

static int forward_or_equal(uint32_t newer,uint32_t older) {
    return (uint32_t)(newer-older)<UINT32_C(0x80000000);
}
void ou_ups_monitor_revoke(ou_ups_monitor *m) {
    if(!m)return;
    if(m->initialized || m->link.initialized || m->hid.initialized)
        ou_native_service_source_lost();
    ou_shut_uart_link_revoke(&m->link);
    ou_hid_telemetry_revoke(&m->hid);
    m->initialized=0U;
    m->cycle_active=0U;
    m->report_index=0U;
}
static int fail(ou_ups_monitor *m) {
    if(m) {
        if(m->failures!=UINT32_MAX)++m->failures;
        ou_ups_monitor_revoke(m);
    }
    return -1;
}
int ou_ups_monitor_bind(ou_ups_monitor *m,
                        const ou_ns7520_serial_rx *physical_rx,
                        ou_shut_uart_write8 tx_byte,
                        ou_shut_uart_write32 tx_status,
                        void *tx_context,
                        uint8_t tx_pin_verified,
                        uint8_t baud_verified,
                        uint8_t shut_protocol_verified,
                        uint32_t measured_baud,
                        const uint8_t *hid_descriptor,
                        size_t hid_descriptor_length,
                        const ou_hid_metric_binding bindings[OU_HID_TELEMETRY_METRICS],
                        const ou_hid_source_evidence *source_evidence) {
    uint8_t ids[OU_UPS_MONITOR_MAX_REPORTS]={0};
    uint16_t lengths[OU_UPS_MONITOR_MAX_REPORTS]={0};
    size_t i,j;
    unsigned count=0U;
    if(!m)return -1;
    /* A binding may never silently overwrite an active transaction or
     * authenticated history. Explicit revoke is required on failure. */
    if(m->initialized)return -1;
    *m=(ou_ups_monitor){0};
    if(ou_hid_telemetry_init(&m->hid,hid_descriptor,hid_descriptor_length,
                              bindings,source_evidence)!=0)return -1;
    /* SHUT GET REPORT replies include the HID report ID byte. A no-ID
     * descriptor cannot be matched by the current strict SHUT receiver. */
    if(m->hid.map.has_report_ids!=1U)
        return fail(m);
    for(i=0U;i<OU_HID_TELEMETRY_METRICS;++i) {
        uint8_t id=bindings[i].report_id;
        uint32_t bits,bytes;
        if(id==0U)return fail(m);
        for(j=0U;j<count;++j)if(ids[j]==id)break;
        if(j<count)continue;
        bits=m->hid.map.feature_bits[id];
        bytes=(bits+7U)/8U+1U; /* HID report-id prefix. */
        if(bits==0U||bytes>OU_SHUT_RX_MESSAGE_CAP ||
           bytes>OU_SHUT_GET_REPORT_MAX_LENGTH ||
           count>=OU_UPS_MONITOR_MAX_REPORTS)return fail(m);
        ids[count]=id;
        lengths[count]=(uint16_t)bytes;
        ++count;
    }
    if(count==0U ||
       ou_shut_uart_link_bind(&m->link,physical_rx,tx_byte,tx_status,
                              tx_context,tx_pin_verified,baud_verified,
                              shut_protocol_verified,measured_baud)!=0)
        return fail(m);
    for(i=0U;i<count;++i){
        m->report_id[i]=ids[i];
        m->report_length[i]=lengths[i];
    }
    m->report_count=(uint8_t)count;
    m->initialized=1U;
    return 0;
}
int ou_ups_monitor_step(ou_ups_monitor *m,uint32_t now_ms) {
    ou_shut_rx_message result;
    ou_driver_report report;
    int progress,received;
    if(!m||m->initialized!=1U)return -1;
    if(m->clock_started && !forward_or_equal(now_ms,m->last_ms))
        return fail(m);
    m->clock_started=1U;
    m->last_ms=now_ms;
    if(!m->cycle_active) {
        if(m->published_sets!=0U &&
           (uint32_t)(now_ms-m->last_publish_ms)<OU_UPS_MONITOR_PERIOD_MS)
            return 0;
        /* The previous report must already have been consumed; never
         * reuse an unconsumed result or stale fragments. */
        m->report_index=0U;
        if(ou_shut_uart_link_begin(&m->link,m->report_id[0],
                   m->report_length[0],now_ms)!=0)return fail(m);
        m->cycle_active=1U;
    }
    progress=ou_shut_uart_link_step(&m->link,now_ms);
    if(progress<0)return fail(m);
    received=ou_shut_uart_link_take_report(&m->link,&result);
    if(received<0)return fail(m);
    if(received==0)return progress>0?1:0;
    if(result.packet_type!=OU_SHUT_RX_RESPONSE ||
       result.length!=m->report_length[m->report_index] ||
       result.payload[0]!=m->report_id[m->report_index] ||
       ou_hid_telemetry_ingest(&m->hid,&result,now_ms)!=0)
        return fail(m);
    ++m->report_index;
    if(m->report_index<m->report_count) {
        if(ou_shut_uart_link_begin(&m->link,m->report_id[m->report_index],
                   m->report_length[m->report_index],now_ms)!=0)
            return fail(m);
        return 1;
    }
    /* Never publish an incomplete bundle or partially validated values. */
    if(ou_hid_telemetry_take(&m->hid,now_ms,&report)!=0 ||
       ou_native_service_driver_report(&report,now_ms)!=0)
        return fail(m);
    m->last_publish_ms=now_ms;
    if(m->published_sets!=UINT32_MAX)++m->published_sets;
    m->cycle_active=0U;
    return 1;
}
