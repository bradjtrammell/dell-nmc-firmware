#ifndef OU_BOARD_RUNTIME_H
#define OU_BOARD_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
#include "ou_ethernet_manager.h"
#include "ou_ns7520_hal.h"

typedef int (*ou_runtime_clock_ms)(void *opaque, uint32_t *monotonic_ms);

typedef struct {
    ou_ethernet_path ethernet;
    ou_mac_state mac;
    ou_dma_bus bus;
    ou_ns7520_hal *hal;
    ou_runtime_clock_ms clock_ms;
    void *clock_context;
    uint32_t last_ms;
    uint32_t successful_polls;
    uint32_t failed_polls;
    uint8_t running;
} ou_board_runtime;

typedef struct {
    const ou_vlan_table *vlans;
    const ou_mac_config *mac;
    ou_ns7520_hal *hal;
    ou_dma_bd *rx_descriptor;
    uint32_t rx_descriptor_bus;
    uint8_t *rx_data;
    uint32_t rx_data_bus;
    ou_dma_bd *tx_descriptor;
    uint32_t tx_descriptor_bus;
    uint8_t *tx_data;
    uint32_t tx_data_bus;
    ou_runtime_clock_ms clock_ms;
    void *clock_context;
    uint32_t tcp_boot_seed;
    ou_tcp_app http_application;
    void *http_context;
    uint8_t enable_tcp_http;
} ou_board_boot_config;

/* Start the real polling RX/TX runtime after board preflight. There is no
 * automatic default MAC, IP, PHY clock, boot entropy, or RAM mapping. */
int ou_board_runtime_start(ou_board_runtime *runtime,
                           const ou_board_boot_config *config);
int ou_board_runtime_step(ou_board_runtime *runtime);
int ou_board_runtime_stop(ou_board_runtime *runtime);
#endif
