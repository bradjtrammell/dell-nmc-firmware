#ifndef OU_NS7520_HAL_H
#define OU_NS7520_HAL_H
#include <stddef.h>
#include <stdint.h>
#include "ou_ns7520_dma.h"
#include "ou_ns7520_mac.h"

/* Register-level HAL for the NS7520 memory-mapped MAC and fly-by DMA.
 * This module never configures an unknown flash chip, bus clock, PHY or UPS.
 * Hardware evidence is supplied by an independently qualified BSP. */
typedef int (*ou_hal_read)(void *, uint32_t, uint32_t *);
typedef int (*ou_hal_write)(void *, uint32_t, uint32_t);
typedef int (*ou_hal_dma_barrier)(void *, uint32_t, size_t, uint8_t);

typedef struct {
    ou_hal_read read;
    ou_hal_write write;
    ou_hal_dma_barrier dma_barrier;
    void *opaque;
    uint32_t dma_start;
    uint32_t dma_span;
    uint8_t board_verified;
    uint8_t mac_exclusive;
    uint8_t phy_clocks_verified;
    uint8_t dma_mapping_verified;
    uint8_t configured;
} ou_ns7520_hal;

/* Fails closed unless explicit evidence and platform operations are present.
 * RAM window covers all DMA bus addresses, not merely a CPU-side pointer. */
int ou_ns7520_hal_init(ou_ns7520_hal *hal, ou_hal_read read,
                       ou_hal_write write, ou_hal_dma_barrier barrier,
                       void *opaque, uint32_t dma_start, uint32_t dma_span,
                       uint8_t board_verified, uint8_t mac_exclusive,
                       uint8_t phy_clocks_verified, uint8_t dma_mapping_verified);

int ou_ns7520_hal_read32(void *opaque, uint32_t reg, uint32_t *value);
int ou_ns7520_hal_write32(void *opaque, uint32_t reg, uint32_t value);
int ou_ns7520_hal_dma_sync(void *opaque, uint32_t bus_address,
                            size_t length, uint8_t to_device);
int ou_ns7520_hal_bus(ou_ns7520_hal *hal, ou_dma_bus *bus);

/* Direct ARM7 physical-register primitives. Guarded to ARM big-endian only;
 * these MUST be invoked solely through the whitelist above, after platform
 * evidence is satisfied. Not executable from a host test process. */
int ou_ns7520_arm_read32(void *context, uint32_t reg, uint32_t *value);
int ou_ns7520_arm_write32(void *context, uint32_t reg, uint32_t value);
#endif
