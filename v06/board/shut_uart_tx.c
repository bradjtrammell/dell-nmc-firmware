#include "ou_shut_uart_tx.h"

#define OU_TX_READY UINT32_C(0x8)
#define OU_TX_BUFFER_COMPLETE UINT32_C(0x2)
#define OU_TX_EMPTY UINT32_C(0x1)
#define OU_UART_CTRL_A_ENABLED_8N1 UINT32_C(0x83000000)
#define OU_UART_CTRL_A_FORBIDDEN UINT32_C(0x4c300101)
#define OU_UART_CTRL_B_FORBIDDEN UINT32_C(0x003801c0)

static int ready(const ou_shut_uart_tx *tx) {
    return tx&&tx->initialized==1U&&tx->session&&tx->write8&&tx->write32 &&
           tx->uart.read32&&(tx->uart.channel==1U||tx->uart.channel==2U)&&
           tx->uart.peripheral_mapping_verified==1U&&
           tx->uart.uart_fifo_mode_verified==1U&&
           tx->uart.receive_byte_order_verified==1U&&
           tx->uart.exclusive_channel_ownership==1U&&
           tx->uart.ups_physical_wiring_verified==1U;
}
static void clear_frame(ou_shut_uart_tx *tx) {
    size_t i;
    for(i=0U;i<sizeof(tx->frame);++i)tx->frame[i]=0U;
    tx->length=0U;
    tx->offset=0U;
    tx->waiting_for_empty=0U;
    tx->last_fifo_write_ms=0U;
    tx->empty_seen_ms=0U;
}
void ou_shut_uart_tx_revoke(ou_shut_uart_tx *tx) {
    if(!tx)return;
    clear_frame(tx);
    tx->uart=(ou_ns7520_serial_rx){0};
    tx->session=0;
    tx->write8=0;
    tx->write32=0;
    tx->context=0;
    tx->initialized=0U;
    tx->character_guard_ms=0U;
}
static int fault(ou_shut_uart_tx *tx) {
    if(!tx)return -1;
    if(tx->session && tx->session->state!=OU_SHUT_SESSION_FAILED)
        (void)ou_shut_session_abort(tx->session,OU_SHUT_SESSION_ERR_IO);
    if(tx->faults!=UINT32_MAX)++tx->faults;
    ou_shut_uart_tx_revoke(tx);
    return -1;
}
int ou_shut_uart_tx_bind(ou_shut_uart_tx *tx,
                         const ou_ns7520_serial_rx *source,
                         ou_shut_session *session,
                         ou_shut_uart_write8 write8,
                         ou_shut_uart_write32 write32,
                         void *context,
                         uint8_t tx_pin_verified,
                         uint8_t bit_rate_verified,
                         uint8_t read_only_shut_protocol_verified,
                         uint32_t measured_baud) {
    if(!tx)return -1;
    *tx=(ou_shut_uart_tx){0};
    if(!source || !session || !write8 || !write32 ||
       tx_pin_verified!=1U || bit_rate_verified!=1U ||
       read_only_shut_protocol_verified!=1U ||
       measured_baud<300U || measured_baud>115200U ||
       !source->read32 ||
       (source->channel!=1U && source->channel!=2U) ||
       source->peripheral_mapping_verified!=1U ||
       source->uart_fifo_mode_verified!=1U ||
       source->receive_byte_order_verified!=1U ||
       source->exclusive_channel_ownership!=1U ||
       source->ups_physical_wiring_verified!=1U)return -1;
    tx->uart=*source;
    tx->session=session;
    tx->write8=write8;
    tx->write32=write32;
    tx->context=context;
    /* 8N1 = 10 bits; 11 bit-times allows a full character plus
     * the inter-character margin specified by Digi's NS7520 errata.
     * Ceiling division; minimum 1ms for a millisecond board timer. */
    tx->character_guard_ms=(uint16_t)((UINT32_C(11000)+
                                measured_baud-1U)/measured_baud);
    tx->initialized=1U;
    return 0;
}
static int allowed(const uint8_t *f,size_t count,const ou_shut_session *s) {
    uint8_t crc=0U;
    size_t i;
    uint16_t length;
    if(!f||!s)return 0;
    if(count==1U)
        return (s->state==OU_SHUT_SESSION_SYNC_READY&&f[0]==0x16U) ||
               (s->state==OU_SHUT_SESSION_ACK_READY&&f[0]==0x06U);
    if(count!=OU_SHUT_GET_REPORT_FRAME_SIZE ||
       s->state!=OU_SHUT_SESSION_REQUEST_READY ||
       f[0]!=0x81U || f[1]!=0x88U ||
       f[2]!=0xa1U || f[3]!=0x01U ||
       f[4]!=s->report_id || f[5]!=0x03U ||
       f[6]!=0U || f[7]!=0U)return 0;
    length=(uint16_t)((uint16_t)f[8]|((uint16_t)f[9]<<8U));
    if(length>OU_SHUT_GET_REPORT_MAX_LENGTH ||
       length!=s->requested_bytes)return 0;
    for(i=2U;i<10U;++i)crc^=f[i];
    return crc==f[10];
}
static int status_read(ou_shut_uart_tx *tx,uint32_t *status) {
    uint32_t ctrl_a=0U,ctrl_b=0U;
    uint32_t base=tx->uart.channel==1U?OU_NS7520_SER1_STATUS_A:
                                            OU_NS7520_SER2_STATUS_A;
    if(ou_ns7520_serial_rx_read_whitelisted(
         &tx->uart,base-UINT32_C(8),&ctrl_a)!=0 ||
       ou_ns7520_serial_rx_read_whitelisted(
         &tx->uart,base-UINT32_C(4),&ctrl_b)!=0 ||
       (ctrl_a&OU_UART_CTRL_A_ENABLED_8N1)!=OU_UART_CTRL_A_ENABLED_8N1 ||
       (ctrl_a&OU_UART_CTRL_A_FORBIDDEN)!=0U ||
       (ctrl_b&OU_UART_CTRL_B_FORBIDDEN)!=0U ||
       ou_ns7520_serial_rx_read_whitelisted(
         &tx->uart,base,status)!=0 ||
       (*status&OU_NS7520_SER_RX_ERRORS)!=0U)return -1;
    return 0;
}
int ou_shut_uart_tx_step(ou_shut_uart_tx *tx,uint32_t now_ms) {
    size_t n=0U;
    int r;
    uint32_t status=0U,base;
    if(!ready(tx))return -1;
    r=ou_shut_session_tick(tx->session,now_ms);
    if(r<0 || tx->session->state==OU_SHUT_SESSION_FAILED)
        return fault(tx);
    if(!tx->length) {
        r=ou_shut_session_peek_tx(tx->session,now_ms,
                                   tx->frame,sizeof(tx->frame),&n);
        if(r<0)return fault(tx);
        if(r==0)return 0;
        if(!allowed(tx->frame,n,tx->session))return fault(tx);
        tx->length=(uint8_t)n;
        tx->offset=0U;
    } else if(tx->session->state!=OU_SHUT_SESSION_SYNC_READY &&
              tx->session->state!=OU_SHUT_SESSION_REQUEST_READY &&
              tx->session->state!=OU_SHUT_SESSION_ACK_READY)
        return fault(tx);
    if(status_read(tx,&status)!=0)return fault(tx);
    base=tx->uart.channel==1U?OU_NS7520_SER1_STATUS_A:
                              OU_NS7520_SER2_STATUS_A;
    if(status&OU_TX_BUFFER_COMPLETE) {
        /* TXBC/TBC is unreliable on NS7520 (Digi Errata Rev L p.14).
         * Clear a stuck W1C flag to unblock TXRDY, but NEVER use it
         * to establish physical completion or transition SHUT state.
         * May occur in the middle of a multi-byte frame. */
        if(tx->write32(tx->context,base,OU_TX_BUFFER_COMPLETE)!=0)
            return fault(tx);
        if(tx->tbc_acks!=UINT32_MAX)++tx->tbc_acks;
        return 0;
    }
    if(tx->offset==tx->length) {
        /* The final byte was merely queued in FIFO. Wait until TXEMPTY
         * is actually observed and then a FULL character guard has
         * elapsed. This protects the shift register still sending. */
        if(!(status&OU_TX_EMPTY)){
            tx->waiting_for_empty=0U;
            if(tx->blocked_polls!=UINT32_MAX)++tx->blocked_polls;
            return 0;
        }
        if(!tx->waiting_for_empty){
            tx->waiting_for_empty=1U;
            tx->empty_seen_ms=now_ms;
            return 0;
        }
        if((uint32_t)(now_ms-tx->empty_seen_ms)<tx->character_guard_ms)
            return 0;
        if(ou_shut_session_confirm_tx(tx->session,now_ms)!=0)
            return fault(tx);
        clear_frame(tx);
        return 0;
    }
    /* TXEMPTY is safer than TXRDY alone: the NS7520 errata warns
     * consecutive byte writes can corrupt the next character's
     * first bit. Keep one byte in flight and wait >= 11 bit-times. */
    if(!(status&OU_TX_READY) || !(status&OU_TX_EMPTY) ||
       (tx->offset!=0U &&
        (uint32_t)(now_ms-tx->last_fifo_write_ms) <
            tx->character_guard_ms)) {
        if(tx->blocked_polls!=UINT32_MAX)++tx->blocked_polls;
        return 0;
    }
    if(tx->offset>=tx->length)return fault(tx);
    if(tx->write8(tx->context,base+UINT32_C(8),
                  tx->frame[tx->offset])!=0)return fault(tx);
    tx->last_fifo_write_ms=now_ms;
    if(tx->sent_bytes!=UINT32_MAX)++tx->sent_bytes;
    ++tx->offset;
    return 1;
}
int ou_ns7520_arm_uart_fifo_write8(void *context,
                                    uint32_t addr,uint8_t byte) {
    (void)context;
#if defined(__arm__) && defined(__ARMEB__)
    if(addr!=OU_NS7520_SER1_FIFO&&addr!=OU_NS7520_SER2_FIFO)return -1;
    *(volatile uint8_t *)(uintptr_t)addr=byte;
    return 0;
#else
    (void)addr;(void)byte;
    return -1;
#endif
}
