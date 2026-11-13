#ifndef OU_SNMP_H
#define OU_SNMP_H
#include <stddef.h>
#include <stdint.h>
#include "openh910p.h"

/* Standalone, bounded, read-only RFC 3416 SNMPv2c GET/GETNEXT/GETBULK layer.
 * Never bind UDP/161 until trusted-source ACL and preprovisioned community
 * verification exist. SNMPv3 USM authPriv remains separately required. */
#define OU_SNMP_MAX_PACKET 768U
#define OU_SNMP_MAX_VARBINDS 8U
#define OU_SNMP_MAX_COMMUNITY 32U
#define OU_SNMP_OID_COUNT 7U
typedef int (*ou_snmp_authorize_fn)(void *ctx,const uint8_t *community,
                                    size_t len);
typedef struct {
    ou_snmp_authorize_fn authorize;
    void *context;
    uint8_t source_acl_qualified;
    uint8_t community_store_qualified;
    uint8_t isolated_network_qualified;
} ou_snmp_access;
/* Return 0 with a wire response, 1 for an explicitly denied/dropped
 * request, -1 on malformed protocol, packet overflow or invalid params.
 * No data is disclosed on authorization failure or missing native data. */
int ou_snmp_respond(const ou_snmp_access *access,
                    const ou_snapshot *telemetry,
                    const uint8_t *request,size_t request_len,
                    uint8_t *response,size_t response_capacity,
                    size_t *written);
#endif
