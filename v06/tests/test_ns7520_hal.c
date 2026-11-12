#include "ou_ns7520_hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {unsigned reads,writes,syncs;uint32_t last_addr,last_value;} fake;
static int rd(void *ctx,uint32_t addr,uint32_t*value){fake*f=ctx;++f->reads;f->last_addr=addr;*value=0;return 0;}
static int wr(void *ctx,uint32_t addr,uint32_t value){fake*f=ctx;++f->writes;f->last_addr=addr;f->last_value=value;return 0;}
static int sync(void *ctx,uint32_t addr,size_t len,uint8_t tx){fake*f=ctx;(void)addr;(void)len;(void)tx;++f->syncs;return 0;}
int main(void){
 ou_ns7520_hal h;ou_dma_bus bus;fake f={0};uint32_t out=999;
 memset(&h,0,sizeof(h));
 assert(ou_ns7520_hal_read32(&h,OU_DMA_RX_CTRL,&out)==-1&&!out);
 assert(ou_ns7520_hal_init(&h,rd,wr,sync,&f,0x08000000,0x100000,0,1,1,1)==-1);
 assert(ou_ns7520_hal_init(&h,rd,wr,sync,&f,0x08000000,0x100000,1,1,1,1)==0);
 assert(ou_ns7520_hal_bus(&h,&bus)==0);
 assert(bus.exclusive_mac_ownership&&bus.mac_phy_clocks_verified&&bus.bus_mapping_verified);
 assert(bus.read32(bus.context,OU_DMA_RX_CTRL,&out)==0&&f.reads==1);
 assert(bus.read32(bus.context,0xff000000,&out)==-1&&f.reads==1);
 assert(bus.read32(bus.context,OU_DMA_RX_CTRL+1,&out)==-1&&f.reads==1);
 assert(bus.write32(bus.context,OU_DMA_RX_CTRL,OU_DMA_CONTROL_ENABLE)==0&&f.writes==1);
 assert(bus.write32(bus.context,0xffb00000,0)==-1&&f.writes==1);
 assert(bus.write32(bus.context,0xffd00000,0)==-1&&f.writes==1);
 assert(bus.write32(bus.context,0xffc00000,0)==-1&&f.writes==1);
 assert(bus.write32(bus.context,OU_DMA_RX_BDP,0x08001000)==0&&f.writes==2);
 assert(bus.write32(bus.context,OU_DMA_TX_BDP,0x08000010)==-1&&f.writes==2);
 assert(bus.write32(bus.context,OU_DMA_TX_BDP,0x08100000)==-1&&f.writes==2);
 assert(bus.sync(bus.context,0x08001000,1518,1)==0&&f.syncs==1);
 assert(bus.sync(bus.context,0x080ffffc,1518,1)==-1&&f.syncs==1);
 assert(bus.sync(bus.context,0x08001000,1,2)==-1&&f.syncs==1);
 assert(bus.sync(bus.context,0x08001001,8,0)==-1&&f.syncs==1);
 assert(ou_ns7520_arm_read32(0,OU_DMA_RX_CTRL,&out)==-1);
 assert(ou_ns7520_arm_write32(0,OU_DMA_RX_CTRL,1)==-1);
 puts("PASS: physical NS7520 MMIO whitelist, DMA RAM bounds, clock/ownership gating");
 return 0;
}
