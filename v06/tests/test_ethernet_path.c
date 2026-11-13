#include "ou_ethernet_path.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const ou_net_identity id={{2,3,4,5,6,7},{192,168,10,2},1};
typedef struct {uint32_t rx_ctrl,tx_ctrl;unsigned writes,syncs;} mock;
static int rd(void *p,uint32_t a,uint32_t *o){mock*m=p;if(a==OU_DMA_RX_CTRL)*o=m->rx_ctrl;else if(a==OU_DMA_TX_CTRL)*o=m->tx_ctrl;else return -1;return 0;}
static int wr(void *p,uint32_t a,uint32_t v){mock*m=p;++m->writes;if(a==OU_DMA_RX_CTRL)m->rx_ctrl=v;else if(a==OU_DMA_TX_CTRL)m->tx_ctrl=v;else if(a!=OU_DMA_RX_BDP&&a!=OU_DMA_TX_BDP)return -1;return 0;}
static int sy(void *p,uint32_t a,size_t n,uint8_t dir){mock*m=p;(void)a;(void)n;(void)dir;++m->syncs;return 0;}
static void arp(uint8_t *p) {
    static const uint8_t sender[]={2,4,6,8,10,12};
    memset(p,0,60);memset(p,0xff,6);memcpy(p+6,sender,6);
    p[12]=0x08;p[13]=0x06;p[15]=1;p[16]=0x08;p[19]=6;p[20]=0;
    p[18]=6;p[19]=4;p[21]=1;
    memcpy(p+22,sender,6);
    p[28]=192;p[29]=168;p[30]=10;p[31]=1;
    memcpy(p+38,id.ipv4,4);
}
static void test_path(void) {
    ou_ethernet_path path;
    ou_dma_bd rbd,tbd;
    uint8_t rx[1518],tx[1518];
    mock m={0};ou_dma_bus bus={rd,wr,sy,&m,1,1,1};
    assert(ou_ethernet_path_init(&path,&id,&rbd,0x08008000,rx,0x08010000,
                                 &tbd,0x08008400,tx,0x08011000)==0);
    assert(ou_ethernet_path_start(&path,&bus)==0);
    assert(ou_ethernet_path_poll(&path,&bus)==0);
    arp(rx);
    rbd.status_length=((uint32_t)OU_DMA_RX_OK<<16)|OU_DMA_BD_FULL|60U;
    assert(ou_ethernet_path_poll(&path,&bus)==1);
    assert(path.received==1U&&path.answered==1U);
    assert(path.tx.active);
    assert(tx[12]==8&&tx[13]==6&&tx[21]==2);
    /* An RX frame that arrives while TX is active must remain pending;
     * previously the poller consumed and discarded it, losing TCP ACKs. */
    arp(rx);
    rbd.status_length=((uint32_t)OU_DMA_RX_OK<<16)|OU_DMA_BD_FULL|60U;
    assert(ou_ethernet_path_poll(&path,&bus)==0);
    assert(path.received==1U&&path.answered==1U&&path.ignored==0U);
    assert((rbd.status_length&OU_DMA_BD_FULL)!=0U);
    assert(path.tx.active);
    /* The instant DMA completes, process the retained RX frame. */
    tbd.status_length=((uint32_t)OU_DMA_TX_OK<<16)|60U;
    assert(ou_ethernet_path_poll(&path,&bus)==1);
    assert(path.received==2U&&path.answered==2U&&path.ignored==0U);
    assert(path.tx.active);
    assert((rbd.status_length&OU_DMA_BD_FULL)==0U);
    assert(tx[12]==8&&tx[13]==6&&tx[21]==2);
    tbd.status_length=((uint32_t)OU_DMA_TX_OK<<16)|60U;
    assert(ou_ethernet_path_poll(&path,&bus)==0);
    assert(!path.tx.active);
    assert(ou_ethernet_path_stop(&path,&bus)==0);
    assert(!path.started);
}
static void test_invalid(void) {
    ou_ethernet_path path;
    ou_dma_bd rbd,tbd;uint8_t rx[1518],tx[1518];
    ou_net_identity disabled=id;
    disabled.enabled=0;
    assert(ou_ethernet_path_init(&path,&disabled,&rbd,0x08008000,rx,0x08010000,
                                 &tbd,0x08008400,tx,0x08011000)==-1);
    assert(!path.started);
    assert(ou_ethernet_path_poll(&path,0)==-1);
    disabled=id;
    assert(ou_ethernet_path_init(&path,&disabled,&rbd,0x08008000,rx,0x08010000,
                                 &tbd,0x08008400,tx,0x08010080)==-1);
    assert(!path.started);
}
int main(void){test_path();test_invalid();puts("PASS: RX DMA to ARP to TX DMA integration; backpressure and release");return 0;}
