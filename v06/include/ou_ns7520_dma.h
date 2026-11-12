#ifndef OU_NS7520_DMA_H
#define OU_NS7520_DMA_H
#include <stddef.h>
#include <stdint.h>

#define OU_DMA_RX_BDP UINT32_C(0xFF900000)
#define OU_DMA_RX_CTRL UINT32_C(0xFF900010)
#define OU_DMA_RX_STATUS UINT32_C(0xFF900014)
#define OU_DMA_TX_BDP UINT32_C(0xFF900080)
#define OU_DMA_TX_CTRL UINT32_C(0xFF900090)
#define OU_DMA_TX_STATUS UINT32_C(0xFF900094)
#define OU_DMA_BD_WRAP UINT32_C(0x80000000)
#define OU_DMA_BD_INTERRUPT UINT32_C(0x40000000)
#define OU_DMA_BD_LAST UINT32_C(0x20000000)
#define OU_DMA_BD_ADDR_MASK UINT32_C(0x1fffffff)
#define OU_DMA_BD_FULL UINT32_C(0x00008000)
#define OU_DMA_BD_LENGTH_MASK UINT32_C(0x00007fff)
#define OU_DMA_CONTROL_ENABLE UINT32_C(0x80000000)
#define OU_DMA_CONTROL_TX_MODE UINT32_C(0x04000000)
#define OU_DMA_RX_OK UINT16_C(0x2000)
#define OU_DMA_TX_OK UINT16_C(0x8000)
#define OU_DMA_RX_ERROR_MASK UINT16_C(0x07f0)
#define OU_DMA_TX_ABORT_MASK UINT16_C(0x1f00)
#define OU_DMA_MIN_FRAME 60U
#define OU_DMA_MAX_FRAME 1518U

typedef struct {
    volatile uint32_t address_flags;
    volatile uint32_t status_length;
} ou_dma_bd;

typedef int (*ou_dma_read32)(void *, uint32_t, uint32_t *);
typedef int (*ou_dma_write32)(void *, uint32_t, uint32_t);
/* Must flush/invalidate cache and supply required memory ordering for DMA.
 * The callback must distinguish CPU->device vs device->CPU transactions. */
typedef int (*ou_dma_sync)(void *, uint32_t bus_address, size_t length,
                           uint8_t to_device);

typedef struct {
    ou_dma_read32 read32;
    ou_dma_write32 write32;
    ou_dma_sync sync;
    void *context;
    uint8_t exclusive_mac_ownership;
    uint8_t mac_phy_clocks_verified;
    uint8_t bus_mapping_verified;
} ou_dma_bus;

typedef struct {
    ou_dma_bd *bd;
    uint8_t *buffer;
    uint32_t bd_bus;
    uint32_t buffer_bus;
    uint16_t buffer_capacity;
    uint8_t direction_tx;
    uint8_t prepared;
    uint8_t active;
    uint8_t data_pending;
    uint8_t faulted;
} ou_dma_queue;

/* Caller supplies a 1024-aligned DMA-visible descriptor area, physically
 * mapped nonoverlapping buffer, and exact CPU mappings. No allocations. */
int ou_dma_prepare(ou_dma_queue *queue, ou_dma_bd *bd, uint32_t bd_bus,
                   uint8_t *data, uint32_t data_bus, uint16_t capacity,
                   uint8_t direction_tx);
/* Starts RX with empty descriptor, or TX after transmit payload has been set.
 * No MAC EFE changes here; no unknown register addresses are touched. */
int ou_dma_start(const ou_dma_bus *bus, ou_dma_queue *queue);
/* Submit one Ethernet frame to a stopped TX DMA queue. */
int ou_dma_tx_copy(ou_dma_queue *queue, const uint8_t *frame, size_t len);
/* Returns 1 for complete valid frame, 0 for pending, -1 for fault.
 * RX payload remains owned by CPU until ou_dma_rx_release(). */
int ou_dma_rx_poll(const ou_dma_bus *bus, ou_dma_queue *queue,
                   const uint8_t **frame, size_t *length);
/* Re-arm the last delivered RX descriptor after caller finishes reading. */
int ou_dma_rx_release(const ou_dma_bus *bus, ou_dma_queue *queue);
/* Returns 1 success, 0 pending, -1 on failed completion/IO. */
int ou_dma_tx_poll(const ou_dma_bus *bus, ou_dma_queue *queue);
/* Disable the DMA channel; do not touch EFE, PHY, flash or UPS. */
int ou_dma_stop(const ou_dma_bus *bus, ou_dma_queue *queue);
#endif
