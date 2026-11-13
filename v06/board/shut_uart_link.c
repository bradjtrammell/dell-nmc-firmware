#include "ou_shut_uart_link.h"

static void stop(ou_shut_uart_link *link) {
    if(!link)return;
    ou_shut_uart_tx_revoke(&link->tx);
    ou_shut_uart_bridge_discard(&link->rx);
    link->rx=(ou_shut_uart_bridge){0};
    link->initialized=0U;
}
void ou_shut_uart_link_revoke(ou_shut_uart_link *link) {
    if(!link)return;
    if(link->session.state!=OU_SHUT_SESSION_IDLE &&
       link->session.state!=OU_SHUT_SESSION_COMPLETE &&
       link->session.state!=OU_SHUT_SESSION_FAILED)
        (void)ou_shut_session_abort(&link->session,OU_SHUT_SESSION_ERR_IO);
    stop(link);
}
static int failure(ou_shut_uart_link *link) {
    if(link) {
        if(link->session.state!=OU_SHUT_SESSION_FAILED)
            (void)ou_shut_session_abort(&link->session,OU_SHUT_SESSION_ERR_IO);
        if(link->io_failures!=UINT32_MAX)++link->io_failures;
        stop(link);
    }
    return -1;
}
int ou_shut_uart_link_bind(ou_shut_uart_link *link,
                           const ou_ns7520_serial_rx *source,
                           ou_shut_uart_write8 write8,
                           ou_shut_uart_write32 write32,
                           void *write_context,
                           uint8_t tx_pin_verified,
                           uint8_t bit_rate_verified,
                           uint8_t shut_protocol_verified,
                           uint32_t measured_baud) {
    if(!link)return -1;
    *link=(ou_shut_uart_link){0};
    ou_shut_session_init(&link->session);
    if(ou_shut_uart_bridge_init(&link->rx,source,
                                &link->session)!=0 ||
       ou_shut_uart_tx_bind(&link->tx,source,&link->session,
                            write8,write32,write_context,
                            tx_pin_verified,bit_rate_verified,
                            shut_protocol_verified,measured_baud)!=0) {
        stop(link);
        return -1;
    }
    link->initialized=1U;
    return 0;
}
int ou_shut_uart_link_begin(ou_shut_uart_link *link,
                            uint8_t report_id,uint16_t length,
                            uint32_t now_ms) {
    if(!link||link->initialized!=1U ||
       link->session.state!=OU_SHUT_SESSION_IDLE ||
       link->rx.pending_count!=0U || link->tx.length!=0U)return -1;
    if(ou_shut_session_begin(&link->session,report_id,length,now_ms,1U,1U)!=1)
        return -1;
    link->counted_complete=0U;
    return 0;
}
int ou_shut_uart_link_step(ou_shut_uart_link *link,uint32_t now_ms) {
    int sent,received;
    if(!link||link->initialized!=1U)return -1;
    if(link->polls!=UINT32_MAX)++link->polls;
    /* Hardware writes use only the qualified read-only SHUT TX
     * command set; received bytes never bypass checksum/ID checks. */
    sent=ou_shut_uart_tx_step(&link->tx,now_ms);
    if(sent<0)return failure(link);
    received=ou_shut_uart_bridge_poll(&link->rx,now_ms);
    if(received<0)return failure(link);
    if(link->session.state==OU_SHUT_SESSION_FAILED)return failure(link);
    if(link->session.state==OU_SHUT_SESSION_COMPLETE &&
       !link->counted_complete) {
        link->counted_complete=1U;
        if(link->reports_completed!=UINT32_MAX)++link->reports_completed;
    }
    return (sent>0||received>0)?1:0;
}
int ou_shut_uart_link_take_report(ou_shut_uart_link *link,
                                  ou_shut_rx_message *out) {
    if(!link||link->initialized!=1U||!out)return -1;
    return ou_shut_session_take_report(&link->session,out);
}
