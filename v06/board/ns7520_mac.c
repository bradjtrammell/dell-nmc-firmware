#include "ou_ns7520_mac.h"

static int valid_bus(const ou_dma_bus *bus) {
    return bus && bus->read32 && bus->write32 && bus->sync &&
           bus->exclusive_mac_ownership == 1U &&
           bus->mac_phy_clocks_verified == 1U &&
           bus->bus_mapping_verified == 1U;
}
static int valid_config(const ou_mac_config *c) {
    unsigned i;
    uint8_t all_zero=1U, all_ff=1U;
    if(!c || c->full_duplex>1U || c->link_confirmed!=1U ||
       c->timing_registers_verified!=1U ||
       (c->station_address[0]&1U)!=0U ||
       c->max_frame<1518U || c->max_frame>1522U ||
       c->ipgt==0U || c->ipgr==0U || c->clrt==0U) return 0;
    for(i=0U;i<6U;++i) {
        if(c->station_address[i]!=0U) all_zero=0U;
        if(c->station_address[i]!=255U) all_ff=0U;
    }
    return !all_zero && !all_ff;
}
static int write_checked(const ou_dma_bus *bus,uint32_t address,uint32_t value) {
    return bus->write32(bus->context,address,value);
}
int ou_mac_configure(const ou_dma_bus *bus,const ou_mac_config *cfg,
                     ou_mac_state *state) {
    uint32_t egcr,mac1;
    if(!state)return -1;
    *state=(ou_mac_state){0};
    if(!valid_bus(bus) || !valid_config(cfg))return -1;
    if(bus->read32(bus->context,OU_MAC_EGCR,&egcr)!=0 ||
       bus->read32(bus->context,OU_MAC_MAC1,&mac1)!=0 ||
       (egcr&(OU_MAC_EGCR_ERX|OU_MAC_EGCR_ERXDMA|OU_MAC_EGCR_ETX|
              OU_MAC_EGCR_ETXDMA))!=0U ||
       (mac1&OU_MAC_MAC1_RXEN)!=0U) return -1;
    /* Only hardware-verified fields. Do NOT touch reset, PHY or
     * station filter hash registers. */
    if(write_checked(bus,OU_MAC_SA1,
         ((uint32_t)cfg->station_address[0]<<8)|cfg->station_address[1])!=0 ||
       write_checked(bus,OU_MAC_SA2,
         ((uint32_t)cfg->station_address[2]<<8)|cfg->station_address[3])!=0 ||
       write_checked(bus,OU_MAC_SA3,
         ((uint32_t)cfg->station_address[4]<<8)|cfg->station_address[5])!=0 ||
       write_checked(bus,OU_MAC_IPGT,cfg->ipgt)!=0 ||
       write_checked(bus,OU_MAC_IPGR,cfg->ipgr)!=0 ||
       write_checked(bus,OU_MAC_CLRT,cfg->clrt)!=0 ||
       write_checked(bus,OU_MAC_MAXF,cfg->max_frame)!=0 ||
       write_checked(bus,OU_MAC_MAC2,OU_MAC_MAC2_PADEN|
                  OU_MAC_MAC2_CRCEN|
                  (cfg->full_duplex?OU_MAC_MAC2_FULLD:0U))!=0) {
        state->faulted=1U;return -1;
    }
    state->full_duplex=cfg->full_duplex;
    state->configured=1U;
    return 0;
}
int ou_mac_activate(const ou_dma_bus *bus,ou_mac_state *state,
                    const ou_dma_queue *rx,const ou_dma_queue *tx) {
    const uint32_t options=OU_MAC_EGCR_ERX|OU_MAC_EGCR_ERXDMA|
                           OU_MAC_EGCR_ETX|OU_MAC_EGCR_ETXDMA|
                           (state && state->full_duplex ? OU_MAC_EGCR_EFULLD : 0U);
    if(!valid_bus(bus)||!state||state->configured!=1U||state->enabled||
       state->faulted||!rx||!tx||rx->direction_tx||!tx->direction_tx||
       !rx->active||!tx->prepared||tx->faulted)return -1;
    if(write_checked(bus,OU_MAC_EGCR,options)!=0) {
        state->faulted=1U;return -1;
    }
    if(write_checked(bus,OU_MAC_MAC1,OU_MAC_MAC1_RXEN)!=0) {
        /* MAC receiver stays off, but quiesce the EFE as well. */
        (void)write_checked(bus,OU_MAC_EGCR,0U);
        state->faulted=1U;return -1;
    }
    state->enabled=1U;
    return 0;
}
int ou_mac_disable(const ou_dma_bus *bus,ou_mac_state *state) {
    int first,second;
    if(!valid_bus(bus)||!state||!state->configured)return -1;
    /* Disable receiver first, then EFE/DMA requests. No reset/clock edits. */
    first=write_checked(bus,OU_MAC_MAC1,0U);
    second=write_checked(bus,OU_MAC_EGCR,0U);
    if(first || second) {state->faulted=1U;return -1;}
    state->enabled=0U;
    return 0;
}
