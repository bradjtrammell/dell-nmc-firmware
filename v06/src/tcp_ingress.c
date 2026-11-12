#include "ou_tcp_ingress.h"
static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static uint32_t rd32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static int equal(const uint8_t *a, const uint8_t *b, size_t count) {
    size_t i;
    for (i = 0U; i < count; ++i) if (a[i] != b[i]) return 0;
    return 1;
}
static uint32_t add16(uint32_t sum, const uint8_t *data, size_t size) {
    size_t i;
    for (i = 0U; i + 1U < size; i += 2U) sum += rd16(data + i);
    if (i < size) sum += (uint32_t)data[i] << 8;
    while (sum > UINT32_C(0xffff)) sum = (sum & UINT32_C(0xffff)) + (sum >> 16);
    return sum;
}
static int checksum_good(uint32_t sum) {
    while (sum > UINT32_C(0xffff)) sum = (sum & UINT32_C(0xffff)) + (sum >> 16);
    return sum == UINT32_C(0xffff);
}
int ou_tcp_ingress_check(const ou_vlan_table *vlans, const uint8_t *frame,
                         size_t length, ou_tcp_ingress *result) {
    ou_vlan_rx_context vlan;
    size_t ip_offset, ip_len, tcp_offset, tcp_len, tcp_header;
    uint16_t tcp_port;
    uint32_t pseudo;
    const uint8_t *ip, *tcp;
    int classified;
    unsigned i;
    if (!result) return -1;
    *result = (ou_tcp_ingress){0};
    if (!vlans || !frame) return -1;
    classified = ou_vlan_classify(vlans, frame, length, &vlan);
    if (classified != 1) return 0;
    if (vlan.inner_ether_type != UINT16_C(0x0800)) return 0;
    ip_offset = vlan.tagged ? 18U : 14U;
    if (length - ip_offset < 40U) return 0;
    ip = frame + ip_offset;
    if (ip[0] != UINT8_C(0x45) || ip[8] == 0U || ip[9] != OU_TCP_IPV4_PROTOCOL ||
        (rd16(ip + 6U) & UINT16_C(0x3fff)) != 0U ||
        ip[12] == 0U || ip[12] == 127U || ip[12] >= 224U ||
        (frame[6] & 1U) != 0U || (frame[0] & 1U) != 0U ||
        equal(frame + 6U, (const uint8_t[6]){0}, 6U) ||
        !equal(frame, vlans->mac, 6U)) return 0;
    ip_len = rd16(ip + 2U);
    if (ip_len < 40U || ip_len > OU_NET_MTU || ip_len > length - ip_offset ||
        !checksum_good(add16(0U, ip, 20U)) ||
        !equal(ip + 16U, vlans->interfaces[vlan.interface_index].ipv4, 4U) ||
        equal(ip + 12U, ip + 16U, 4U)) return 0;
    if (!ou_vlan_authorize(vlans, vlan.vlan_id, ip + 12U,
                            OU_VLAN_SERVICE_HTTP_READ)) return 0;
    tcp_offset = ip_offset + 20U;
    tcp_len = ip_len - 20U;
    tcp = frame + tcp_offset;
    tcp_header = (size_t)((tcp[12] >> 4) & 15U) * 4U;
    tcp_port = rd16(tcp + 2U);
    if (tcp_header < 20U || tcp_header > tcp_len ||
        tcp_port != OU_TCP_HTTP_PORT || rd16(tcp) == 0U ||
        (tcp[12] & 15U) != 0U ||
        (tcp[13] & UINT8_C(0x3f)) == 0U ||
        ((tcp[13] & UINT8_C(0x03)) == UINT8_C(0x03))) return 0;
    /* RFC 9293 checksum covers TCP header, payload and IPv4 pseudoheader. */
    pseudo = add16(0U, ip + 12U, 8U);
    pseudo += OU_TCP_IPV4_PROTOCOL;
    pseudo += (uint32_t)tcp_len;
    pseudo = add16(pseudo, tcp, tcp_len);
    if (!checksum_good(pseudo)) return 0;
    result->vlan = vlan;
    for (i = 0U; i < 6U; ++i) result->source_mac[i] = frame[6U + i];
    for (i = 0U; i < 4U; ++i) {
        result->source_ipv4[i] = ip[12U + i];
        result->destination_ipv4[i] = ip[16U + i];
    }
    result->source_port = rd16(tcp);
    result->destination_port = tcp_port;
    result->advertised_window = rd16(tcp + 14U);
    result->sequence = rd32(tcp + 4U);
    result->acknowledgment = rd32(tcp + 8U);
    result->flags = tcp[13];
    result->payload = tcp + tcp_header;
    result->payload_length = tcp_len - tcp_header;
    return 1;
}
