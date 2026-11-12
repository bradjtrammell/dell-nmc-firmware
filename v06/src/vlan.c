#include "ou_vlan.h"

static uint16_t u16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static void put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v;
}
static int eq(const uint8_t *a, const uint8_t *b, size_t n) {
    size_t i;
    for (i = 0U; i < n; ++i) if (a[i] != b[i]) return 0;
    return 1;
}
static int all(const uint8_t *a, uint8_t value, size_t n) {
    size_t i;
    for (i = 0U; i < n; ++i) if (a[i] != value) return 0;
    return 1;
}
static uint32_t ipv4u32(const uint8_t a[4]) {
    return ((uint32_t)a[0] << 24) | ((uint32_t)a[1] << 16) |
           ((uint32_t)a[2] << 8) | a[3];
}
static uint32_t mask(uint8_t prefix) {
    if (prefix == 0U) return 0U;
    if (prefix == 32U) return UINT32_MAX;
    return UINT32_MAX << (32U - prefix);
}
static int ip_valid(const uint8_t ip[4]) {
    return ip[0] != 0U && ip[0] != 127U && ip[0] < 224U &&
           !all(ip, 255U, 4U);
}
static int cidr_match(const uint8_t ip[4], const uint8_t base[4],
                      uint8_t prefix) {
    if (prefix == 0U || prefix > 32U) return 0;
    return (ipv4u32(ip) & mask(prefix)) ==
           (ipv4u32(base) & mask(prefix));
}
static int interface_valid(const ou_vlan_interface *c) {
    size_t i;
    if (!c || c->vid == 0U || c->vid > OU_VLAN_ID_MAX ||
        !ip_valid(c->ipv4) || c->prefix_length < 1U ||
        c->prefix_length > 32U || c->tx_pcp > 7U || c->allow_dei > 1U ||
        c->allow_local_subnet > 1U || c->acl_count > OU_VLAN_ACL_MAX ||
        (c->allowed_services & ~OU_VLAN_SERVICE_ALL) != 0U ||
        ((c->allowed_services & OU_VLAN_SERVICE_PING) != 0U &&
         (c->allowed_services & OU_VLAN_SERVICE_ARP) == 0U)) return 0;
    if (!all(c->gateway, 0U, 4U)) {
        if (!ip_valid(c->gateway) ||
            !cidr_match(c->gateway, c->ipv4, c->prefix_length) ||
            eq(c->gateway, c->ipv4, 4U)) return 0;
    }
    if (c->prefix_length < 31U) {
        uint32_t hostmask = ~mask(c->prefix_length);
        uint32_t host = ipv4u32(c->ipv4) & hostmask;
        if (host == 0U || host == hostmask) return 0;
    }
    for (i = 0U; i < c->acl_count; ++i) {
        const ou_vlan_cidr *a = &c->acl[i];
        if (a->prefix_length == 0U || a->prefix_length > 32U ||
            !ip_valid(a->address) ||
            (ipv4u32(a->address) & ~mask(a->prefix_length)) != 0U)
            return 0;
    }
    return 1;
}
static int find(const ou_vlan_table *t, uint16_t vid) {
    size_t i;
    for (i = 0U; i < t->count; ++i)
        if (t->interfaces[i].vid == vid) return (int)i;
    return -1;
}
static void inc(uint32_t *value) {
    if (*value != UINT32_MAX) ++*value;
}
int ou_vlan_configure(ou_vlan_table *table, const uint8_t mac[6],
                      const ou_vlan_interface *interfaces, size_t count,
                      uint16_t native_vid, uint8_t trunk_enabled,
                      uint8_t accept_tagged_native, uint8_t accept_priority_tags,
                      uint8_t priority_reply_tagged) {
    size_t i, j;
    if (!table) return -1;
    *table = (ou_vlan_table){0};
    if (!mac || !interfaces || count == 0U ||
        count > OU_VLAN_MAX_INTERFACES || trunk_enabled > 1U ||
        accept_tagged_native > 1U || accept_priority_tags > 1U ||
        priority_reply_tagged > 1U || (mac[0] & 1U) ||
        all(mac, 0U, 6U) || all(mac, 255U, 6U) ||
        native_vid > OU_VLAN_ID_MAX ||
        (native_vid == 0U && (accept_tagged_native || accept_priority_tags ||
                              priority_reply_tagged))) return -1;
    for (i = 0U; i < count; ++i) {
        if (!interface_valid(&interfaces[i])) return -1;
        for (j = 0U; j < i; ++j)
            if (interfaces[j].vid == interfaces[i].vid) return -1;
    }
    if ((native_vid == 0U && !trunk_enabled) ||
        (!trunk_enabled && (count != 1U || interfaces[0].vid != native_vid)))
        return -1;
    if (native_vid != 0U) {
        int found = 0;
        for (i = 0U; i < count; ++i)
            if (interfaces[i].vid == native_vid) found = 1;
        if (!found) return -1;
    }
    for (i = 0U; i < count; ++i) table->interfaces[i] = interfaces[i];
    for (i = 0U; i < 6U; ++i) table->mac[i] = mac[i];
    table->count = (uint8_t)count;
    table->native_enabled = (uint8_t)(native_vid != 0U);
    table->native_vid = native_vid;
    table->trunk_enabled = trunk_enabled;
    table->accept_tagged_native = accept_tagged_native;
    table->accept_priority_tags = accept_priority_tags;
    table->priority_reply_tagged = priority_reply_tagged;
    return 0;
}
/* Revalidate a loaded policy before enabling any network DMA. This ignores
 * mutable per-interface statistics and never writes any hardware register. */
int ou_vlan_validate(const ou_vlan_table *table) {
    size_t i,j;
    uint8_t native_found=0U;
    if (!table || table->count==0U || table->count>OU_VLAN_MAX_INTERFACES ||
        table->trunk_enabled>1U || table->native_enabled>1U ||
        table->accept_tagged_native>1U || table->accept_priority_tags>1U ||
        table->priority_reply_tagged>1U ||
        (table->mac[0]&1U) || all(table->mac,0U,6U) ||
        all(table->mac,255U,6U) || table->native_vid>OU_VLAN_ID_MAX ||
        table->native_enabled!=(uint8_t)(table->native_vid!=0U) ||
        (table->native_vid==0U && (!table->trunk_enabled ||
           table->accept_tagged_native || table->accept_priority_tags ||
           table->priority_reply_tagged)) ||
        (!table->trunk_enabled && table->count!=1U)) return 0;
    for(i=0U;i<table->count;++i) {
        const ou_vlan_interface *it=&table->interfaces[i];
        if(!interface_valid(it))return 0;
        if(it->vid==table->native_vid)native_found=1U;
        for(j=0U;j<i;++j)
            if(it->vid==table->interfaces[j].vid)return 0;
    }
    if (table->native_enabled && !native_found)return 0;
    if (!table->trunk_enabled && table->interfaces[0].vid!=table->native_vid)
        return 0;
    return 1;
}
int ou_vlan_classify(const ou_vlan_table *table, const uint8_t *frame,
                     size_t length, ou_vlan_rx_context *context) {
    uint16_t outer, vid = 0U, tci = 0U, inner;
    int idx;
    uint8_t tagged = 0U;
    if (!context) return -1;
    *context = (ou_vlan_rx_context){0};
    if (!table || !frame || length < OU_VLAN_FRAME_MIN_UNTAGGED ||
        length > OU_VLAN_MAX_TAGGED || table->count == 0U) return -1;
    outer = u16(frame + 12U);
    if (outer == OU_VLAN_TPID_QINQ || outer == OU_VLAN_TPID_QINQ_ALT)
        return 0;
    if (outer == OU_VLAN_TPID) {
        if (!table->trunk_enabled || length < OU_VLAN_FRAME_MIN_TAGGED)
            return 0;
        tagged = 1U;
        tci = u16(frame + 14U);
        vid = tci & UINT16_C(0x0fff);
        inner = u16(frame + 16U);
        if (inner == OU_VLAN_TPID || inner == OU_VLAN_TPID_QINQ ||
            inner == OU_VLAN_TPID_QINQ_ALT || vid == UINT16_C(4095))
            return 0;
        if (vid == 0U) {
            if (!table->accept_priority_tags || !table->native_enabled)
                return 0;
            vid = table->native_vid;
        } else if (table->native_enabled && vid == table->native_vid &&
                   !table->accept_tagged_native) return 0;
    } else {
        if (!table->native_enabled || length > OU_VLAN_MAX_UNTAGGED)
            return 0;
        vid = table->native_vid;
        inner = outer;
    }
    idx = find(table, vid);
    if (idx < 0) return 0;
    if (tagged && ((tci & UINT16_C(0x1000)) != 0U) &&
        !table->interfaces[idx].allow_dei) return 0;
    context->vlan_id = vid;
    context->tagged = tagged;
    context->ingress_pcp = (uint8_t)((tci >> 13) & 7U);
    context->ingress_dei = (uint8_t)((tci >> 12) & 1U);
    context->inner_ether_type = inner;
    context->interface_index = (uint8_t)idx;
    return 1;
}
int ou_vlan_authorize(const ou_vlan_table *table, uint16_t vlan,
                      const uint8_t source_ipv4[4], uint32_t service) {
    const ou_vlan_interface *c;
    size_t i;
    int idx;
    if (!table || !source_ipv4 || service == 0U ||
        (service & (service - 1U)) != 0U ||
        (service & ~OU_VLAN_SERVICE_ALL) != 0U || !ip_valid(source_ipv4))
        return 0;
    idx = find(table, vlan);
    if (idx < 0) return 0;
    c = &table->interfaces[idx];
    if (!(c->allowed_services & service)) return 0;
    if (service == OU_VLAN_SERVICE_ARP)
        return cidr_match(source_ipv4,c->ipv4,c->prefix_length);
    if (c->allow_local_subnet &&
        cidr_match(source_ipv4, c->ipv4, c->prefix_length)) return 1;
    for (i = 0U; i < c->acl_count; ++i)
        if (cidr_match(source_ipv4, c->acl[i].address,
                       c->acl[i].prefix_length)) return 1;
    return 0;
}
int ou_vlan_reply(ou_vlan_table *table, const uint8_t *frame,
                  size_t length, uint8_t *response, size_t capacity,
                  size_t *written) {
    ou_vlan_rx_context ctx;
    ou_net_identity device;
    uint8_t stripped[OU_VLAN_MAX_UNTAGGED];
    const uint8_t *packet = stripped;
    const ou_vlan_interface *c;
    const uint8_t *src_ip;
    uint16_t service;
    size_t i, plain_len, reply_len = 0U, out_len;
    int match, result;
    uint8_t tag_output;
    if (!written) return -1;
    *written = 0U;
    if (!table || !frame || !response ||
        capacity < OU_VLAN_FRAME_MIN_UNTAGGED ||
        length < OU_VLAN_FRAME_MIN_UNTAGGED ||
        length > OU_VLAN_MAX_TAGGED) return -1;
    inc(&table->received);
    match = ou_vlan_classify(table, frame, length, &ctx);
    if (match <= 0) {
        inc(&table->denied);
        return match < 0 ? -1 : 0;
    }
    c = &table->interfaces[ctx.interface_index];
    inc(&table->statistics[ctx.interface_index].received);
    if (ctx.inner_ether_type != UINT16_C(0x0806) &&
        ctx.inner_ether_type != UINT16_C(0x0800)) return 0;
    if (ctx.tagged) {
        plain_len = length - 4U;
        if (plain_len > sizeof(stripped)) return 0;
        for (i = 0U; i < 12U; ++i) stripped[i] = frame[i];
        for (i = 12U; i < plain_len; ++i) stripped[i] = frame[i + 4U];
    } else {
        plain_len = length;
        packet = frame;
    }
    if (ctx.inner_ether_type == UINT16_C(0x0806)) {
        if (plain_len < 42U) return 0;
        service = OU_VLAN_SERVICE_ARP;
        src_ip = packet + 28U;
        if (!cidr_match(src_ip,c->ipv4,c->prefix_length)) {
            inc(&table->denied);
            inc(&table->statistics[ctx.interface_index].denied);
            return 0;
        }
    } else {
        if (plain_len < 34U) return 0;
        service = OU_VLAN_SERVICE_PING;
        src_ip = packet + 26U;
    }
    if (eq(src_ip,c->ipv4,4U) ||
        !ou_vlan_authorize(table,ctx.vlan_id,src_ip,service)) {
        inc(&table->denied);
        inc(&table->statistics[ctx.interface_index].denied);
        return 0;
    }
    inc(&table->matched);
    for (i = 0U; i < 6U; ++i) device.mac[i] = table->mac[i];
    for (i = 0U; i < 4U; ++i) device.ipv4[i] = c->ipv4[i];
    device.enabled = 1U;
    result = ou_net_reply(&device,packet,plain_len,response,capacity,&reply_len);
    if (result < 0) {
        /* Invalid on-wire source MAC or malformed packet must never strand
         * the RX DMA descriptor. A full-size output buffer distinguishes
         * packet rejection from insufficient caller storage. */
        if (capacity < OU_VLAN_MAX_TAGGED) return -1;
        inc(&table->denied);
        inc(&table->statistics[ctx.interface_index].denied);
        return 0;
    }
    if (result == 0) return 0;
    tag_output = (uint8_t)(ctx.tagged &&
        (((u16(frame+14U) & UINT16_C(0x0fff)) != 0U) ||
         table->priority_reply_tagged));
    if (tag_output) {
        uint16_t reply_vid = (uint16_t)(u16(frame+14U) & UINT16_C(0x0fff));
        uint16_t tci = (uint16_t)(((uint16_t)c->tx_pcp << 13) | reply_vid);
        if (reply_len + 4U > capacity ||
            reply_len + 4U > OU_VLAN_MAX_TAGGED) return -1;
        for (i = reply_len; i > 12U; --i) response[i+3U] = response[i-1U];
        put16(response+12U, OU_VLAN_TPID);
        put16(response+14U, tci);
        out_len = reply_len + 4U;
    } else out_len = reply_len;
    *written = out_len;
    inc(&table->replied);
    inc(&table->statistics[ctx.interface_index].answered);
    return 1;
}
