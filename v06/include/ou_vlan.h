#ifndef OU_VLAN_H
#define OU_VLAN_H
#include <stddef.h>
#include <stdint.h>
#include "ou_ipv4_wire.h"

#define OU_VLAN_MAX_INTERFACES 64U
#define OU_VLAN_ACL_MAX 8U
#define OU_VLAN_TPID UINT16_C(0x8100)
#define OU_VLAN_TPID_QINQ UINT16_C(0x88a8)
#define OU_VLAN_TPID_QINQ_ALT UINT16_C(0x9100)
#define OU_VLAN_ID_MAX 4094U
#define OU_VLAN_MAX_UNTAGGED 1514U
#define OU_VLAN_MAX_TAGGED 1518U
#define OU_VLAN_FRAME_MIN_UNTAGGED 60U
#define OU_VLAN_FRAME_MIN_TAGGED 64U
#define OU_VLAN_SERVICE_ARP UINT32_C(0x0001)
#define OU_VLAN_SERVICE_PING UINT32_C(0x0002)
#define OU_VLAN_SERVICE_HTTP_READ UINT32_C(0x0004)
#define OU_VLAN_SERVICE_SNMP UINT32_C(0x0008)
#define OU_VLAN_SERVICE_CONTROL UINT32_C(0x0010)
#define OU_VLAN_SERVICE_UPDATES UINT32_C(0x0020)
#define OU_VLAN_SERVICE_ALL UINT32_C(0x003f)

typedef struct {
    uint8_t address[4];
    uint8_t prefix_length;
} ou_vlan_cidr;

typedef struct {
    uint16_t vid;
    uint8_t ipv4[4];
    uint8_t prefix_length;
    uint8_t gateway[4];
    uint32_t allowed_services;
    uint8_t tx_pcp;
    uint8_t allow_dei;
    uint8_t allow_local_subnet;
    uint8_t acl_count;
    ou_vlan_cidr acl[OU_VLAN_ACL_MAX];
} ou_vlan_interface;

typedef struct {
    uint32_t received;
    uint32_t denied;
    uint32_t answered;
} ou_vlan_interface_counters;

typedef struct {
    uint8_t mac[6];
    uint8_t trunk_enabled;
    uint8_t native_enabled;
    uint16_t native_vid;
    uint8_t accept_tagged_native;
    uint8_t accept_priority_tags;
    uint8_t priority_reply_tagged;
    uint8_t count;
    ou_vlan_interface interfaces[OU_VLAN_MAX_INTERFACES];
    ou_vlan_interface_counters statistics[OU_VLAN_MAX_INTERFACES];
    uint32_t received;
    uint32_t matched;
    uint32_t denied;
    uint32_t replied;
} ou_vlan_table;

typedef struct {
    uint16_t vlan_id;
    uint8_t tagged;
    uint8_t ingress_pcp;
    uint8_t ingress_dei;
    uint16_t inner_ether_type;
    uint8_t interface_index;
} ou_vlan_rx_context;

/* Transactional installation. Failure clears output, no partial config.
 * The input entries must not alias the output table. */
int ou_vlan_configure(ou_vlan_table *table, const uint8_t mac[6],
                      const ou_vlan_interface *interfaces, size_t count,
                      uint16_t native_vid, uint8_t trunk_enabled,
                      uint8_t accept_tagged_native, uint8_t accept_priority_tags,
                      uint8_t priority_reply_tagged);
/* 1=matched, 0=drop, -1=argument error. Unknown VLANs, QinQ, double tags
 * and VLAN ID 4095 are always rejected. */
int ou_vlan_classify(const ou_vlan_table *table, const uint8_t *frame,
                     size_t length, ou_vlan_rx_context *context);
/* Per-service/per-source ACL for services. Does not replace authentication.
 * The caller must derive vlan from ou_vlan_classify, not user input. */
int ou_vlan_authorize(const ou_vlan_table *table, uint16_t vlan,
                      const uint8_t source_ipv4[4], uint32_t service);
/* 1=ARP or ICMP reply, 0=drop/ignored, -1=invalid arguments.
 * Caller owns buffers, no heap or hardware access. */
int ou_vlan_reply(ou_vlan_table *table, const uint8_t *frame,
                  size_t length, uint8_t *response, size_t capacity,
                  size_t *written);
#endif
