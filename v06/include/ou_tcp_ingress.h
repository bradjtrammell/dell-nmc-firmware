#ifndef OU_TCP_INGRESS_H
#define OU_TCP_INGRESS_H
#include <stddef.h>
#include <stdint.h>
#include "ou_vlan.h"
#define OU_TCP_HTTP_PORT 80U
#define OU_TCP_IPV4_PROTOCOL 6U
typedef struct {
    ou_vlan_rx_context vlan;
    uint8_t source_mac[6];
    uint8_t source_ipv4[4];
    uint8_t destination_ipv4[4];
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t advertised_window;
    uint32_t sequence;
    uint32_t acknowledgment;
    uint8_t flags;
    const uint8_t *payload; /* Borrowed DMA buffer; copy before RX release. */
    size_t payload_length;
} ou_tcp_ingress;
/* 1: validated TCP segment with ingress VLAN HTTP-read ACL permission.
 * 0: unrecognized/denied wire packet. -1: invalid API arguments.
 * This is not a TCP connection handler. Source CIDRs are NOT authentication. */
int ou_tcp_ingress_check(const ou_vlan_table *vlans, const uint8_t *frame,
                         size_t length, ou_tcp_ingress *result);
#endif
