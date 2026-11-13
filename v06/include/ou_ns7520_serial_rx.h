#ifndef OU_NS7520_SERIAL_RX_H
#define OU_NS7520_SERIAL_RX_H

#include <stddef.h>
#include <stdint.h>

/* NS7520 Rev G serial controller: channel A/B Status A + FIFO registers.
 * This is receive-only. It NEVER configures clocks, pinmux, DMA, interrupts,
 * control registers, FIFO writes, Settings jack, or UPS power outputs.
 * The H910P's internal UPS channel and electrical wiring are unverified. */
#define OU_NS7520_SER1_STATUS_A UINT32_C(0xffd00008)
#define OU_NS7520_SER1_FIFO UINT32_C(0xffd00010)
#define OU_NS7520_SER2_STATUS_A UINT32_C(0xffd00048)
#define OU_NS7520_SER2_FIFO UINT32_C(0xffd00050)
#define OU_NS7520_SER_RX_READY UINT32_C(0x00000800)
#define OU_NS7520_SER_RX_BUFFER_CLOSED UINT32_C(0x00000200)
#define OU_NS7520_SER_RX_ERRORS UINT32_C(0x0000f000)

typedef int (*ou_ns7520_serial_read32)(void *context, uint32_t address,
                                        uint32_t *value);
typedef struct {
    ou_ns7520_serial_read32 read32;
    void *context;
    uint8_t channel; /* Exactly 1 or 2, preselected by a qualified BSP. */
    uint8_t peripheral_mapping_verified;
    uint8_t uart_fifo_mode_verified; /* UART, polling, not DMA or SPI. */
    uint8_t receive_byte_order_verified; /* First byte in high word lane. */
    uint8_t exclusive_channel_ownership;
    uint8_t ups_physical_wiring_verified;
} ou_ns7520_serial_rx;

/* Zero, no FIFO data; positive, 1..4 bytes; -1, invalid/error.
 * The first callback reads Status A; only when RRDY and no error exists may
 * it read the FIFO once. No serial MMIO writes are possible in this module.
 * Output is unchanged and *written=0 on refusal or callback failure. */
int ou_ns7520_serial_rx_poll(const ou_ns7520_serial_rx *source,
                              uint8_t *output, size_t capacity,
                              size_t *written);

/* CPU read primitive: same qualified read implementation as the existing
 * NS7520 HAL, but this wrapper permits ONLY Status A and FIFO of the
 * independently chosen channel. It cannot access other SoC registers. */
int ou_ns7520_serial_rx_read_whitelisted(const ou_ns7520_serial_rx *source,
                                          uint32_t address, uint32_t *value);
#endif
