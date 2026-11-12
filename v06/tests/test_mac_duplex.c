#include "ou_ns7520_mac.h"
#include <assert.h>
#include <stdio.h>
typedef struct {uint32_t egcr,mac1,mac2;unsigned writes;} mock;
static int rd(void *ctx,uint32_t a,uint32_t*o){mock*m=ctx;if(a==OU_MAC_EGCR){*o=m->egcr;return 0;}if(a==OU_MAC_MAC1){*o=m->mac1;return 0;}return -1;}
static int wr(void*ctx,uint32_t a,uint32_t v){mock*m=ctx;++m->writes;if(a==OU_MAC_EGCR)m->egcr=v;if(a==OU_MAC_MAC1)m->mac1=v;if(a==OU_MAC_MAC2)m->mac2=v;return 0;}
static int sync(void*ctx,uint32_t a,size_t n,uint8_t d){(void)ctx;(void)a;(void)n;(void)d;return 0;}
static void check(unsigned full){
 mock m={0};ou_mac_state state;ou_dma_bus b={rd,wr,sync,&m,1,1,1};ou_dma_queue rx={0},tx={0};
 ou_mac_config cfg={{2,10,20,30,40,50},0,1,1,0x15,0x12,0x370f,1518};
 cfg.full_duplex=(uint8_t)full;
 assert(ou_mac_configure(&b,&cfg,&state)==0);
 assert(state.full_duplex==full);
 assert((m.mac2&OU_MAC_MAC2_FULLD)==(full?OU_MAC_MAC2_FULLD:0));
 rx.prepared=1;rx.active=1;tx.prepared=1;tx.direction_tx=1;
 assert(ou_mac_activate(&b,&state,&rx,&tx)==0);
 assert((m.egcr&OU_MAC_EGCR_EFULLD)==(full?OU_MAC_EGCR_EFULLD:0));
 assert((m.egcr&OU_MAC_EGCR_ETXDMA)!=0);
 assert(ou_mac_disable(&b,&state)==0);
}
int main(void){check(0);check(1);puts("PASS: NS7520 MAC2/EFE negotiated duplex flags agree");return 0;}
