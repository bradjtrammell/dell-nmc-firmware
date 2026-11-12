#ifndef OU_SHUT_RX_H
#define OU_SHUT_RX_H

#include <stddef.h>
#include <stdint.h>
#include "ou_ups_link.h"

/* SHUT (Serial HID UPS Transfer), candidate UPS RX transport only.
 * These wire definitions are supported by MGE 51029473ZA Rev AC, section 2.3.
 * They DO NOT prove the Dell H910P's internal electrical/bus wiring. */
#define OU_SHUT_RX_MESSAGE_CAP OU_LINK_MAX_REPORT_BYTES
#define OU_SHUT_RX_PACKET_DATA_MAX 8U
#define OU_SHUT_RX_TIMEOUT_MS UINT32_C(750)
#define OU_SHUT_RX_RESPONSE 4U
#define OU_SHUT_RX_NOTIFY 5U

typedef struct {
    uint8_t payload[OU_SHUT_RX_MESSAGE_CAP];
    uint16_t length;
    uint8_t packet_type; /* 4 RESPONSE or 5 NOTIFY. */
    uint8_t telemetry_verified; /* Always zero: framing is not HID validation. */
    uint32_t monotonic_ms;
} ou_shut_rx_message;

typedef struct {
    uint8_t message[OU_SHUT_RX_MESSAGE_CAP];
    uint8_t frame[OU_SHUT_RX_PACKET_DATA_MAX];
    uint32_t last_ms;
    uint32_t accepted_packets;
    uint32_t completed_messages;
    uint32_t invalid_packets;
    uint32_t timeouts;
    uint32_t ack_count;
    uint32_t nak_count;
    uint32_t sync_count;
    uint16_t assembled;
    uint8_t message_type;
    uint8_t frame_type;
    uint8_t expected;
    uint8_t received;
    uint8_t stage; /* 0 type, 1 length, 2 bytes, 3 XOR checksum */
    uint8_t active;
    uint8_t started;
} ou_shut_rx;

void ou_shut_rx_init(ou_shut_rx *decoder);
/* Consume one byte from a separately verified *receive-only* UPS adapter.
 * +1 = completed, checksum-correct opaque report; 0 = incomplete/control;
 * -1 = malformed, timeout or invalid arguments. On success, output is a
 * copy; RX context retains no assembled raw message between reports.
 * This implementation performs NO transmit, ACK, polling or UPS control. */
int ou_shut_rx_feed(ou_shut_rx *decoder, uint8_t byte, uint32_t now_ms,
                    ou_link_observer *observer, ou_shut_rx_message *output);
/* 1 means stale partial message was discarded, 0 nothing pending, -1 invalid.
 * Poll even in the absence of UART input to bound stale assembly state. */
int ou_shut_rx_expire(ou_shut_rx *decoder, uint32_t now_ms);
#endif
