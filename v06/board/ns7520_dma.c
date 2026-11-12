#include "ou_ns7520_dma.h"

static int valid_bus(const ou_dma_bus *bus) {
    return bus && bus->read32 && bus->write32 && bus->sync &&
           bus->exclusive_mac_ownership == 1U &&
           bus->mac_phy_clocks_verified == 1U &&
           bus->bus_mapping_verified == 1U;
}
static int synced(const ou_dma_bus *bus, uint32_t address, size_t length,
                  uint8_t to_device) {
    return bus->sync(bus->context, address, length, to_device);
}
static uint32_t control(const ou_dma_queue *q) {
    return q->direction_tx ? OU_DMA_TX_CTRL : OU_DMA_RX_CTRL;
}
static uint32_t pointer(const ou_dma_queue *q) {
    return q->direction_tx ? OU_DMA_TX_BDP : OU_DMA_RX_BDP;
}
static int copy_frame(uint8_t *dst, const uint8_t *src, size_t len) {
    size_t i;
    if (!dst || !src) return -1;
    for (i = 0U; i < len; ++i) dst[i] = src[i];
    return 0;
}
int ou_dma_prepare(ou_dma_queue *q, ou_dma_bd *bd, uint32_t bd_bus,
                   uint8_t *data, uint32_t data_bus, uint16_t capacity,
                   uint8_t direction_tx) {
    uint32_t bd_end, data_end;
    if (!q) return -1;
    *q = (ou_dma_queue){0};
    if (!bd || !data || direction_tx > 1U ||
        capacity < OU_DMA_MIN_FRAME || capacity > OU_DMA_MAX_FRAME ||
        bd_bus == 0U || (bd_bus & 1023U) != 0U ||
        data_bus == 0U || (data_bus & 3U) != 0U ||
        data_bus > OU_DMA_BD_ADDR_MASK ||
        bd_bus > OU_DMA_BD_ADDR_MASK - 8U ||
        data_bus > OU_DMA_BD_ADDR_MASK - (uint32_t)capacity) return -1;
    bd_end = bd_bus + 8U;
    data_end = data_bus + (uint32_t)capacity;
    if (bd_bus < data_end && data_bus < bd_end) return -1;
    q->bd = bd;
    q->buffer = data;
    q->bd_bus = bd_bus;
    q->buffer_bus = data_bus;
    q->buffer_capacity = capacity;
    q->direction_tx = direction_tx;
    q->prepared = 1U;
    bd->address_flags = OU_DMA_BD_WRAP |
        (direction_tx ? (OU_DMA_BD_INTERRUPT | OU_DMA_BD_LAST) : 0U) |
        data_bus;
    bd->status_length = direction_tx ? 0U : (uint32_t)capacity;
    return 0;
}
int ou_dma_tx_copy(ou_dma_queue *q, const uint8_t *frame, size_t len) {
    if (!q || !q->prepared || !q->direction_tx || q->active ||
        q->data_pending || q->faulted || !frame ||
        len < OU_DMA_MIN_FRAME || len > q->buffer_capacity) return -1;
    if (copy_frame(q->buffer, frame, len) != 0) return -1;
    q->bd->status_length = OU_DMA_BD_FULL | (uint32_t)len;
    q->data_pending = 1U;
    return 0;
}
int ou_dma_start(const ou_dma_bus *bus, ou_dma_queue *q) {
    uint32_t existing;
    uint32_t mode;
    if (!valid_bus(bus) || !q || !q->prepared || q->active || q->faulted ||
        (q->direction_tx && !q->data_pending) ||
        (!q->direction_tx && q->data_pending)) return -1;
    if (bus->read32(bus->context, control(q), &existing) != 0 ||
        (existing & OU_DMA_CONTROL_ENABLE) != 0U) return -1;
    if (q->direction_tx && synced(bus,q->buffer_bus,
        (size_t)(q->bd->status_length & OU_DMA_BD_LENGTH_MASK),1U) != 0) return -1;
    if (synced(bus,q->bd_bus,sizeof(ou_dma_bd),1U) != 0) return -1;
    if (bus->write32(bus->context,pointer(q),q->bd_bus) != 0) {
        q->faulted = 1U; return -1;
    }
    mode = q->direction_tx ? OU_DMA_CONTROL_TX_MODE : 0U;
    if (bus->write32(bus->context,control(q),mode) != 0) {
        q->faulted = 1U; return -1;
    }
    if (bus->write32(bus->context,control(q),mode | OU_DMA_CONTROL_ENABLE) != 0) {
        q->faulted = 1U; return -1;
    }
    q->active = 1U;
    return 0;
}
int ou_dma_rx_poll(const ou_dma_bus *bus, ou_dma_queue *q,
                   const uint8_t **frame, size_t *length) {
    uint32_t status;
    uint16_t flags;
    size_t frame_bytes;
    if (!frame || !length) return -1;
    *frame = 0; *length = 0U;
    if (!valid_bus(bus) || !q || !q->prepared || q->direction_tx ||
        !q->active || q->faulted) return -1;
    if (q->data_pending) {
        *frame = q->buffer;
        *length = (size_t)(q->bd->status_length & OU_DMA_BD_LENGTH_MASK);
        return 1;
    }
    if (synced(bus,q->bd_bus,sizeof(ou_dma_bd),0U) != 0) {
        q->faulted = 1U; return -1;
    }
    status = q->bd->status_length;
    if ((status & OU_DMA_BD_FULL) == 0U) return 0;
    flags = (uint16_t)(status >> 16);
    frame_bytes = (size_t)(status & OU_DMA_BD_LENGTH_MASK);
    if (frame_bytes < OU_DMA_MIN_FRAME || frame_bytes > q->buffer_capacity ||
        (flags & OU_DMA_RX_OK) == 0U ||
        (flags & OU_DMA_RX_ERROR_MASK) != 0U) {
        q->faulted = 1U; return -1;
    }
    if (synced(bus,q->buffer_bus,frame_bytes,0U) != 0) {
        q->faulted = 1U; return -1;
    }
    q->data_pending = 1U;
    *frame = q->buffer;
    *length = frame_bytes;
    return 1;
}
int ou_dma_rx_release(const ou_dma_bus *bus, ou_dma_queue *q) {
    if (!valid_bus(bus) || !q || !q->prepared || q->direction_tx ||
        !q->active || !q->data_pending || q->faulted) return -1;
    q->bd->status_length = (uint32_t)q->buffer_capacity;
    if (synced(bus,q->bd_bus,sizeof(ou_dma_bd),1U) != 0) {
        q->faulted = 1U; return -1;
    }
    q->data_pending = 0U;
    return 0;
}
int ou_dma_tx_poll(const ou_dma_bus *bus, ou_dma_queue *q) {
    uint32_t status;
    uint16_t flags;
    uint32_t ctl;
    if (!valid_bus(bus) || !q || !q->prepared || !q->direction_tx ||
        !q->active || !q->data_pending || q->faulted) return -1;
    if (synced(bus,q->bd_bus,sizeof(ou_dma_bd),0U) != 0) {
        q->faulted = 1U; return -1;
    }
    status = q->bd->status_length;
    if (status & OU_DMA_BD_FULL) return 0;
    flags = (uint16_t)(status >> 16);
    ctl = control(q);
    if (bus->write32(bus->context,ctl,OU_DMA_CONTROL_TX_MODE) != 0) {
        q->faulted = 1U; return -1;
    }
    q->active = 0U;
    q->data_pending = 0U;
    q->bd->status_length = 0U;
    return ((flags & OU_DMA_TX_OK) != 0U &&
            (flags & OU_DMA_TX_ABORT_MASK) == 0U) ? 1 : -1;
}
int ou_dma_stop(const ou_dma_bus *bus, ou_dma_queue *q) {
    if (!valid_bus(bus) || !q || !q->prepared) return -1;
    if (bus->write32(bus->context,control(q),
                     q->direction_tx ? OU_DMA_CONTROL_TX_MODE : 0U) != 0) {
        q->faulted=1U; return -1;
    }
    q->active=0U; q->data_pending=0U;
    return 0;
}
