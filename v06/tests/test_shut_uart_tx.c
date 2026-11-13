#include "ou_shut_uart_tx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static ou_shut_session session;
static ou_shut_uart_tx tx;
static uint8_t written[64];
static unsigned write_count,ack_count,forbidden;
static uint32_t status,control_a,control_b,now;
static unsigned channel,fail_after;

static int rd(void *context,uint32_t addr,uint32_t *out) {
    uint32_t base=channel==1U?OU_NS7520_SER1_STATUS_A:
                               OU_NS7520_SER2_STATUS_A;
    (void)context;
    if(addr==base-8U){*out=control_a;return 0;}
    if(addr==base-4U){*out=control_b;return 0;}
    if(addr==base){*out=status;return 0;}
    ++forbidden;return -1;
}
static int wr8(void *context,uint32_t addr,uint8_t v) {
    uint32_t fifo=channel==1U?OU_NS7520_SER1_FIFO:
                               OU_NS7520_SER2_FIFO;
    (void)context;
    if(addr!=fifo){++forbidden;return -1;}
    if(fail_after && write_count==fail_after)return -1;
    assert(write_count<sizeof(written));
    written[write_count++]=v;
    return 0;
}
static int wr32(void *context,uint32_t addr,uint32_t v) {
    uint32_t base=channel==1U?OU_NS7520_SER1_STATUS_A:
                               OU_NS7520_SER2_STATUS_A;
    (void)context;
    if(addr!=base||v!=2U||!(status&2U)){++forbidden;return -1;}
    ++ack_count;
    status&=~2U;
    status|=8U;
    return 0;
}
static ou_ns7520_serial_rx source(unsigned ch) {
    ou_ns7520_serial_rx s={0};
    channel=ch;
    s.channel=(uint8_t)ch;
    s.read32=rd;
    s.peripheral_mapping_verified=1U;
    s.uart_fifo_mode_verified=1U;
    s.receive_byte_order_verified=1U;
    s.exclusive_channel_ownership=1U;
    s.ups_physical_wiring_verified=1U;
    return s;
}
static void reset(unsigned ch) {
    ou_ns7520_serial_rx s;
    ou_shut_session_init(&session);
    ou_shut_uart_tx_revoke(&tx);
    memset(written,0,sizeof(written));
    write_count=ack_count=forbidden=fail_after=0U;
    control_a=UINT32_C(0x83000000);
    control_b=0U;
    status=8U;
    now=1000U;
    s=source(ch);
    assert(ou_shut_uart_tx_bind(&tx,&s,&session,wr8,wr32,0,1U,1U,1U)==0);
}
static void begin(unsigned id) {
    assert(ou_shut_session_begin(&session,(uint8_t)id,0U,now,1U,1U)==1);
}
static void normal(unsigned ch) {
    unsigned i;
    const uint8_t report[]={0x31U,0x64U,0x08U};
    uint8_t expected[11]={0};
    size_t n=0U;
    ou_shut_rx_message result;
    reset(ch);
    begin(0x31U);
    assert(ou_shut_uart_tx_step(&tx,now)==1);
    assert(write_count==1U&&written[0]==0x16U);
    assert(session.state==OU_SHUT_SESSION_SYNC_WAIT);
    assert(ou_shut_session_feed(&session,0x16U,++now)==1);
    assert(session.state==OU_SHUT_SESSION_REQUEST_READY);
    assert(ou_shut_session_peek_tx(&session,now,expected,sizeof(expected),&n)==1);
    assert(n==11U);
    /* Prior frame completion blocks transmit until the documented
     * Status A TBC W1C acknowledgement is performed. */
    status=2U;
    assert(ou_shut_uart_tx_step(&tx,now)==0);
    assert(ack_count==1U && write_count==1U);
    status=0U;
    assert(ou_shut_uart_tx_step(&tx,now)==0);
    assert(session.state==OU_SHUT_SESSION_REQUEST_READY);
    status=8U;
    for(i=0U;i<10U;++i) {
        assert(ou_shut_uart_tx_step(&tx,++now)==1);
        assert(session.state==OU_SHUT_SESSION_REQUEST_READY);
    }
    assert(ou_shut_uart_tx_step(&tx,++now)==1);
    assert(session.state==OU_SHUT_SESSION_REQUEST_WAIT);
    assert(session.request_attempts==1U);
    assert(write_count==12U && !memcmp(written+1,expected,11U));
    assert(ou_shut_session_feed(&session,0x06U,++now)==0);
    assert(session.state==OU_SHUT_SESSION_RESPONSE_WAIT);
    /* Exact checksum-valid single-packet SHUT GET REPORT reply. */
    assert(ou_shut_session_feed(&session,0x84U,++now)==0);
    assert(ou_shut_session_feed(&session,0x33U,++now)==0);
    for(i=0U;i<3U;++i)
        assert(ou_shut_session_feed(&session,report[i],++now)==0);
    assert(ou_shut_session_feed(&session,0x31U^0x64U^0x08U,++now)==1);
    assert(session.state==OU_SHUT_SESSION_ACK_READY);
    assert(ou_shut_uart_tx_step(&tx,++now)==1);
    assert(session.state==OU_SHUT_SESSION_COMPLETE);
    assert(write_count==13U&&written[12]==0x06U);
    assert(ou_shut_session_take_report(&session,&result)==1);
    assert(result.length==3U&&result.payload[0]==0x31U);
    assert(!result.telemetry_verified && !forbidden);
    assert(tx.sent_bytes==13U);
    assert(tx.tbc_acks==1U);
}
static void deny_unqualified(void) {
    ou_ns7520_serial_rx s=source(1U);
    uint8_t saved[64];size_t bytes;
    reset(1U);
    memcpy(saved,written,sizeof(saved));
    bytes=write_count;
    assert(ou_shut_uart_tx_bind(&tx,&s,&session,wr8,wr32,0,0U,1U,1U)==-1);
    assert(ou_shut_uart_tx_step(&tx,now)==-1);
    assert(write_count==bytes&&!memcmp(written,saved,sizeof(saved)));
    assert(ou_shut_uart_tx_bind(&tx,&s,&session,wr8,wr32,0,1U,0U,1U)==-1);
    assert(ou_shut_uart_tx_bind(&tx,&s,&session,wr8,wr32,0,1U,1U,0U)==-1);
    s.ups_physical_wiring_verified=0U;
    assert(ou_shut_uart_tx_bind(&tx,&s,&session,wr8,wr32,0,1U,1U,1U)==-1);
}
static void no_ready_timeout(void) {
    reset(2U);
    begin(0x31U);
    status=0U;
    assert(ou_shut_uart_tx_step(&tx,now)==0);
    assert(session.state==OU_SHUT_SESSION_SYNC_READY&&!write_count);
    now+=OU_SHUT_SESSION_ACK_TIMEOUT_MS+1U;
    assert(ou_shut_uart_tx_step(&tx,now)==-1);
    assert(session.state==OU_SHUT_SESSION_FAILED&&!tx.initialized);
    assert(tx.blocked_polls==1U&&!write_count);
}
static void partial_write_failure(void) {
    unsigned i;
    reset(1U);begin(0x31U);
    assert(ou_shut_uart_tx_step(&tx,now)==1);
    assert(ou_shut_session_feed(&session,0x16U,++now)==1);
    fail_after=4U; /* Sync + 3 report bytes reach FIFO. */
    for(i=0U;i<3U;++i)assert(ou_shut_uart_tx_step(&tx,++now)==1);
    assert(session.state==OU_SHUT_SESSION_REQUEST_READY);
    assert(ou_shut_uart_tx_step(&tx,++now)==-1);
    assert(session.state==OU_SHUT_SESSION_FAILED);
    assert(session.error==OU_SHUT_SESSION_ERR_IO&&!session.out_length);
    assert(!tx.initialized&&!tx.write8&&!tx.uart.read32);
    assert(ou_shut_uart_tx_step(&tx,++now)==-1);
    assert(write_count==4U&&!forbidden);
}
static void deny_mutating_and_bad_mode(void) {
    reset(1U);begin(0x31U);
    session.out[0]=0x82U; /* attempted non-read command */
    assert(ou_shut_uart_tx_step(&tx,now)==-1);
    assert(session.state==OU_SHUT_SESSION_FAILED&&!write_count);
    reset(2U);begin(0x31U);
    control_a|=1U; /* ETXDMA enabled, cannot compete for TX FIFO */
    assert(ou_shut_uart_tx_step(&tx,now)==-1);
    assert(!write_count&&!tx.initialized);
    reset(2U);begin(0x31U);
    control_b=UINT32_C(0x00100000); /* SPI mode */
    assert(ou_shut_uart_tx_step(&tx,now)==-1);
    assert(!write_count&&!tx.initialized);
    reset(1U);begin(0x31U);
    status=OU_NS7520_SER_RX_ERRORS|8U;
    assert(ou_shut_uart_tx_step(&tx,now)==-1);
    assert(!write_count);
}
int main(void) {
    normal(1U);normal(2U);
    deny_unqualified();no_ready_timeout();
    partial_write_failure();deny_mutating_and_bad_mode();
    puts("PASS: NS7520 UART read-only SHUT TX; byte-width writes, TBC, TRDY, 2-phase replies, faults and no power ops");
    return 0;
}
