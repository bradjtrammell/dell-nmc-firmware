#ifndef OU_NS7520_BOARD_MMIO_H
#define OU_NS7520_BOARD_MMIO_H
#include "ou_board_probe.h"

/* A read-only, fixed-address NS7520 register snapshot. The source callback
 * is provided by the verified physical H910P port (or a host test fixture).
 * Do not use this with unknown address decode or an unrecoverable live card. */
typedef struct {
    ou_mem_read32 physical_read;
    void *context;
    uint8_t explicitly_authorized;
    uint8_t peripheral_mapping_verified;
    uint8_t recovery_access_verified;
} ou_ns7520_probe_source;

/* Exactly 23 documented non-FIFO system, Ethernet, serial-control and memory
 * controller registers. Reads no flash, RAM contents, UART data or UPS bus.
 * Output stays zero on any error; no field grants hardware/install approval. */
int ou_ns7520_board_snapshot(const ou_ns7520_probe_source *source,
                             ou_board_probe *snapshot);

/* Exposed for synthetic acceptance tests and verified BSP adapters. Read-only.
 * Returns -1 for every other address, including UART FIFOs, flash and GPIO. */
int ou_ns7520_probe_read32(void *context, uint32_t address, uint32_t *value);
#endif
