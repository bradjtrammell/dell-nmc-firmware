#ifndef OU_SHUT_UART_LINK_H
#define OU_SHUT_UART_LINK_H
#include "ou_shut_uart_bridge.h"
#include "ou_shut_uart_tx.h"

/* Single-owner, nonblocking, receive-only SHUT GET REPORT transaction
 * over a separately qualified NS7520 UART channel. No power commands.
 * Real installed-card bus/baud/boot recovery have NOT been qualified. */
typedef struct {
    ou_shut_session session;
    ou_shut_uart_bridge rx;
    ou_shut_uart_tx tx;
    uint32_t polls;
    uint32_t reports_completed;
    uint32_t io_failures;
    uint8_t initialized;
    uint8_t counted_complete;
} ou_shut_uart_link;

/* Physical callbacks and attestations MUST come from independently
 * qualified board-specific hardware code. Caller flags alone do not
 * prove electrical wiring or the actual Dell UPS protocol. */
int ou_shut_uart_link_bind(ou_shut_uart_link *link,
                           const ou_ns7520_serial_rx *source,
                           ou_shut_uart_write8 write8,
                           ou_shut_uart_write32 write32,
                           void *write_context,
                           uint8_t tx_pin_verified,
                           uint8_t bit_rate_verified,
                           uint8_t shut_protocol_verified,
                           uint32_t measured_baud);

/* Start precisely one read-only HID Feature GET REPORT transaction;
 * refuse reentrancy or unconsumed results. */
int ou_shut_uart_link_begin(ou_shut_uart_link *link,
                            uint8_t report_id,uint16_t length,
                            uint32_t now_ms);

/* One bounded cooperative event-loop step. TX is always serviced before
 * RX to give mandatory SHUT ACKs priority. No loops, sleeps, DMA, flash,
 * or configuration changes. 1=progress, 0=idle, -1=link revoked. */
int ou_shut_uart_link_step(ou_shut_uart_link *link,uint32_t now_ms);

/* Read a completed response exactly once; keeps telemetry_verified=0
 * until HID descriptor mapping and independently verified source identity.
 * 1=message, 0=not yet available, -1=invalid or revoked. */
int ou_shut_uart_link_take_report(ou_shut_uart_link *link,
                                  ou_shut_rx_message *out);

/* Destructive only to in-memory transaction state: abandon any partial
 * read and revoke all hardware callbacks on link loss. */
void ou_shut_uart_link_revoke(ou_shut_uart_link *link);
#endif
