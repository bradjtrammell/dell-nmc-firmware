#include "ou_board_runtime.h"

static int mapped(const ou_ns7520_hal *hal,uint32_t bus,size_t size) {
    uint32_t offset;
    if (!hal || bus < hal->dma_start || size == 0U || size > hal->dma_span) return 0;
    offset=bus-hal->dma_start;
    return offset <= hal->dma_span && size <= hal->dma_span-offset;
}
static int identities(const ou_board_boot_config *cfg){
    if(!cfg->dma_resolve || !cfg->hal)return 0;
    return cfg->dma_resolve(cfg->dma_resolve_context,cfg->rx_descriptor,
             cfg->rx_descriptor_bus,sizeof(ou_dma_bd))==1 &&
           cfg->dma_resolve(cfg->dma_resolve_context,cfg->tx_descriptor,
             cfg->tx_descriptor_bus,sizeof(ou_dma_bd))==1 &&
           cfg->dma_resolve(cfg->dma_resolve_context,cfg->rx_data,
             cfg->rx_data_bus,OU_DMA_MAX_FRAME)==1 &&
           cfg->dma_resolve(cfg->dma_resolve_context,cfg->tx_data,
             cfg->tx_data_bus,OU_DMA_MAX_FRAME)==1;
}
static int validate(const ou_board_boot_config *cfg) {
    if (!cfg || !cfg->hal || !cfg->hal->configured || !cfg->vlans ||
        !cfg->board_snapshot ||
        cfg->board_snapshot->registers_observed!=1U ||
        /* A decoded address aperture is not installed board evidence.
         * Require independent BSP proof of H910P identity, PHY and rescue
         * before any MAC/DMA register writes. The passive snapshot sets
         * none of these; production startup cannot opt in accidentally. */
        cfg->board_snapshot->h910p_board_verified!=1U ||
        cfg->board_snapshot->ethernet_phy_verified!=1U ||
        cfg->board_snapshot->recovery_verified!=1U ||
        cfg->board_snapshot->soc_revision_matches_reference!=1U ||
        cfg->board_snapshot->application_load_window_decoded!=1U ||
        cfg->board_snapshot->memory.decoded!=1U ||
        /* Controller decode is not physical installed-chip capacity proof.
         * Nonetheless, prevent startup outside the live decoded aperture. */
        !ou_board_application_window(&cfg->board_snapshot->memory,
             OU_NMC_APPLICATION_LOAD,UINT32_C(0x00100000)) ||
        !ou_board_application_window(&cfg->board_snapshot->memory,
             cfg->hal->dma_start,cfg->hal->dma_span) ||
        !cfg->mac || cfg->mac->link_confirmed!=1U || !cfg->clock_ms || !cfg->rx_descriptor ||
        !cfg->tx_descriptor || !cfg->rx_data || !cfg->tx_data ||
        cfg->enable_tcp_http>1U ||
        (cfg->ups_monitor &&
         (cfg->ups_monitor->initialized!=1U ||
          cfg->ups_monitor->link.initialized!=1U ||
          cfg->ups_monitor->hid.initialized!=1U ||
          cfg->ups_monitor->report_count==0U)) ||
        (cfg->enable_tcp_http && (!cfg->tcp_boot_seed || !cfg->http_application)) ||
        !ou_vlan_validate(cfg->vlans) ||
        !mapped(cfg->hal,cfg->rx_descriptor_bus,sizeof(ou_dma_bd)) ||
        !mapped(cfg->hal,cfg->tx_descriptor_bus,sizeof(ou_dma_bd)) ||
        !mapped(cfg->hal,cfg->rx_data_bus,OU_DMA_MAX_FRAME) ||
        !mapped(cfg->hal,cfg->tx_data_bus,OU_DMA_MAX_FRAME) ||
        !identities(cfg))return 0;
    return 1;
}
int ou_board_runtime_start(ou_board_runtime *runtime,
                           const ou_board_boot_config *cfg) {
    uint32_t now_ms=0U;
    ou_phy_bus phy;
    if (!runtime || !cfg || runtime->running || !validate(cfg))return -1;
    if (ou_ns7520_hal_bus(cfg->hal,&runtime->bus)!=0) return -1;
    runtime->hal=cfg->hal;
    if(cfg->clock_ms(cfg->clock_context,&now_ms)!=0) return -1;
    /* Require a real MDIO identity and physical link, not merely a
     * caller-supplied link_confirmed boolean. No PHY configuration writes. */
    phy.read=ou_ns7520_hal_read32;
    phy.write=ou_ns7520_hal_write32;
    phy.context=cfg->hal;
    phy.exclusive_mac_ownership=1U;
    phy.phy_clocks_verified=1U;
    phy.phy_address=0U;
    if(ou_phy_discover(&phy,&runtime->phy)!=0 || !runtime->phy.link_up)
        return -1;
    phy.phy_address=runtime->phy.phy_address;
    if(ou_phy_resolve_link(&phy,&runtime->link)!=0 ||
       runtime->link.full_duplex!=cfg->mac->full_duplex)return -1;
    if(ou_ethernet_path_init_vlans(&runtime->ethernet,cfg->vlans,
          cfg->rx_descriptor,cfg->rx_descriptor_bus,
          cfg->rx_data,cfg->rx_data_bus,
          cfg->tx_descriptor,cfg->tx_descriptor_bus,
          cfg->tx_data,cfg->tx_data_bus)!=0) return -1;
    if(cfg->enable_tcp_http && ou_ethernet_path_tcp_enable(&runtime->ethernet,
          cfg->tcp_boot_seed,cfg->http_application,cfg->http_context)!=0)return -1;
    if(ou_ethernet_manager_start(&runtime->ethernet,&runtime->bus,
                                 cfg->mac,&runtime->mac)!=0)return -1;
    runtime->clock_ms=cfg->clock_ms;
    runtime->clock_context=cfg->clock_context;
    runtime->last_ms=now_ms;
    runtime->last_link_check_ms=now_ms;
    runtime->ups_monitor=cfg->ups_monitor;
    runtime->ups_polls=0U;
    runtime->ups_failures=0U;
    runtime->running=1U;
    return 0;
}
int ou_board_runtime_step(ou_board_runtime *runtime) {
    uint32_t now_ms;
    int status;
    if(!runtime || !runtime->running || !runtime->clock_ms)return -1;
    if(runtime->clock_ms(runtime->clock_context,&now_ms)!=0 ||
       (uint32_t)(now_ms-runtime->last_ms)>UINT32_C(0x7fffffff)) {
        if(runtime->failed_polls!=UINT32_MAX)++runtime->failed_polls;
        (void)ou_board_runtime_stop(runtime);
        return -1;
    }
    runtime->last_ms=now_ms;
    /* Link changes invalidate ongoing TCP sessions and PHY/MAC negotiation;
     * fail closed and quiesce DMA instead of continuing with stale settings. */
    if((uint32_t)(now_ms-runtime->last_link_check_ms)>=1000U) {
        ou_phy_bus phy;
        ou_phy_link current;
        phy.read=ou_ns7520_hal_read32;
        phy.write=ou_ns7520_hal_write32;
        phy.context=runtime->hal;
        phy.exclusive_mac_ownership=1U;
        phy.phy_clocks_verified=1U;
        phy.phy_address=runtime->phy.phy_address;
        if(ou_phy_resolve_link(&phy,&current)!=0 ||
           current.full_duplex!=runtime->link.full_duplex ||
           current.speed100!=runtime->link.speed100) {
            if(runtime->failed_polls!=UINT32_MAX)++runtime->failed_polls;
            (void)ou_board_runtime_stop(runtime);
            return -1;
        }
        runtime->last_link_check_ms=now_ms;
    }
    status=ou_ethernet_path_poll_at(&runtime->ethernet,&runtime->bus,now_ms);
    if(status<0) {
        if(runtime->failed_polls!=UINT32_MAX)++runtime->failed_polls;
        (void)ou_board_runtime_stop(runtime);
        return -1;
    }
    if(runtime->ups_monitor) {
        int ups_result=ou_ups_monitor_step(runtime->ups_monitor,now_ms);
        if(runtime->ups_polls!=UINT32_MAX)++runtime->ups_polls;
        if(ups_result<0) {
            /* Never take the management network offline because UPS
             * telemetry is unavailable. Revoke the failed UART owner and
             * immediately hide UPS measurements; preserve the service's
             * anti-replay history. */
            ou_ups_monitor_revoke(runtime->ups_monitor);
            runtime->ups_monitor=0;
            if(runtime->ups_failures!=UINT32_MAX)++runtime->ups_failures;
        }
    }
    if(runtime->successful_polls!=UINT32_MAX)++runtime->successful_polls;
    return status;
}
int ou_board_runtime_stop(ou_board_runtime *runtime) {
    int result;
    if(!runtime || !runtime->running)return -1;
    /* Network/link loss also withdraws any UPS value obtained through
     * this boot session. It never resets physical anti-replay history. */
    if(runtime->ups_monitor) {
        ou_ups_monitor_revoke(runtime->ups_monitor);
        runtime->ups_monitor=0;
    }
    result=ou_ethernet_manager_stop(&runtime->ethernet,&runtime->bus,
                                    &runtime->mac);
    runtime->running=0U;
    return result;
}
