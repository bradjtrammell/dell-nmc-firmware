#include "ou_ns7520_mac.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {uint32_t address[32],value[32];unsigned count,reads,fail_write;uint32_t egcr,mac1;}mock;
static int rd(void*p,uint32_t a,uint32_t*out){mock*m=p;++m->reads;if(a==OU_MAC_EGCR)*out=m->egcr;else if(a==OU_MAC_MAC1)*out=m->mac1;else return -1;return 0;}
static int wr(void*p,uint32_t a,uint32_t v){mock*m=p;unsigned index=m->count++;if(index==m->fail_write || index>=32)return -1;m->address[index]=a;m->value[index]=v;if(a==OU_MAC_EGCR)m->egcr=v;if(a==OU_MAC_MAC1)m->mac1=v;return 0;}
static int sy(void*p,uint32_t a,size_t n,uint8_t d){(void)p;(void)a;(void)n;(void)d;return 0;}
static ou_mac_config cfg(void){ou_mac_config c={{2,10,20,30,40,50},1,1,1,0x15,0x0012,0x370f,1518};return c;}
static void init(mock*m){memset(m,0,sizeof(*m));m->fail_write=9999U;}
static void test_configure(void){mock m;ou_dma_bus b={rd,wr,sy,&m,1,1,1};ou_mac_state s;ou_mac_config c=cfg();ou_dma_queue rx={0},tx={0};init(&m);
 assert(ou_mac_configure(&b,&c,&s)==0);assert(s.configured&&!s.enabled&&s.full_duplex);
 assert(m.count==8);assert(m.address[0]==OU_MAC_SA1&&m.value[0]==0x020a);
 assert(m.address[1]==OU_MAC_SA2&&m.value[1]==0x141e);
 assert(m.address[2]==OU_MAC_SA3&&m.value[2]==0x2832);
 assert(m.address[3]==OU_MAC_IPGT&&m.value[3]==0x15);
 assert(m.address[7]==OU_MAC_MAC2&&m.value[7]==0x31);
 assert(ou_mac_activate(&b,&s,&rx,&tx)==-1);
 rx.active=1;rx.prepared=1;tx.prepared=1;tx.direction_tx=1;
 assert(ou_mac_activate(&b,&s,&rx,&tx)==0);assert(s.enabled);
 assert(m.address[8]==OU_MAC_EGCR&&m.value[8]==0xc0c10000);
 assert(m.address[9]==OU_MAC_MAC1&&m.value[9]==1);
 assert(ou_mac_disable(&b,&s)==0);assert(!s.enabled);
 assert(m.address[10]==OU_MAC_MAC1&&m.value[10]==0);
 assert(m.address[11]==OU_MAC_EGCR&&m.value[11]==0);
}
static void test_gates(void){mock m;ou_dma_bus b={rd,wr,sy,&m,0,1,1};ou_mac_state s;ou_mac_config c=cfg();init(&m);
 assert(ou_mac_configure(&b,&c,&s)==-1&&!m.count);
 b.exclusive_mac_ownership=1;b.mac_phy_clocks_verified=0;
 assert(ou_mac_configure(&b,&c,&s)==-1&&!m.count);
 b.mac_phy_clocks_verified=1;c.link_confirmed=0;
 assert(ou_mac_configure(&b,&c,&s)==-1&&!m.count);
 c=cfg();c.station_address[0]=1;
 assert(ou_mac_configure(&b,&c,&s)==-1&&!m.count);
 c=cfg();c.timing_registers_verified=0;
 assert(ou_mac_configure(&b,&c,&s)==-1&&!m.count);
 c=cfg();m.egcr=OU_MAC_EGCR_ERX;
 assert(ou_mac_configure(&b,&c,&s)==-1&&!m.count);
 m.egcr=0;m.mac1=OU_MAC_MAC1_RXEN;
 assert(ou_mac_configure(&b,&c,&s)==-1&&!m.count);
}
static void test_enable_rollback(void){
 mock m;ou_dma_bus b={rd,wr,sy,&m,1,1,1};ou_mac_state s;
 ou_mac_config c=cfg();ou_dma_queue rx={0},tx={0};
 init(&m);assert(ou_mac_configure(&b,&c,&s)==0);
 rx.prepared=1;rx.active=1;tx.prepared=1;tx.direction_tx=1;
 m.fail_write=9U;
 assert(ou_mac_activate(&b,&s,&rx,&tx)==-1);
 assert(s.faulted&&!s.enabled);
 assert(m.egcr==0U);
 assert(m.count==11U);
}
static void test_write_failures(void){mock m;ou_dma_bus b={rd,wr,sy,&m,1,1,1};ou_mac_state s;ou_mac_config c=cfg();unsigned i;
 for(i=0;i<8;i++) {init(&m);m.fail_write=i;
  assert(ou_mac_configure(&b,&c,&s)==-1 && s.faulted);
 }
}
int main(void){test_configure();test_gates();test_enable_rollback();test_write_failures();puts("PASS: NS7520 MAC station address, EFE/MAC startup, DMA ordering and failure gates");return 0;}
