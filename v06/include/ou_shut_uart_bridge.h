#ifndef OU_SHUT_UART_BRIDGE_H
#define OU_SHUT_UART_BRIDGE_H

#include "ou_ns7520_serial_rx.h"
#include "ou_shut_session.h"

/* Read-only adapter: NS7520 physical FIFO -> existing SHUT parser/session.
 * One verified FIFO word can contain bytes past a SHUT packet boundary.
 * Preserve them until the caller finishes the ACK/GET REPORT transaction.
 * No TX, power operations, UART config writes, or hidden FIFO reads. */
typedef struct {
    ou_ns7520_serial_rx uart;
    ou_shut_session *session;
    uint8_t pending[4];
    uint8_t pending_count;
    uint8_t pending_offset;
    uint32_t total_bytes;
    uint32_t deferred_fifo_bytes;
    uint32_t rejected_polls;
    uint8_t initialized;
} ou_shut_uart_bridge;

/* Refuse initialization unless channel and physical-source qualifications
 * are fully supplied. The flags are promises from a future verified BSP,
 * NOT independent proof of the UPS wiring or serial protocol. */
int ou_shut_uart_bridge_init(ou_shut_uart_bridge *bridge,
                              const ou_ns7520_serial_rx *source,
                              ou_shut_session *session);

/* Consume a bounded FIFO word (up to 4 bytes), or resume previously read
 * bytes after the session queues ACK. Zero means no bytes available or a
 * queued transmission is pending; positive means bytes consumed.
 * A FIFO error invalidates the current session and staged bytes. */
int ou_shut_uart_bridge_poll(ou_shut_uart_bridge *bridge, uint32_t now_ms);

/* Explicitly discard buffered bytes on physical link loss; this does not
 * reset the SHUT session's authenticated/qualified lifetime. */
void ou_shut_uart_bridge_discard(ou_shut_uart_bridge *bridge);
#endif
