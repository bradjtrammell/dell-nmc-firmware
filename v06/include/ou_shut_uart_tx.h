#ifndef OU_SHUT_UART_TX_H
#define OU_SHUT_UART_TX_H
#include <stddef.h>
#include <stdint.h>
#include "ou_ns7520_serial_rx.h"
#include "ou_shut_session.h"

/* Hardware byte-width FIFO write: NS7520 writes exactly one UART byte for
 * an ARM7 STRB to channel FIFO address 0xFFD00010 or 0xFFD00050.
 * Only a qualified physical H910P board owner may bind these callbacks. */
typedef int (*ou_shut_uart_write8)(void *context,uint32_t addr,uint8_t byte);
typedef int (*ou_shut_uart_write32)(void *context,uint32_t addr,uint32_t word);

typedef struct {
    ou_ns7520_serial_rx uart;
    ou_shut_session *session;
    ou_shut_uart_write8 write8;
    ou_shut_uart_write32 write32;
    void *context;
    uint8_t frame[OU_SHUT_GET_REPORT_FRAME_SIZE];
    uint8_t length;
    uint8_t offset;
    uint8_t initialized;
    uint32_t sent_bytes;
    uint32_t blocked_polls;
    uint32_t tbc_acks;
    uint32_t faults;
} ou_shut_uart_tx;

/* Attestation is a caller-supplied hardware BSP assertion, NOT proof of
 * installed wiring, timing, voltage or recovery. The live H910P BSP is
 * intentionally unqualified and cannot bind this interface. */
int ou_shut_uart_tx_bind(ou_shut_uart_tx *tx,
                         const ou_ns7520_serial_rx *qualified_rx,
                         ou_shut_session *session,
                         ou_shut_uart_write8 write8,
                         ou_shut_uart_write32 write32,
                         void *context,
                         uint8_t tx_pin_verified,
                         uint8_t bit_rate_verified,
                         uint8_t read_only_shut_protocol_verified);

/* One bounded transaction step, no wait loops and at most ONE STRB.
 * Frames can be only SYNC=0x16, ACK=0x06, or validated 11-byte
 * HID Feature GET REPORT (A1 01); never SET REPORT or UPS control.
 * The SHUT session enters RX state only after the last byte succeeds.
 * 1=byte written, 0=no data or TRDY off, -1=terminal failure.
 * Status TBC acknowledgement writes ONLY bit 1 (0x2) to Status A. */
int ou_shut_uart_tx_step(ou_shut_uart_tx *tx,uint32_t now_ms);

/* Permanently revoke until independently re-bound after failure. */
void ou_shut_uart_tx_revoke(ou_shut_uart_tx *tx);

/* Raw primitive for verified big-endian ARM7 BSP use ONLY, restricted
 * to the two exact NS7520 UART FIFO addresses. Refuses host execution. */
int ou_ns7520_arm_uart_fifo_write8(void *context,uint32_t addr,uint8_t byte);
#endif
