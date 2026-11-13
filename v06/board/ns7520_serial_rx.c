#include "ou_ns7520_serial_rx.h"

static int qualified(const ou_ns7520_serial_rx *source) {
    return source && source->read32 &&
           (source->channel==1U || source->channel==2U) &&
           source->peripheral_mapping_verified==1U &&
           source->uart_fifo_mode_verified==1U &&
           source->receive_byte_order_verified==1U &&
           source->exclusive_channel_ownership==1U &&
           source->ups_physical_wiring_verified==1U;
}
int ou_ns7520_serial_rx_read_whitelisted(const ou_ns7520_serial_rx *source,
                                          uint32_t address, uint32_t *value) {
    uint32_t base;
    if(!value)return -1;
    *value=0U;
    if(!qualified(source))return -1;
    base=(source->channel==1U)?OU_NS7520_SER1_STATUS_A:
                                OU_NS7520_SER2_STATUS_A;
    if(address!=base && address!=base+UINT32_C(8))return -1;
    return source->read32(source->context,address,value);
}
int ou_ns7520_serial_rx_poll(const ou_ns7520_serial_rx *source,
                              uint8_t *output,size_t capacity,
                              size_t *written) {
    uint32_t status=0U,data=0U,base;
    unsigned available,i;
    uint8_t stage[4];
    if(!written)return -1;
    *written=0U;
    if(!qualified(source)||!output)return -1;
    base=(source->channel==1U)?OU_NS7520_SER1_STATUS_A:
                                OU_NS7520_SER2_STATUS_A;
    if(ou_ns7520_serial_rx_read_whitelisted(source,base,&status)!=0)
        return -1;
    /* Status A RBRK/RFE/RPE/ROVER bits 15..12. These are sticky error
     * flags: never clear them here (that would be a register write).
     * RBC and RRDY must not both be active per Digi Rev G p.238. */
    if(status & (OU_NS7520_SER_RX_ERRORS|
                   (OU_NS7520_SER_RX_READY&
                    ((status&OU_NS7520_SER_RX_BUFFER_CLOSED)?
                     UINT32_MAX:0U))))return -1;
    if(!(status&OU_NS7520_SER_RX_READY))return 0;
    /* RXFDB bits 21:20: 00=>4, 01=>1, 10=>2, 11=>3 valid bytes. */
    available=(unsigned)((status>>20U)&3U);
    if(available==0U)available=4U;
    /* Before draining FIFO, refuse any short caller buffer: no loss. */
    if(capacity<available)return -1;
    if(ou_ns7520_serial_rx_read_whitelisted(
         source,base+UINT32_C(8),&data)!=0)return -1;
    for(i=0U;i<available;++i)
        stage[i]=(uint8_t)(data>>(24U-8U*i));
    for(i=0U;i<available;++i)output[i]=stage[i];
    *written=available;
    return (int)available;
}
