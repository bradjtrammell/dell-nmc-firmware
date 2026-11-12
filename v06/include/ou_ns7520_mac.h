#ifndef OU_NS7520_MAC_H
#define OU_NS7520_MAC_H
#include <stdint.h>
#include "ou_ns7520_dma.h"

#define OU_MAC_EGCR UINT32_C(0xFF800000)
#define OU_MAC_MAC1 UINT32_C(0xFF800400)
#define OU_MAC_MAC2 UINT32_C(0xFF800404)
#define OU_MAC_IPGT UINT32_C(0xFF800408)
#define OU_MAC_IPGR UINT32_C(0xFF80040C)
#define OU_MAC_CLRT UINT32_C(0xFF800410)
#define OU_MAC_MAXF UINT32_C(0xFF800414)
#define OU_MAC_SA1 UINT32_C(0xFF800440)
#define OU_MAC_SA2 UINT32_C(0xFF800444)
#define OU_MAC_SA3 UINT32_C(0xFF800448)
#define OU_MAC_EGCR_ERX UINT32_C(0x80000000)
#define OU_MAC_EGCR_ERXDMA UINT32_C(0x40000000)
#define OU_MAC_EGCR_ETX UINT32_C(0x00800000)
#define OU_MAC_EGCR_ETXDMA UINT32_C(0x00400000)
#define OU_MAC_MAC1_RXEN 1U
#define OU_MAC_MAC2_PADEN UINT32_C(0x20)
#define OU_MAC_MAC2_CRCEN UINT32_C(0x10)
#define OU_MAC_MAC2_FULLD UINT32_C(1)

typedef struct {
    uint8_t station_address[6];
    uint8_t full_duplex;
    uint8_t link_confirmed;
    uint8_t timing_registers_verified;
    uint16_t ipgt;
    uint16_t ipgr;
    uint16_t clrt;
    uint16_t max_frame;
} ou_mac_config;

typedef struct {
    uint8_t configured;
    uint8_t enabled;
    uint8_t faulted;
} ou_mac_state;

/* Exclusively-owned, MMIO-capable DMA bus. No guessed H910P address,
 * PHY setup, default IPG timings or MAC identity. Caller establishes
 * PHY link and known-good timing settings first. */
int ou_mac_configure(const ou_dma_bus *bus, const ou_mac_config *config,
                     ou_mac_state *state);
int ou_mac_activate(const ou_dma_bus *bus, ou_mac_state *state,
                    const ou_dma_queue *rx, const ou_dma_queue *tx);
int ou_mac_disable(const ou_dma_bus *bus, ou_mac_state *state);
#endif
