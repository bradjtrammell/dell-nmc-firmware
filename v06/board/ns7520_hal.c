#include "ou_ns7520_hal.h"
#include "ou_ns7520_timer.h"

/* NS7520 registers documented in Digi Hardware Reference Rev G. Reads of
 * bus status are permitted; only the MAC and its dedicated DMA channels
 * can be written. No writable clocks, GPIO, watchdog, serial or flash. */
static int readable(uint32_t address) {
    switch (address) {
        case UINT32_C(0xFFB00000): /* GEN SYS control */
        case UINT32_C(0xFFB00004): /* GEN SYS status */
        case UINT32_C(0xFFB00030): /* GEN interrupt enable */
        case UINT32_C(0xFF800000): /* EFE general control */
        case UINT32_C(0xFF800004): /* EFE status */
        case OU_MAC_MAC1:
        case OU_MAC_MAC2:
        case OU_MAC_IPGT:
        case OU_MAC_IPGR:
        case OU_MAC_CLRT:
        case OU_MAC_MAXF:
        case OU_MAC_SA1:
        case OU_MAC_SA2:
        case OU_MAC_SA3:
        case OU_DMA_RX_BDP:
        case OU_DMA_RX_CTRL:
        case OU_DMA_RX_STATUS:
        case OU_DMA_TX_BDP:
        case OU_DMA_TX_CTRL:
        case OU_DMA_TX_STATUS:
        case UINT32_C(0xFF800420): /* MCFG */
        case UINT32_C(0xFF800424): /* MCMD */
        case UINT32_C(0xFF800428): /* MADR */
        case UINT32_C(0xFF80042C): /* MWTD */
        case UINT32_C(0xFF800430): /* MRDD */
        case UINT32_C(0xFF800434): /* MIND */
            return 1;
        default: return 0;
    }
}
static int writable(uint32_t address) {
    switch (address) {
        case OU_MAC_EGCR:
        case OU_MAC_MAC1:
        case OU_MAC_MAC2:
        case OU_MAC_IPGT:
        case OU_MAC_IPGR:
        case OU_MAC_CLRT:
        case OU_MAC_MAXF:
        case OU_MAC_SA1:
        case OU_MAC_SA2:
        case OU_MAC_SA3:
        case UINT32_C(0xFF800424): /* PHY MCMD */
        case UINT32_C(0xFF800428): /* PHY MADR */
        case OU_DMA_RX_BDP:
        case OU_DMA_RX_CTRL:
        case OU_DMA_TX_BDP:
        case OU_DMA_TX_CTRL:
            return 1;
        default: return 0;
    }
}
static int valid(const ou_ns7520_hal *hal) {
    return hal && hal->configured == 1U &&
           hal->board_verified == 1U && hal->mac_exclusive == 1U &&
           hal->phy_clocks_verified == 1U &&
           hal->dma_mapping_verified == 1U &&
           hal->read && hal->write && hal->dma_barrier;
}
int ou_ns7520_hal_init(ou_ns7520_hal *hal, ou_hal_read read,
                       ou_hal_write write, ou_hal_dma_barrier barrier,
                       void *opaque, uint32_t dma_start, uint32_t dma_span,
                       uint8_t board_verified, uint8_t mac_exclusive,
                       uint8_t phy_clocks_verified, uint8_t dma_mapping_verified) {
    if (!hal) return -1;
    *hal=(ou_ns7520_hal){0};
    if (!read || !write || !barrier || board_verified != 1U ||
        mac_exclusive != 1U || phy_clocks_verified != 1U ||
        dma_mapping_verified != 1U || dma_start == 0U ||
        (dma_start & 3U) != 0U || dma_span < 4096U ||
        dma_span > OU_DMA_BD_ADDR_MASK ||
        dma_start > OU_DMA_BD_ADDR_MASK - dma_span) return -1;
    hal->read=read;hal->write=write;hal->dma_barrier=barrier;
    hal->opaque=opaque;hal->dma_start=dma_start;hal->dma_span=dma_span;
    hal->board_verified=board_verified;hal->mac_exclusive=mac_exclusive;
    hal->phy_clocks_verified=phy_clocks_verified;
    hal->dma_mapping_verified=dma_mapping_verified;
    hal->configured=1U;return 0;
}
/* A timer lease never permits general system, watchdog or flash writes. */
static int timer_register(const ou_ns7520_hal *hal,uint32_t reg){
 uint32_t control;
 if(!hal || hal->timer_bound!=1U ||
    (hal->timer_index!=1U && hal->timer_index!=2U))return 0;
 control=hal->timer_index==1U?OU_TIMER1_CTRL:OU_TIMER2_CTRL;
 return reg==control || reg==control+4U;
}
int ou_ns7520_hal_timer_bind(ou_ns7520_hal *hal,unsigned timer_index,
                              uint8_t exclusive,uint8_t clock_verified){
 if(!valid(hal) || hal->timer_bound ||
    (timer_index!=1U && timer_index!=2U) ||
    exclusive!=1U || clock_verified!=1U)return -1;
 hal->timer_index=(uint8_t)timer_index;hal->timer_bound=1U;
 return 0;
}
int ou_ns7520_hal_timer_unbind(ou_ns7520_hal *hal){
 uint32_t control=0U,reg;
 if(!valid(hal) || !hal->timer_bound)return -1;
 reg=hal->timer_index==1U?OU_TIMER1_CTRL:OU_TIMER2_CTRL;
 if(hal->read(hal->opaque,reg,&control)!=0 || (control&OU_TIMER_TE))return -1;
 hal->timer_bound=0U;hal->timer_index=0U;
 return 0;
}
int ou_ns7520_hal_read32(void *opaque, uint32_t reg, uint32_t *value) {
    ou_ns7520_hal *hal=opaque;
    if (!value) return -1;
    *value=0U;
    if (!valid(hal) || (reg & 3U) || (!readable(reg) && !timer_register(hal,reg))) return -1;
    return hal->read(hal->opaque,reg,value);
}
int ou_ns7520_hal_write32(void *opaque, uint32_t reg, uint32_t value) {
    ou_ns7520_hal *hal=opaque;
    if (!valid(hal) || (reg & 3U) || (!writable(reg) && !timer_register(hal,reg))) return -1;
    /* Never permit DMA pointer registers to target outside qualified RAM. */
    if ((reg == OU_DMA_RX_BDP || reg == OU_DMA_TX_BDP) &&
        (value < hal->dma_start || value >
          hal->dma_start + hal->dma_span - (uint32_t)sizeof(ou_dma_bd) ||
          (value & 1023U) != 0U)) return -1;
    return hal->write(hal->opaque,reg,value);
}
int ou_ns7520_hal_dma_sync(void *opaque, uint32_t bus_address,
                            size_t length, uint8_t to_device) {
    ou_ns7520_hal *hal=opaque;
    uint32_t delta;
    if (!valid(hal) || to_device > 1U || length == 0U ||
        (bus_address & 3U) != 0U || bus_address < hal->dma_start ||
        length > hal->dma_span) return -1;
    delta=bus_address-hal->dma_start;
    if (delta > hal->dma_span || length > hal->dma_span-delta) return -1;
    /* ARM7 has no integrated cache-coherency abstraction. The board-specific
     * callback MUST verify/provide the required memory visibility ordering. */
    return hal->dma_barrier(hal->opaque,bus_address,length,to_device);
}
int ou_ns7520_hal_bus(ou_ns7520_hal *hal, ou_dma_bus *bus) {
    if (!bus) return -1;
    *bus=(ou_dma_bus){0};
    if (!valid(hal)) return -1;
    bus->read32=ou_ns7520_hal_read32;
    bus->write32=ou_ns7520_hal_write32;
    bus->sync=ou_ns7520_hal_dma_sync;
    bus->context=hal;
    bus->exclusive_mac_ownership=1U;
    bus->mac_phy_clocks_verified=1U;
    bus->bus_mapping_verified=1U;
    return 0;
}
int ou_ns7520_arm_read32(void *context, uint32_t reg, uint32_t *value) {
    (void)context;
    if (!value) return -1;
    *value=0U;
#if defined(__arm__) && defined(__ARMEB__)
    if ((reg & 3U) != 0U) return -1;
    *value=*(volatile const uint32_t *)(uintptr_t)reg;
    return 0;
#else
    (void)reg;
    return -1;
#endif
}
int ou_ns7520_arm_write32(void *context, uint32_t reg, uint32_t value) {
    (void)context;
#if defined(__arm__) && defined(__ARMEB__)
    if ((reg & 3U) != 0U) return -1;
    *(volatile uint32_t *)(uintptr_t)reg=value;
    return 0;
#else
    (void)reg;(void)value;
    return -1;
#endif
}
