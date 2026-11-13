#include "ou_shut_uart_bridge.h"

static int may_receive(const ou_shut_session *s) {
    return s && (s->state==OU_SHUT_SESSION_SYNC_WAIT ||
                 s->state==OU_SHUT_SESSION_REQUEST_WAIT ||
                 s->state==OU_SHUT_SESSION_RESPONSE_WAIT);
}
static void zero_pending(ou_shut_uart_bridge *bridge) {
    unsigned i;
    for(i=0U;i<sizeof(bridge->pending);++i)bridge->pending[i]=0U;
    bridge->pending_offset=0U;
    bridge->pending_count=0U;
}
void ou_shut_uart_bridge_discard(ou_shut_uart_bridge *bridge) {
    if(bridge)zero_pending(bridge);
}
int ou_shut_uart_bridge_init(ou_shut_uart_bridge *bridge,
                              const ou_ns7520_serial_rx *source,
                              ou_shut_session *session) {
    if(!bridge)return -1;
    *bridge=(ou_shut_uart_bridge){0};
    if(!source || !source->read32 || !session ||
       (source->channel!=1U && source->channel!=2U) ||
       source->peripheral_mapping_verified!=1U ||
       source->uart_fifo_mode_verified!=1U ||
       source->receive_byte_order_verified!=1U ||
       source->exclusive_channel_ownership!=1U ||
       source->ups_physical_wiring_verified!=1U)return -1;
    bridge->uart=*source;
    bridge->session=session;
    bridge->initialized=1U;
    return 0;
}
int ou_shut_uart_bridge_poll(ou_shut_uart_bridge *bridge,uint32_t now_ms){
    unsigned consumed=0U;
    if(!bridge || bridge->initialized!=1U || !bridge->session)return -1;
    if(!may_receive(bridge->session))return 0;
    if(bridge->pending_offset==bridge->pending_count){
        size_t n=0U;
        int r;
        zero_pending(bridge);
        r=ou_ns7520_serial_rx_poll(&bridge->uart,bridge->pending,
                                     sizeof(bridge->pending),&n);
        if(r<0 || n>sizeof(bridge->pending) || (r>0 && n!=(size_t)r)){
            if(bridge->rejected_polls!=UINT32_MAX)++bridge->rejected_polls;
            zero_pending(bridge);
            /* A damaged physical transaction cannot be reused as a
             * partially verified report. No outgoing writes on failure. */
            bridge->session->state=OU_SHUT_SESSION_FAILED;
            bridge->session->out_length=0U;
            bridge->session->result=(ou_shut_rx_message){0};
            return -1;
        }
        if(r==0)return 0;
        bridge->pending_count=(uint8_t)n;
    }
    while(bridge->pending_offset<bridge->pending_count &&
          may_receive(bridge->session)){
        uint8_t byte=bridge->pending[bridge->pending_offset++];
        int r=ou_shut_session_feed(bridge->session,byte,now_ms);
        if(r<0){
            if(bridge->rejected_polls!=UINT32_MAX)++bridge->rejected_polls;
            zero_pending(bridge);
            return -1;
        }
        if(bridge->total_bytes!=UINT32_MAX)++bridge->total_bytes;
        ++consumed;
    }
    if(bridge->pending_offset<bridge->pending_count &&
       bridge->deferred_fifo_bytes!=UINT32_MAX)
        ++bridge->deferred_fifo_bytes;
    if(bridge->pending_offset==bridge->pending_count)zero_pending(bridge);
    return (int)consumed;
}
