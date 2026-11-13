#include "ou_shut_uart_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Pure host fake of NS7520 control/status/FIFO hardware. Not evidence of
 * installed H910P wiring, baud rate, boot, or physical UPS operation. */
static struct {
    uint8_t incoming[128],outgoing[128];
    unsigned received,consumed,sent,other_writes,read_fail;
} hw;
static ou_shut_uart_link link;
static uint32_t now_ms;
static unsigned selected_channel;

static int read32(void *ctx,uint32_t addr,uint32_t *out) {
    uint32_t base=selected_channel==1U?OU_NS7520_SER1_STATUS_A:
                                        OU_NS7520_SER2_STATUS_A;
    unsigned n,i;
    (void)ctx;
    if(hw.read_fail)return -1;
    if(addr==base-8U){*out=UINT32_C(0x83000000);return 0;}
    if(addr==base-4U){*out=0U;return 0;}
    if(addr==base){
        *out=9U; /* TRDY and TXEMPTY. TXBC may never occur. */
        if(hw.consumed<hw.received) {
            n=hw.received-hw.consumed;
            if(n>4U)n=4U;
            *out|=OU_NS7520_SER_RX_READY|
                  ((uint32_t)(n==4U?0U:n)<<20U);
        }
        return 0;
    }
    if(addr==base+8U) {
        n=hw.received-hw.consumed;
        if(n>4U)n=4U;
        if(!n)return -1;
        *out=0U;
        for(i=0U;i<n;++i)
            *out|=(uint32_t)hw.incoming[hw.consumed++]<<(24U-i*8U);
        return 0;
    }
    ++hw.other_writes;
    return -1;
}
static int write8(void *ctx,uint32_t addr,uint8_t byte) {
    uint32_t fifo=selected_channel==1U?OU_NS7520_SER1_FIFO:
                                        OU_NS7520_SER2_FIFO;
    (void)ctx;
    if(addr!=fifo || hw.sent>=sizeof(hw.outgoing)){
        ++hw.other_writes;
        return -1;
    }
    hw.outgoing[hw.sent++]=byte;
    return 0;
}
static int write32(void *ctx,uint32_t addr,uint32_t value) {
    /* No TBC on the reference board simulation (documented erratum).
     * The driver must work without any status clearing. */
    (void)ctx;(void)addr;(void)value;
    ++hw.other_writes;return -1;
}
static void setup(unsigned channel) {
    ou_ns7520_serial_rx source={0};
    memset(&hw,0,sizeof(hw));
    now_ms=1000U;
    selected_channel=channel;
    source.read32=read32;
    source.channel=(uint8_t)channel;
    source.peripheral_mapping_verified=1U;
    source.uart_fifo_mode_verified=1U;
    source.receive_byte_order_verified=1U;
    source.exclusive_channel_ownership=1U;
    source.ups_physical_wiring_verified=1U;
    assert(ou_shut_uart_link_bind(&link,&source,write8,write32,0,
                                  1U,1U,1U,2400U)==0);
}
static void inject(const uint8_t *data,size_t len){
    assert(hw.received+len<=sizeof(hw.incoming));
    memcpy(hw.incoming+hw.received,data,len);
    hw.received+=(unsigned)len;
}
static void until(unsigned desired){
    unsigned i;
    for(i=0U;i<700U&&link.session.state!=desired;++i){
        int progressed=ou_shut_uart_link_step(&link,now_ms++);
        assert(progressed>=0);
    }
    assert(link.session.state==desired);
}
static void read_once(void) {
    static const uint8_t sync[]={0x16U},request_ack[]={0x06U};
    static const uint8_t response[]={
        0x84U,0x33U,0x31U,0x64U,0x08U,0x5dU
    };
    ou_shut_rx_message result={0};
    unsigned before=hw.sent;
    assert(ou_shut_uart_link_begin(&link,0x31U,0U,now_ms)==0);
    assert(ou_shut_uart_link_begin(&link,0x31U,0U,now_ms)==-1);
    until(OU_SHUT_SESSION_SYNC_WAIT);
    assert(hw.sent==before+1U&&hw.outgoing[before]==0x16U);
    inject(sync,sizeof(sync));until(OU_SHUT_SESSION_REQUEST_READY);
    until(OU_SHUT_SESSION_REQUEST_WAIT);
    assert(hw.sent==before+12U);
    assert(hw.outgoing[before+1]==0x81U);
    assert(hw.outgoing[before+2]==0x88U);
    assert(hw.outgoing[before+3]==0xa1U);
    assert(hw.outgoing[before+5]==0x31U);
    inject(request_ack,sizeof(request_ack));
    until(OU_SHUT_SESSION_RESPONSE_WAIT);
    inject(response,sizeof(response));
    until(OU_SHUT_SESSION_ACK_READY);
    assert(ou_shut_uart_link_take_report(&link,&result)==0);
    until(OU_SHUT_SESSION_COMPLETE);
    assert(hw.sent==before+13U&&hw.outgoing[before+12U]==0x06U);
    assert(ou_shut_uart_link_take_report(&link,&result)==1);
    assert(result.packet_type==OU_SHUT_RX_RESPONSE);
    assert(result.length==3U&&result.payload[0]==0x31U);
    assert(result.payload[1]==0x64U&&result.payload[2]==0x08U);
    assert(result.telemetry_verified==0U);
    assert(ou_shut_uart_link_take_report(&link,&result)==0);
    assert(hw.consumed==hw.received&&!hw.other_writes);
}
static void normal_and_repeat(void){
    setup(1U);read_once();
    assert(link.reports_completed==1U);
    read_once();
    assert(link.reports_completed==2U);
    assert(link.session.successful_reads==1U);
    /* Session state resets per read; this counter is per transaction. */
    ou_shut_uart_link_revoke(&link);
    assert(!link.initialized);
    assert(ou_shut_uart_link_step(&link,now_ms)==-1);
    setup(2U);read_once();
    assert(!hw.other_writes);
}
static void bad_source_and_fault(void){
    ou_ns7520_serial_rx source={0};
    setup(1U);
    source=link.rx.uart;
    source.ups_physical_wiring_verified=0U;
    assert(ou_shut_uart_link_bind(&link,&source,write8,write32,0,
                                  1U,1U,1U,2400U)==-1);
    assert(ou_shut_uart_link_step(&link,now_ms)==-1);
    setup(2U);
    assert(ou_shut_uart_link_begin(&link,0x31U,0U,now_ms)==0);
    hw.read_fail=1U;
    assert(ou_shut_uart_link_step(&link,now_ms)==-1);
    assert(link.io_failures==1U && !link.initialized);
    assert(link.session.state==OU_SHUT_SESSION_FAILED);
    assert(!link.tx.write8 && !link.rx.uart.read32);
    assert(hw.sent==0U);
}
static void corrupted_response(void) {
    static const uint8_t sync[]={0x16U},ack[]={0x06U};
    static const uint8_t broken[]={0x84U,0x33U,0x31U,0x64U,0x08U,0x00U};
    ou_shut_rx_message r;
    setup(2U);
    assert(ou_shut_uart_link_begin(&link,0x31U,0U,now_ms)==0);
    until(OU_SHUT_SESSION_SYNC_WAIT);
    inject(sync,sizeof(sync));until(OU_SHUT_SESSION_REQUEST_READY);
    until(OU_SHUT_SESSION_REQUEST_WAIT);
    inject(ack,sizeof(ack));until(OU_SHUT_SESSION_RESPONSE_WAIT);
    inject(broken,sizeof(broken));
    {
        int failed=0;unsigned i;
        for(i=0U;i<12U;++i){
            int status=ou_shut_uart_link_step(&link,now_ms++);
            if(status<0){failed=1;break;}
        }
        assert(failed);
    }
    assert(link.session.state==OU_SHUT_SESSION_FAILED);
    assert(!link.initialized);
    assert(ou_shut_uart_link_take_report(&link,&r)==-1);
    assert(!link.session.result.length);
}
int main(void){
    normal_and_repeat();
    bad_source_and_fault();
    corrupted_response();
    puts("PASS: single-owner NS7520 SHUT RX/TX event-loop, GET REPORT/ACK, repeat reads, error revocation");
    return 0;
}
