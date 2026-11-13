#include "ou_ns7520_serial_rx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned calls;
static uint32_t status_data,fifo_data;
static uint32_t selected_status,selected_fifo;
static uint32_t ctrl_a=UINT32_C(0x83000000),ctrl_b=0U;
static int fail_status,fail_fifo;
static int rd(void *context,uint32_t address,uint32_t *value){
    (void)context;
    ++calls;
    if(address==selected_status-8U){*value=ctrl_a;return 0;}
    if(address==selected_status-4U){*value=ctrl_b;return 0;}
    if(address==selected_status){
        if(fail_status)return -1;
        *value=status_data;return 0;
    }
    if(address==selected_fifo){
        if(fail_fifo)return -1;
        *value=fifo_data;return 0;
    }
    assert(!"unexpected register read");
    return -1;
}
static ou_ns7520_serial_rx qualified(unsigned channel){
    ou_ns7520_serial_rx s={0};
    s.read32=rd;s.channel=(uint8_t)channel;
    s.peripheral_mapping_verified=1U;
    s.uart_fifo_mode_verified=1U;
    s.receive_byte_order_verified=1U;
    s.exclusive_channel_ownership=1U;
    s.ups_physical_wiring_verified=1U;
    selected_status=channel==1U?OU_NS7520_SER1_STATUS_A:
                                 OU_NS7520_SER2_STATUS_A;
    selected_fifo=selected_status+8U;
    return s;
}
static void fields(unsigned channel){
    ou_ns7520_serial_rx s=qualified(channel);
    uint8_t out[8];
    uint32_t value;
    size_t written;
    unsigned count;
    for(count=1U;count<=4U;++count){
        memset(out,0xa5,sizeof(out));calls=0U;
        status_data=OU_NS7520_SER_RX_READY|
                    ((count==4U?0U:count)<<20U);
        fifo_data=UINT32_C(0x12345678);
        assert(ou_ns7520_serial_rx_poll(&s,out,sizeof(out),&written)==(int)count);
        assert(written==count&&calls==4U);
        assert(out[0]==0x12U);
        if(count>1U)assert(out[1]==0x34U);
        if(count>2U)assert(out[2]==0x56U);
        if(count>3U)assert(out[3]==0x78U);
        assert(out[count]==0xa5U);
    }
    calls=0U;status_data=0U;
    assert(ou_ns7520_serial_rx_poll(&s,out,sizeof(out),&written)==0);
    assert(written==0U&&calls==3U);
    assert(ou_ns7520_serial_rx_read_whitelisted(&s,selected_fifo,&value)==0);
    assert(value==fifo_data);
    assert(ou_ns7520_serial_rx_read_whitelisted(&s,
              selected_status-8U,&value)==0&&value==ctrl_a);
    assert(ou_ns7520_serial_rx_read_whitelisted(&s,
              selected_status-4U,&value)==0&&value==ctrl_b);
    assert(ou_ns7520_serial_rx_read_whitelisted(&s,
              selected_status+4U,&value)==-1&&value==0U);
    assert(ou_ns7520_serial_rx_read_whitelisted(&s,
              channel==1U?OU_NS7520_SER2_FIFO:OU_NS7520_SER1_FIFO,
              &value)==-1&&value==0U);
}
static void errors(void){
    ou_ns7520_serial_rx s=qualified(1U);
    uint8_t out[8];size_t written;
    unsigned bit;
    for(bit=12U;bit<=15U;++bit){
        calls=0U;memset(out,0xa5,sizeof(out));
        status_data=OU_NS7520_SER_RX_READY|(1U<<bit);
        assert(ou_ns7520_serial_rx_poll(&s,out,sizeof(out),&written)==-1);
        assert(!written&&calls==3U&&out[0]==0xa5U);
    }
    calls=0U;status_data=OU_NS7520_SER_RX_READY|
                          OU_NS7520_SER_RX_BUFFER_CLOSED;
    assert(ou_ns7520_serial_rx_poll(&s,out,sizeof(out),&written)==-1);
    assert(calls==3U);
    calls=0U;status_data=OU_NS7520_SER_RX_READY;
    assert(ou_ns7520_serial_rx_poll(&s,out,3U,&written)==-1);
    assert(!written&&calls==3U);
    fail_status=1;calls=0;
    assert(ou_ns7520_serial_rx_poll(&s,out,sizeof(out),&written)==-1);
    assert(!written&&calls==3U);fail_status=0;
    fail_fifo=1;calls=0;
    assert(ou_ns7520_serial_rx_poll(&s,out,sizeof(out),&written)==-1);
    assert(!written&&calls==4U);fail_fifo=0;
}
static void hardware_mode_rejection(void){
    ou_ns7520_serial_rx s=qualified(1U);
    uint8_t out[4]={0xa5U};size_t n=99U;
    static const uint32_t invalid_a[]={
        0U,UINT32_C(0x03000000),UINT32_C(0xc3000000),
        UINT32_C(0x8b000000),UINT32_C(0x87000000),
        UINT32_C(0x82000000),UINT32_C(0x83200000),
        UINT32_C(0x83100000),UINT32_C(0x83000100)
    };
    static const uint32_t invalid_b[]={
        UINT32_C(0x00100000),UINT32_C(0x00200000),
        UINT32_C(0x00080000),UINT32_C(0x00000040)
    };
    unsigned i;
    status_data=OU_NS7520_SER_RX_READY;fifo_data=0x12345678U;
    for(i=0U;i<sizeof(invalid_a)/sizeof(invalid_a[0]);++i){
        ctrl_a=invalid_a[i];calls=0U;
        assert(ou_ns7520_serial_rx_poll(&s,out,sizeof(out),&n)==-1);
        assert(!n&&calls==2U&&out[0]==0xa5U);
    }
    ctrl_a=UINT32_C(0x83000000);
    for(i=0U;i<sizeof(invalid_b)/sizeof(invalid_b[0]);++i){
        ctrl_b=invalid_b[i];calls=0U;
        assert(ou_ns7520_serial_rx_poll(&s,out,sizeof(out),&n)==-1);
        assert(!n&&calls==2U&&out[0]==0xa5U);
    }
    ctrl_b=0U;
}
static void qualifications(void){
    ou_ns7520_serial_rx s=qualified(1U);
    uint8_t out[8];size_t n;
    calls=0U;
    s.ups_physical_wiring_verified=0U;
    assert(ou_ns7520_serial_rx_poll(&s,out,sizeof(out),&n)==-1&&!calls);
    s.ups_physical_wiring_verified=1U;s.uart_fifo_mode_verified=2U;
    assert(ou_ns7520_serial_rx_poll(&s,out,sizeof(out),&n)==-1&&!calls);
    s.uart_fifo_mode_verified=1U;s.exclusive_channel_ownership=0U;
    assert(ou_ns7520_serial_rx_poll(&s,out,sizeof(out),&n)==-1&&!calls);
    s.exclusive_channel_ownership=1U;s.channel=0U;
    assert(ou_ns7520_serial_rx_poll(&s,out,sizeof(out),&n)==-1&&!calls);
    s.channel=3U;
    assert(ou_ns7520_serial_rx_poll(&s,out,sizeof(out),&n)==-1&&!calls);
    assert(ou_ns7520_serial_rx_poll(0,out,sizeof(out),&n)==-1&&!calls);
    assert(ou_ns7520_serial_rx_poll(&s,out,sizeof(out),0)==-1&&!calls);
}
int main(void){
    fields(1U);fields(2U);errors();hardware_mode_rejection();
    qualifications();
    puts("PASS: NS7520 serial UART FIFO receive-only MMIO gates, byte counts, errors and no-drain backpressure");
    return 0;
}
