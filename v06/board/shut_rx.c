#include "ou_shut_rx.h"

static void zero_bytes(uint8_t *dest, size_t count) {
    size_t i;
    for (i = 0U; i < count; ++i) dest[i] = 0U;
}
static void count_up(uint32_t *value) {
    if (*value != UINT32_MAX) ++*value;
}
static void reset_partial(ou_shut_rx *decoder) {
    zero_bytes(decoder->message, sizeof(decoder->message));
    zero_bytes(decoder->frame, sizeof(decoder->frame));
    decoder->assembled = 0U;
    decoder->message_type = 0U;
    decoder->frame_type = 0U;
    decoder->expected = 0U;
    decoder->received = 0U;
    decoder->stage = 0U;
    decoder->active = 0U;
}
void ou_shut_rx_init(ou_shut_rx *decoder) {
    if (decoder) *decoder = (ou_shut_rx){0};
}
int ou_shut_rx_expire(ou_shut_rx *decoder, uint32_t now_ms) {
    uint32_t elapsed;
    if (!decoder) return -1;
    if (!decoder->active) return 0;
    elapsed = now_ms - decoder->last_ms;
    if (elapsed <= OU_SHUT_RX_TIMEOUT_MS &&
        elapsed <= UINT32_C(0x7fffffff)) return 0;
    count_up(&decoder->timeouts);
    reset_partial(decoder);
    return 1;
}
int ou_shut_rx_feed(ou_shut_rx *decoder, uint8_t byte, uint32_t now_ms,
                    ou_link_observer *observer, ou_shut_rx_message *output) {
    uint8_t packet_kind, at_end, checksum;
    size_t i;
    if (!decoder || !output) return -1;
    *output = (ou_shut_rx_message){0};
    /* No response is ever accepted from a non-monotonic clock. */
    if (decoder->started &&
        (uint32_t)(now_ms - decoder->last_ms) > UINT32_C(0x7fffffff)) {
        count_up(&decoder->invalid_packets);
        reset_partial(decoder);
        decoder->last_ms = now_ms;
        return -1;
    }
    if (ou_shut_rx_expire(decoder, now_ms) < 0) return -1;
    decoder->last_ms = now_ms;
    decoder->started = 1U;
    if (decoder->stage == 0U) {
        if (byte == 0x06U) { count_up(&decoder->ack_count); return 0; }
        if (byte == 0x15U) {
            count_up(&decoder->nak_count);
            if (decoder->active) reset_partial(decoder);
            return 0;
        }
        if (byte == 0x16U) {
            count_up(&decoder->sync_count);
            if (decoder->active) reset_partial(decoder);
            return 0;
        }
        packet_kind = (uint8_t)(byte & 0x7fU);
        if (packet_kind != OU_SHUT_RX_RESPONSE &&
            packet_kind != OU_SHUT_RX_NOTIFY) {
            count_up(&decoder->invalid_packets);
            reset_partial(decoder);
            return -1;
        }
        if (decoder->active && decoder->message_type != packet_kind) {
            count_up(&decoder->invalid_packets);
            reset_partial(decoder);
            return -1;
        }
        decoder->frame_type = byte;
        decoder->message_type = packet_kind;
        decoder->active = 1U;
        decoder->stage = 1U;
        return 0;
    }
    if (decoder->stage == 1U) {
        packet_kind = (uint8_t)(byte & 0x0fU);
        if (packet_kind < 1U || packet_kind > OU_SHUT_RX_PACKET_DATA_MAX ||
            (byte >> 4U) != packet_kind) {
            count_up(&decoder->invalid_packets);
            reset_partial(decoder);
            return -1;
        }
        decoder->expected = packet_kind;
        decoder->received = 0U;
        decoder->stage = 2U;
        return 0;
    }
    if (decoder->stage == 2U) {
        decoder->frame[decoder->received++] = byte;
        if (decoder->received == decoder->expected) decoder->stage = 3U;
        return 0;
    }
    if (decoder->stage != 3U) {
        count_up(&decoder->invalid_packets);
        reset_partial(decoder);
        return -1;
    }
    checksum = 0U;
    for (i = 0U; i < decoder->expected; ++i) checksum ^= decoder->frame[i];
    if (checksum != byte ||
        (size_t)decoder->assembled + decoder->expected > OU_SHUT_RX_MESSAGE_CAP) {
        count_up(&decoder->invalid_packets);
        reset_partial(decoder);
        return -1;
    }
    for (i = 0U; i < decoder->expected; ++i)
        decoder->message[decoder->assembled + i] = decoder->frame[i];
    decoder->assembled = (uint16_t)(decoder->assembled + decoder->expected);
    at_end = (uint8_t)((decoder->frame_type & 0x80U) != 0U);
    decoder->stage = 0U;
    decoder->received = 0U;
    decoder->expected = 0U;
    zero_bytes(decoder->frame, sizeof(decoder->frame));
    count_up(&decoder->accepted_packets);
    if (!at_end) return 0;
    output->length = decoder->assembled;
    output->packet_type = decoder->message_type;
    output->telemetry_verified = 0U;
    output->monotonic_ms = now_ms;
    for (i = 0U; i < decoder->assembled; ++i)
        output->payload[i] = decoder->message[i];
    if (observer && ou_link_observe(observer, OU_LINK_CHANNEL_OTHER,
                     output->payload, output->length, now_ms) != 0) {
        *output = (ou_shut_rx_message){0};
        count_up(&decoder->invalid_packets);
        reset_partial(decoder);
        return -1;
    }
    count_up(&decoder->completed_messages);
    reset_partial(decoder);
    return 1;
}
