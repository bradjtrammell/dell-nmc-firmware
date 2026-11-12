#ifndef OU_ETHERNET_PATH_H
#define OU_ETHERNET_PATH_H
#include <stdint.h>
#include "ou_ipv4_wire.h"
#include "ou_ns7520_dma.h"
#include "ou_vlan.h"
#include "ou_tcp_engine.h"

typedef struct {
    ou_dma_queue rx;
    ou_dma_queue tx;
    ou_net_identity identity;
    ou_vlan_table vlans;
    ou_tcp_engine tcp;
    ou_tcp_app tcp_app;
    void *tcp_context;
    uint8_t tcp_enabled;
    uint8_t response[OU_DMA_MAX_FRAME];
    uint32_t received;
    uint32_t answered;
    uint32_t ignored;
    uint32_t errors;
    uint8_t started;
} ou_ethernet_path;

/* One-slot synchronous RX1A -> Ethernet/ARP/ICMP -> TX2 path. The physical
 * adapter must configure MAC, RX/TX clocks, filters, PHY, FCS stripping, DMA
 * address translation and data coherency prior to calling this interface.
 * Does not provide TCP, HTTP, authentication or an UPS driver. */
int ou_ethernet_path_init(ou_ethernet_path *path,
                          const ou_net_identity *identity,
                          ou_dma_bd *rx_desc, uint32_t rx_desc_bus,
                          uint8_t *rx_data, uint32_t rx_data_bus,
                          ou_dma_bd *tx_desc, uint32_t tx_desc_bus,
                          uint8_t *tx_data, uint32_t tx_data_bus);
/* 802.1Q interfaces, explicitly configured with no implied default trunk. */
int ou_ethernet_path_init_vlans(ou_ethernet_path *path,
                          const ou_vlan_table *config,
                          ou_dma_bd *rx_desc, uint32_t rx_desc_bus,
                          uint8_t *rx_data, uint32_t rx_data_bus,
                          ou_dma_bd *tx_desc, uint32_t tx_desc_bus,
                          uint8_t *tx_data, uint32_t tx_data_bus);
int ou_ethernet_path_start(ou_ethernet_path *path, const ou_dma_bus *bus);
/* 1 replied, 0 none or no reply needed, -1 on driver fault. */
int ou_ethernet_path_poll(ou_ethernet_path *path, const ou_dma_bus *bus);
/* Must be called before DMA startup with hardware-backed boot entropy and
 * an authenticated/read-only HTTP callback. Never enabled automatically. */
int ou_ethernet_path_tcp_enable(ou_ethernet_path *path,uint32_t seed,
                                ou_tcp_app app,void *context);
/* Explicit monotonic time source is mandatory for TCP retransmission. */
int ou_ethernet_path_poll_at(ou_ethernet_path *path,const ou_dma_bus *bus,
                              uint32_t now_ms);
int ou_ethernet_path_stop(ou_ethernet_path *path, const ou_dma_bus *bus);
#endif
