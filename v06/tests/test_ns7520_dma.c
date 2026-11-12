#include "ou_ns7520_dma.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t addr[128], val[128];
    unsigned writes, reads, syncs, fail_write, fail_read, fail_sync;
    uint32_t ctrl_rx,ctrl_tx;
} mock;
static int read32(void *opaque,uint32_t addr,uint32_t *out) {
    mock *m=opaque; unsigned i=m->reads++;
    if (i==m->fail_read || !out) return -1;
    if (addr==OU_DMA_RX_CTRL) *out=m->ctrl_rx;
    else if (addr==OU_DMA_TX_CTRL) *out=m->ctrl_tx;
    else return -1;
    return 0;
}
static int write32(void *opaque,uint32_t addr,uint32_t val) {
    mock *m=opaque; unsigned i=m->writes++;
    if (i==m->fail_write || i>=128) return -1;
    if (addr!=OU_DMA_RX_BDP && addr!=OU_DMA_RX_CTRL &&
        addr!=OU_DMA_TX_BDP && addr!=OU_DMA_TX_CTRL) return -1;
    m->addr[i]=addr;m->val[i]=val;
    if(addr==OU_DMA_RX_CTRL)m->ctrl_rx=val;
    if(addr==OU_DMA_TX_CTRL)m->ctrl_tx=val;
    return 0;
}
static int sync(void *opaque,uint32_t addr,size_t len,uint8_t to_device) {
    mock *m=opaque;unsigned i=m->syncs++;
    (void)to_device;
    assert(addr!=0U&&len!=0U);
    return i==m->fail_sync?-1:0;
}
static void init(mock *m) {
    memset(m,0,sizeof(*m));
    m->fail_read=m->fail_write=m->fail_sync=9999U;
}
static void check_rejections(void) {
    ou_dma_queue q;
    ou_dma_bd bd;
    uint8_t data[OU_DMA_MAX_FRAME];
    assert(ou_dma_prepare(&q,&bd,0x08008000U,data,0x08010000U,
                          1518U,0U)==0);
    assert((bd.address_flags & OU_DMA_BD_WRAP)!=0U);
    assert(bd.status_length==1518U);
    assert(ou_dma_prepare(&q,&bd,0x08008001U,data,0x08010000U,
                          1518U,0U)==-1);
    assert(ou_dma_prepare(&q,&bd,0x08008000U,data,0x08010002U,
                          1518U,0U)==-1);
    assert(ou_dma_prepare(&q,&bd,0x08008000U,data,0x08008000U,
                          1518U,0U)==-1);
    assert(ou_dma_prepare(&q,&bd,0x08008000U,data,0x08010000U,
                          1518U,2U)==-1);
    assert(ou_dma_prepare(&q,&bd,0x08008000U,data,0x08010000U,
                          1U,0U)==-1);
    assert(ou_dma_prepare(&q,&bd,0x08008000U,data,0x08010000U,
                          1518U,1U)==0);
    assert((bd.address_flags & OU_DMA_BD_LAST)!=0U);
    assert(bd.status_length==0U);
}
static void test_rx(void) {
    mock m;ou_dma_queue q;ou_dma_bd bd;
    uint8_t data[OU_DMA_MAX_FRAME]={0};
    ou_dma_bus bus={read32,write32,sync,&m,1U,1U,1U};
    const uint8_t *frame;size_t n;
    init(&m);
    assert(ou_dma_prepare(&q,&bd,0x08008000U,data,0x08010000U,
                          1518U,0U)==0);
    assert(ou_dma_start(&bus,&q)==0);
    assert(m.writes==3U&&m.syncs==1U);
    assert(m.addr[0]==OU_DMA_RX_BDP&&m.val[0]==0x08008000U);
    assert(m.addr[1]==OU_DMA_RX_CTRL&&m.val[1]==0U);
    assert(m.addr[2]==OU_DMA_RX_CTRL&&m.val[2]==OU_DMA_CONTROL_ENABLE);
    assert(ou_dma_rx_poll(&bus,&q,&frame,&n)==0);
    bd.status_length=((uint32_t)OU_DMA_RX_OK<<16)|OU_DMA_BD_FULL|64U;
    assert(ou_dma_rx_poll(&bus,&q,&frame,&n)==1);
    assert(frame==data&&n==64U);
    assert(ou_dma_rx_poll(&bus,&q,&frame,&n)==1);
    assert(ou_dma_rx_release(&bus,&q)==0);
    assert(bd.status_length==1518U);
    assert(ou_dma_rx_release(&bus,&q)==-1);
    bd.status_length=((uint32_t)OU_DMA_RX_OK<<16)|OU_DMA_BD_FULL|1520U;
    assert(ou_dma_rx_poll(&bus,&q,&frame,&n)==-1&&q.faulted);
    assert(ou_dma_stop(&bus,&q)==0&&!q.active);
}
static void test_tx(void) {
    mock m;ou_dma_queue q;ou_dma_bd bd;
    uint8_t data[OU_DMA_MAX_FRAME]={0},frame[64];
    ou_dma_bus bus={read32,write32,sync,&m,1U,1U,1U};
    unsigned i;
    init(&m);
    for(i=0;i<64;i++)frame[i]=(uint8_t)i;
    assert(ou_dma_prepare(&q,&bd,0x08008400U,data,0x08011000U,
                          1518U,1U)==0);
    assert(ou_dma_start(&bus,&q)==-1);
    assert(ou_dma_tx_copy(&q,frame,64U)==0);
    assert(memcmp(frame,data,64U)==0);
    assert((bd.status_length & OU_DMA_BD_FULL)!=0U);
    assert(ou_dma_start(&bus,&q)==0);
    assert(m.writes==3U&&m.syncs==2U);
    assert(m.addr[0]==OU_DMA_TX_BDP&&m.val[0]==0x08008400U);
    assert(m.addr[1]==OU_DMA_TX_CTRL&&m.val[1]==OU_DMA_CONTROL_TX_MODE);
    assert(m.addr[2]==OU_DMA_TX_CTRL&&m.val[2]==(OU_DMA_CONTROL_TX_MODE|OU_DMA_CONTROL_ENABLE));
    assert(ou_dma_tx_copy(&q,frame,64U)==-1);
    assert(ou_dma_tx_poll(&bus,&q)==0);
    bd.status_length=((uint32_t)OU_DMA_TX_OK<<16)|64U;
    assert(ou_dma_tx_poll(&bus,&q)==1);
    assert(!q.active&&!q.data_pending);
    assert(ou_dma_tx_copy(&q,frame,60U)==0);
    assert(ou_dma_start(&bus,&q)==0);
    bd.status_length=((uint32_t)0x1000U<<16)|60U;
    assert(ou_dma_tx_poll(&bus,&q)==-1);
    assert(!q.active);
}
static void test_fail_closed(void) {
    mock m;ou_dma_queue q;ou_dma_bd bd;
    uint8_t data[OU_DMA_MAX_FRAME]={0},frame[60]={0};
    ou_dma_bus bus={read32,write32,sync,&m,0U,1U,1U};
    unsigned i;
    init(&m);
    assert(ou_dma_prepare(&q,&bd,0x08008000U,data,0x08010000U,1518U,0U)==0);
    assert(ou_dma_start(&bus,&q)==-1&&m.writes==0);
    bus.exclusive_mac_ownership=1U;bus.mac_phy_clocks_verified=0U;
    assert(ou_dma_start(&bus,&q)==-1&&m.writes==0);
    bus.mac_phy_clocks_verified=1U;
    for(i=0U;i<3U;i++) {
        init(&m);m.fail_write=i;
        assert(ou_dma_prepare(&q,&bd,0x08008000U,data,0x08010000U,1518U,0U)==0);
        assert(ou_dma_start(&bus,&q)==-1);
        assert(q.faulted);
    }
    init(&m);m.fail_read=0U;
    assert(ou_dma_prepare(&q,&bd,0x08008000U,data,0x08010000U,1518U,0U)==0);
    assert(ou_dma_start(&bus,&q)==-1&&!m.writes);
    init(&m);m.fail_sync=0U;
    assert(ou_dma_prepare(&q,&bd,0x08008000U,data,0x08010000U,1518U,0U)==0);
    assert(ou_dma_start(&bus,&q)==-1&&!m.writes);
    init(&m);
    assert(ou_dma_prepare(&q,&bd,0x08008400U,data,0x08011000U,1518U,1U)==0);
    assert(ou_dma_tx_copy(&q,frame,60U)==0);
    m.fail_sync=0;
    assert(ou_dma_start(&bus,&q)==-1&&!m.writes);
}
int main(void) {
    check_rejections(); test_rx();test_tx();test_fail_closed();
    puts("PASS: NS7520 RX1A/TX2 DMA descriptors, transaction ordering, ownership, completion and faults");
    return 0;
}
