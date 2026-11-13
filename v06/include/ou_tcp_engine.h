#ifndef OU_TCP_ENGINE_H
#define OU_TCP_ENGINE_H
#include <stdint.h>
#include <stddef.h>
#include "ou_tcp_ingress.h"
#define OU_TCP_SLOTS 4U
#define OU_TCP_REQUEST_CAP 4096U
#define OU_TCP_RESPONSE_CAP 4608U
#define OU_TCP_SEGMENT_MSS 1024U
#define OU_TCP_RETRY_MS 750U
#define OU_TCP_IDLE_MS 20000U
#define OU_TCP_MAX_RETRIES 4U

typedef int (*ou_tcp_app)(void *context,const uint8_t *request,size_t request_len,
                          uint32_t now_ms,char *out,size_t cap,size_t *written);
typedef struct {
    uint8_t state;
    uint8_t tagged;
    uint8_t priority_tag;
    uint8_t ingress_pcp;
    uint16_t vid;
    uint16_t remote_port;
    uint16_t peer_window;
    uint16_t peer_mss;
    uint8_t remote_mac[6];
    uint8_t remote_ip[4];
    uint8_t local_ip[4];
    uint32_t receive_next;
    uint32_t send_next;
    uint32_t send_unacked;
    uint32_t last_activity;
    uint32_t sent_at;
    uint8_t retries;
    uint8_t outstanding;
    uint16_t last_len;
    uint8_t last_packet[OU_NET_MAX_FRAME];
    uint8_t request[OU_TCP_REQUEST_CAP];
    size_t request_len;
    char response[OU_TCP_RESPONSE_CAP];
    size_t response_len;
    size_t response_offset;
} ou_tcp_slot;
typedef struct {
    ou_tcp_slot slots[OU_TCP_SLOTS];
    uint32_t next_isn;
    uint16_t next_ip_id;
} ou_tcp_engine;

/* Must supply a nonzero, unpredictable per-boot seed from the real BSP before
 * any physical networking. This is an IPv4 TCP/HTTP/1.x research transport;
 * plaintext service must not be enabled on an untrusted network. */
int ou_tcp_engine_init(ou_tcp_engine *server,uint32_t boot_seed);
/* Returns 1 if a response Ethernet frame is ready, 0 if ignored/no output,
 * -1 on invalid programmer arguments. Only one complete response per call. */
int ou_tcp_engine_receive(ou_tcp_engine *server,ou_vlan_table *vlans,
                          const uint8_t *frame,size_t len,uint32_t now_ms,
                          ou_tcp_app app,void *context,
                          uint8_t *reply,size_t cap,size_t *written);
int ou_tcp_engine_tick(ou_tcp_engine *server,uint32_t now_ms,
                       uint8_t *reply,size_t cap,size_t *written);
#endif
