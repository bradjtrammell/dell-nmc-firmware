#ifndef OU_IPV4_WIRE_H
#define OU_IPV4_WIRE_H
#include <stddef.h>
#include <stdint.h>
#define OU_NET_MTU 1500U
#define OU_NET_MAX_FRAME 1518U
#define OU_NET_MIN_FRAME 60U
typedef struct {
    uint8_t mac[6];
    uint8_t ipv4[4];
    uint8_t enabled;
} ou_net_identity;
/* Stateless, bounded Ethernet frame processing. Frames enter with FCS removed;
 * hardware TX must append FCS. No socket, heap, flash or UPS control. */
int ou_net_reply(const ou_net_identity *identity, const uint8_t *frame,
                 size_t frame_len, uint8_t *reply, size_t capacity,
                 size_t *reply_len);
#endif
