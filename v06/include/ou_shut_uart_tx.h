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
    uint8_t waiting_for_empty;
    uint16_t character_guard_ms;
    uint32_t last_fifo_write_ms;
    uint32_t empty_seen_ms;
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
                         uint8_t read_only_shut_protocol_verified,
                         uint32_t measured_baud);

/* One bounded transaction step, no wait loops and at most ONE STRB.
 * Frames can be only SYNC=0x16, ACK=0x06, or validated 11-byte
 * HID Feature GET REPORT (A1 01); never SET REPORT or UPS control.
 * Per NS7520 Errata Rev L p.14, TXBC/TBC is NOT a reliable completion
 * indication. Require actual TXEMPTY and at least 11 bit-times between
 * byte writes; after the final byte observe TXEMPTY then allow a further
 * full character time before committing SHUT state. A verified real baud
 * rate must be supplied by the qualified H910P board port.
 * 1=one FIFO byte written, 0=pending/no data, -1=terminal failure.
 * Unexpected TBC is acknowledged via its exact W1C bit but never treated
 * as proof that transmission completed. */
int ou_shut_uart_tx_step(ou_shut_uart_tx *tx,uint32_t now_ms);

/* Permanently revoke until independently re-bound after failure. */
void ou_shut_uart_tx_revoke(ou_shut_uart_tx *tx);

/* Raw primitive for verified big-endian ARM7 BSP use ONLY, restricted
 * to the two exact NS7520 UART FIFO addresses. Refuses host execution. */
int ou_ns7520_arm_uart_fifo_write8(void *context,uint32_t addr,uint8_t byte);
#endif
