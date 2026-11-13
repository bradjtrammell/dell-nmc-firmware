#include "ou_shut_uart_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { uint8_t bytes[128];size_t length,position;
                 unsigned reads,forbidden;int fail_fifo; } fake_fifo;
static fake_fifo wire;
static ou_shut_session session;
static ou_shut_uart_bridge bridge;
static uint32_t now=100U;

static int mmio(void *ctx,uint32_t addr,uint32_t *value){
    fake_fifo *w=ctx;
    size_t count;
    if(addr==OU_NS7520_SER1_STATUS_A){
        ++w->reads;
        if(w->position==w->length){*value=0U;return 0;}
        count=w->length-w->position;if(count>4U)count=4U;
        *value=OU_NS7520_SER_RX_READY |
               ((uint32_t)(count==4U?0U:count)<<20U);
        return 0;
    }
    if(addr==OU_NS7520_SER1_FIFO){
        size_t i;
        if(w->fail_fifo)return -1;
        count=w->length-w->position;if(count>4U)count=4U;
        *value=0U;
        for(i=0U;i<count;++i)
            *value|=(uint32_t)w->bytes[w->position++]<<(24U-8U*i);
        return 0;
    }
    ++w->forbidden;return -1;
}
static void put(const uint8_t *data,size_t count){
    assert(wire.length+count<=sizeof(wire.bytes));
    memcpy(wire.bytes+wire.length,data,count);wire.length+=count;
}
static void reset(void){
    ou_ns7520_serial_rx src={0};
    memset(&wire,0,sizeof(wire));
    ou_shut_session_init(&session);now=100U;
    src.read32=mmio;src.context=&wire;src.channel=1U;
    src.peripheral_mapping_verified=1U;src.uart_fifo_mode_verified=1U;
    src.receive_byte_order_verified=1U;src.exclusive_channel_ownership=1U;
    src.ups_physical_wiring_verified=1U;
    assert(ou_shut_uart_bridge_init(&bridge,&src,&session)==0);
}
static void send_sync_and_request(void){
    uint8_t tx[16];size_t n=0U;
    static const uint8_t sync[]={0x16U},ack[]={0x06U};
    assert(ou_shut_session_begin(&session,0x31U,0U,now,1U,1U)==1);
    assert(ou_shut_session_take_tx(&session,now,tx,sizeof(tx),&n)==1);
    assert(n==1U&&tx[0]==0x16U);
    put(sync,sizeof(sync));
    assert(ou_shut_uart_bridge_poll(&bridge,++now)==1);
    assert(session.state==OU_SHUT_SESSION_REQUEST_READY);
    assert(ou_shut_session_take_tx(&session,now,tx,sizeof(tx),&n)==1);
    assert(n==11U&&tx[0]==0x81U&&tx[4]==0x31U);
    put(ack,sizeof(ack));
    assert(ou_shut_uart_bridge_poll(&bridge,++now)==1);
    assert(session.state==OU_SHUT_SESSION_RESPONSE_WAIT);
}
static void fragments_no_loss(void){
    static const uint8_t fragments[]={
        0x04U,0x33U,0x31U,0x01U,0x02U,0x32U,
        0x84U,0x22U,0x03U,0x04U,0x07U
    };
    uint8_t tx[16];size_t n=0U;
    ou_shut_rx_message out;
    reset();send_sync_and_request();
    put(fragments,sizeof(fragments));
    assert(ou_shut_uart_bridge_poll(&bridge,++now)==4);
    assert(ou_shut_uart_bridge_poll(&bridge,++now)==2);
    assert(session.state==OU_SHUT_SESSION_ACK_READY);
    assert(bridge.pending_count==4U&&bridge.pending_offset==2U);
    assert(bridge.total_bytes==8U);
    assert(ou_shut_uart_bridge_poll(&bridge,++now)==0);
    assert(ou_shut_session_take_tx(&session,now,tx,sizeof(tx),&n)==1);
    assert(n==1U&&tx[0]==0x06U);
    assert(session.state==OU_SHUT_SESSION_RESPONSE_WAIT);
    assert(ou_shut_uart_bridge_poll(&bridge,++now)==2);
    assert(ou_shut_uart_bridge_poll(&bridge,++now)==3);
    assert(session.state==OU_SHUT_SESSION_ACK_READY);
    assert(ou_shut_session_take_report(&session,&out)==0);
    assert(ou_shut_session_take_tx(&session,now,tx,sizeof(tx),&n)==1);
    assert(n==1U&&tx[0]==0x06U);
    assert(ou_shut_session_take_report(&session,&out)==1);
    assert(out.packet_type==OU_SHUT_RX_RESPONSE&&out.length==5U);
    assert(out.payload[0]==0x31U&&out.payload[4]==0x04U);
    assert(out.telemetry_verified==0U);
    assert(bridge.total_bytes==sizeof(fragments)+2U);
    assert(bridge.deferred_fifo_bytes>=1U);
    assert(wire.position==wire.length&&!wire.forbidden);
}
static void status_and_link_failures(void){
    static const uint8_t sync[]={0x16U};
    ou_ns7520_serial_rx src;
    reset();send_sync_and_request();
    src=bridge.uart;src.ups_physical_wiring_verified=0U;
    assert(ou_shut_uart_bridge_init(&bridge,&src,&session)==-1);
    assert(bridge.initialized==0U);
    assert(ou_shut_uart_bridge_poll(&bridge,++now)==-1);
    reset();send_sync_and_request();put(sync,sizeof(sync));
    wire.fail_fifo=1;
    assert(ou_shut_uart_bridge_poll(&bridge,++now)==-1);
    assert(session.state==OU_SHUT_SESSION_FAILED);
    assert(session.error==OU_SHUT_SESSION_ERR_IO);
    assert(session.failures==1U && !session.rx.active);
    assert(!session.result.length && !bridge.pending_count);
    assert(!bridge.initialized && !bridge.uart.read32);
    assert(ou_shut_uart_bridge_poll(&bridge,++now)==-1);
    assert(!wire.forbidden);
}
int main(void){
    fragments_no_loss();status_and_link_failures();
    puts("PASS: verified NS7520 UART FIFO -> SHUT session fragment/ACK bridge, deferred bytes and fail-close");
    return 0;
}
