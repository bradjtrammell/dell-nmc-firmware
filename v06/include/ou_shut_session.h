#ifndef OU_SHUT_SESSION_H
#define OU_SHUT_SESSION_H
#include <stddef.h>
#include <stdint.h>
#include "ou_shut_rx.h"
#include "ou_shut_request.h"

/* SHUT polling state machine. Pure input/output framing only, NO UART driver,
 * registers, DMA, persistent writes, or power-changing HID operations.
 * Serial Settings RJ45 is NOT presumed to be the internal SHUT bus.
 * The caller must independently qualify electrical transport and protocol. */
#define OU_SHUT_SESSION_SYNC_INTERVAL_MS UINT32_C(600)
#define OU_SHUT_SESSION_ACK_TIMEOUT_MS UINT32_C(500)
#define OU_SHUT_SESSION_RESPONSE_TIMEOUT_MS UINT32_C(1500)
#define OU_SHUT_SESSION_ATTEMPTS 4U
#define OU_SHUT_SESSION_REQUEST_ATTEMPTS 3U

#define OU_SHUT_SESSION_IDLE 0U
#define OU_SHUT_SESSION_SYNC_READY 1U
#define OU_SHUT_SESSION_SYNC_WAIT 2U
#define OU_SHUT_SESSION_REQUEST_READY 3U
#define OU_SHUT_SESSION_REQUEST_WAIT 4U
#define OU_SHUT_SESSION_RESPONSE_WAIT 5U
#define OU_SHUT_SESSION_ACK_READY 6U
#define OU_SHUT_SESSION_COMPLETE 7U
#define OU_SHUT_SESSION_FAILED 8U

/* Error codes preserved after fail-closed state changes. */
#define OU_SHUT_SESSION_ERR_NONE 0U
#define OU_SHUT_SESSION_ERR_CLOCK 1U
#define OU_SHUT_SESSION_ERR_TIMEOUT 2U
#define OU_SHUT_SESSION_ERR_BAD_PACKET 3U
#define OU_SHUT_SESSION_ERR_REPORT_MISMATCH 4U
#define OU_SHUT_SESSION_ERR_NAK 5U

typedef struct {
    ou_shut_rx rx;
    ou_link_observer observer;
    ou_shut_rx_message result;
    uint8_t out[OU_SHUT_GET_REPORT_FRAME_SIZE];
    uint32_t last_ms;
    uint32_t sent_ms;
    uint32_t queued_ms;
    uint32_t successful_reads;
    uint32_t failures;
    uint16_t requested_bytes;
    uint8_t report_id;
    uint8_t state;
    uint8_t after_ack;
    uint8_t out_length;
    uint8_t sync_attempts;
    uint8_t request_attempts;
    uint8_t error;
    uint8_t started;
} ou_shut_session;

void ou_shut_session_init(ou_shut_session *s);
/* Electrical mapping and protocol qualification must BOTH be exactly 1.
 * These caller assertions do not prove the H910P hardware. They only guard
 * accidental use of research code. The function does not send any bytes. */
int ou_shut_session_begin(ou_shut_session *s, uint8_t report_id,
                          uint16_t requested_bytes, uint32_t now_ms,
                          uint8_t physical_link_verified,
                          uint8_t protocol_verified);
/* Return a complete outbound frame to a separately qualified UART owner.
 * 1=frame available, 0=nothing, -1=invalid or non-monotonic clock.
 * Only SYNC, feature GET REPORT, and per-packet ACK can be emitted. */
int ou_shut_session_take_tx(ou_shut_session *s, uint32_t now_ms,
                            uint8_t *frame, size_t capacity, size_t *written);
/* Consume one received byte and schedule an ACK after verified SHUT framing.
 * Complete reports stay unverified until HID semantics/UPS identity validated. */
int ou_shut_session_feed(ou_shut_session *s, uint8_t byte, uint32_t now_ms);
/* Drive bounded synchronization/retransmission and response timeout. */
int ou_shut_session_tick(ou_shut_session *s, uint32_t now_ms);
/* Copy one completed, matched GET REPORT reply. 1=result, 0=pending,
 * -1=invalid. Successful read clears the stored result (no replay). */
int ou_shut_session_take_report(ou_shut_session *s, ou_shut_rx_message *result);
#endif
