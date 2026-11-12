#include "ou_tcp_engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Unlike test_tcp_engine.c this exercises the REAL checksum/VLAN ingress
 * validator (no mock) before TCP state transitions and HTTP delivery. */
static ou_tcp_engine engine;
static ou_vlan_table vlans;
static const uint8_t local_mac[6]={2,0x33,0x44,0x55,0x66,0x77};
static const uint8_t peer_mac[6]={2,0xaa,0xbb,0xcc,0xdd,0xee};
static const uint8_t ip1[4]={192,168,30,2};
static const uint8_t peer1[4]={192,168,30,22};
static const uint8_t ip2[4]={10,20,0,2};
static const uint8_t peer2[4]={10,20,0,22};
static uint8_t rx[OU_NET_MAX_FRAME],tx[OU_NET_MAX_FRAME];
static size_t rx_size,tx_size;
static unsigned app_calls;
static uint16_t r16(const uint8_t*p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static uint32_t r32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void w16(uint8_t*p,uint16_t n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static void w32(uint8_t*p,uint32_t n){p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
static uint32_t sum(uint32_t s,const uint8_t*p,size_t n){size_t i;for(i=0;i+1U<n;i+=2U)s+=r16(p+i);if(i<n)s+=(uint32_t)p[i]<<8;while(s>>16)s=(s&65535U)+(s>>16);return s;}
static uint16_t csum(uint32_t s){while(s>>16)s=(s&65535U)+(s>>16);return (uint16_t)~s;}
static int app(void*ctx,const uint8_t*request,size_t n,uint32_t now,
               char*response,size_t cap,size_t*used){
    const char expected[]="GET /api/v1/health HTTP/1.1\r\nHost: card\r\n\r\n";
    const char reply[]="HTTP/1.1 200 OK\r\nContent-Length: 11\r\nConnection: close\r\n\r\n{\"ok\":true}";
    (void)ctx;(void)now;
    assert(n==sizeof(expected)-1U && memcmp(request,expected,n)==0);
    assert(cap>=sizeof(reply)-1U);
    memcpy(response,reply,sizeof(reply)-1U);
    *used=sizeof(reply)-1U;app_calls++;return 0;
}
static void init(void){
    ou_vlan_interface v[2]={{0}};
    assert(ou_tcp_engine_init(&engine,UINT32_C(0x9ac0371f))==0);
    v[0].vid=100;memcpy(v[0].ipv4,ip1,4U);v[0].prefix_length=24U;
    v[0].allowed_services=OU_VLAN_SERVICE_ARP|OU_VLAN_SERVICE_HTTP_READ;
    v[0].allow_local_subnet=1U;v[0].tx_pcp=5U;
    v[1].vid=200;memcpy(v[1].ipv4,ip2,4U);v[1].prefix_length=24U;
    v[1].allowed_services=OU_VLAN_SERVICE_ARP;v[1].allow_local_subnet=1U;
    assert(ou_vlan_configure(&vlans,local_mac,v,2,0U,1U,0U,0U,0U)==0);
    app_calls=0U;
}
static void packet(uint16_t vid,const uint8_t *source,const uint8_t *dest,
                   uint16_t port,uint32_t seq,uint32_t ack,uint8_t flags,
                   const char *payload,size_t payload_size){
    uint8_t *ip=rx+18U,*tcp=ip+20U;size_t size=18U+40U+payload_size;
    uint32_t pseudo;
    assert(size<=sizeof(rx));
    rx_size=size<64U?64U:size;
    memset(rx,0,rx_size);
    memcpy(rx,local_mac,6U);memcpy(rx+6U,peer_mac,6U);
    w16(rx+12U,OU_VLAN_TPID);w16(rx+14U,vid);w16(rx+16U,0x0800U);
    ip[0]=0x45U;w16(ip+2U,(uint16_t)(40U+payload_size));ip[8]=64U;ip[9]=6U;
    memcpy(ip+12U,source,4U);memcpy(ip+16U,dest,4U);
    w16(ip+10U,csum(sum(0U,ip,20U)));
    w16(tcp,port);w16(tcp+2U,80U);w32(tcp+4U,seq);w32(tcp+8U,ack);
    tcp[12U]=0x50U;tcp[13U]=flags;w16(tcp+14U,4096U);
    if(payload_size)memcpy(tcp+20U,payload,payload_size);
    pseudo=sum(0U,ip+12U,8U)+6U+(uint32_t)(20U+payload_size);
    w16(tcp+16U,csum(sum(pseudo,tcp,20U+payload_size)));
}
static int receive(uint32_t now){
    return ou_tcp_engine_receive(&engine,&vlans,rx,rx_size,now,app,0,
                                 tx,sizeof(tx),&tx_size);
}
static void verify_reply(uint8_t flags){
    const uint8_t*ip=tx+18U,*tcp=ip+20U;
    size_t bytes=r16(ip+2U);uint32_t pseudo;
    assert(tx_size>=64U && r16(tx+12U)==OU_VLAN_TPID);
    assert((r16(tx+14U)&0xfffU)==100U && (r16(tx+14U)>>13U)==5U);
    assert((tcp[13U]&flags)==flags);
    assert(!memcmp(tx,peer_mac,6U) && !memcmp(tx+6U,local_mac,6U));
    assert(!memcmp(ip+12U,ip1,4U) && !memcmp(ip+16U,peer1,4U));
    assert(csum(sum(0U,ip,20U))==0U);
    pseudo=sum(0U,ip+12U,8U)+6U+(uint32_t)(bytes-20U);
    assert(csum(sum(pseudo,tcp,bytes-20U))==0U);
}
static void test_http(void){
    const char p1[]="GET /api/v1/health HTTP/1.1\r\nHost:";
    const char p2[]=" card\r\n\r\n";
    uint32_t client=70100U,server_seq,client_next;
    init();packet(100U,peer1,ip1,50001U,client,0U,2U,0,0U);
    assert(receive(100U)==1);verify_reply(0x12U);
    server_seq=r32(tx+18U+20U+4U);
    packet(100U,peer1,ip1,50001U,client+1U,server_seq+1U,0x10U,0,0U);
    assert(receive(101U)==0);
    packet(100U,peer1,ip1,50001U,client+1U,server_seq+1U,0x18U,p1,sizeof(p1)-1U);
    assert(receive(102U)==1);verify_reply(0x10U);
    client_next=client+1U+(uint32_t)(sizeof(p1)-1U);
    packet(100U,peer1,ip1,50001U,client_next,server_seq+1U,0x18U,p2,sizeof(p2)-1U);
    assert(receive(103U)==1);verify_reply(0x18U);
    assert(app_calls==1U);
    assert(!memcmp(tx+18U+40U,"HTTP/1.1 200 OK",15U));
    client_next+=(uint32_t)(sizeof(p2)-1U);
    packet(100U,peer1,ip1,50001U,client_next,engine.slots[0].send_next,0x10U,0,0U);
    assert(receive(104U)==1);verify_reply(0x11U);
    packet(100U,peer1,ip1,50001U,client_next,engine.slots[0].send_next,0x11U,0,0U);
    assert(receive(105U)==1);verify_reply(0x10U);
    assert(!engine.slots[0].state);
}
static void test_denials(void){
    init();
    packet(200U,peer2,ip2,50000U,500U,0U,2U,0,0U);
    assert(receive(10U)==0 && !engine.slots[0].state);
    packet(100U,peer2,ip1,50000U,500U,0U,2U,0,0U);
    assert(receive(11U)==0);
    packet(100U,peer1,ip2,50000U,500U,0U,2U,0,0U);
    assert(receive(12U)==0);
    packet(100U,peer1,ip1,50000U,500U,0U,2U,0,0U);
    rx[18U+20U+16U]^=1U;assert(receive(13U)==0);
    packet(100U,peer1,ip1,50000U,500U,0U,2U,0,0U);
    rx[18U+10U]^=1U;assert(receive(14U)==0);
    packet(100U,peer1,ip1,50000U,500U,0U,2U,0,0U);
    rx[16U]=0x81U;rx[17U]=0U;assert(receive(15U)==0);
    packet(100U,peer1,ip1,50000U,500U,0U,2U,0,0U);
    assert(receive(16U)==1);verify_reply(0x12U);
}
int main(void){
    test_http();test_denials();
    puts("PASS: real VLAN/IP/TCP ingress, split HTTP request, checksums, ACL isolation");
    return 0;
}
