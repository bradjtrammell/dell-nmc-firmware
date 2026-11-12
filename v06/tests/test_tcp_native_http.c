#include "ou_tcp_engine.h"
#include "ou_tcp_adapter.h"
#include "ou_native_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static ou_tcp_engine engine;
static ou_vlan_table vlans;
static const uint8_t mac[6]={2,15,16,17,18,19};
static const uint8_t peer_mac[6]={2,41,42,43,44,45};
static const uint8_t local_ip[4]={172,30,100,9};
static const uint8_t remote_ip[4]={172,30,100,41};
static uint8_t inbound[OU_NET_MAX_FRAME],outbound[OU_NET_MAX_FRAME];
static size_t inbound_len,outbound_len;
static uint16_t r16(const uint8_t*p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static uint32_t r32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void w16(uint8_t*p,uint16_t n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static void w32(uint8_t*p,uint32_t n){p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
static uint32_t sum(uint32_t s,const uint8_t*p,size_t n){size_t i;for(i=0;i+1U<n;i+=2U)s+=r16(p+i);if(i<n)s+=(uint32_t)p[i]<<8;while(s>>16)s=(s&65535U)+(s>>16);return s;}
static uint16_t check(uint32_t s){while(s>>16)s=(s&65535U)+(s>>16);return (uint16_t)~s;}
static void packet(uint16_t vid,uint16_t sport,uint32_t seq,uint32_t ack,uint8_t flags,const char *body,size_t n){
 uint8_t *ip=inbound+18U,*tcp=ip+20U;
 uint32_t pseudo;size_t bytes=18U+40U+n;
 assert(bytes<=sizeof(inbound));inbound_len=bytes<64U?64U:bytes;
 memset(inbound,0,inbound_len);memcpy(inbound,mac,6U);memcpy(inbound+6U,peer_mac,6U);
 w16(inbound+12U,0x8100);w16(inbound+14U,vid);w16(inbound+16U,0x0800);
 ip[0]=0x45;w16(ip+2U,(uint16_t)(40U+n));ip[8]=64;ip[9]=6;
 memcpy(ip+12U,remote_ip,4U);memcpy(ip+16U,local_ip,4U);
 w16(ip+10U,check(sum(0U,ip,20U)));
 w16(tcp,sport);w16(tcp+2U,80);w32(tcp+4U,seq);w32(tcp+8U,ack);
 tcp[12U]=0x50;tcp[13U]=flags;w16(tcp+14U,4096U);
 if(n)memcpy(tcp+20U,body,n);
 pseudo=sum(0U,ip+12U,8U)+6U+(uint32_t)(20U+n);
 w16(tcp+16U,check(sum(pseudo,tcp,20U+n)));
}
static int rx(uint32_t time){
 return ou_tcp_engine_receive(&engine,&vlans,inbound,inbound_len,time,
                              ou_tcp_native_http,0,outbound,sizeof(outbound),&outbound_len);
}
static void init(void){
 ou_vlan_interface iface={0};
 iface.vid=104U;memcpy(iface.ipv4,local_ip,4U);iface.prefix_length=24U;
 iface.allow_local_subnet=1U;
 iface.allowed_services=OU_VLAN_SERVICE_ARP|OU_VLAN_SERVICE_HTTP_READ;
 assert(ou_vlan_configure(&vlans,mac,&iface,1U,0U,1U,0U,0U,0U)==0);
 ou_native_service_init();
 assert(ou_tcp_engine_init(&engine,UINT32_C(0x83142363))==0);
}
static void transact(uint16_t port,const char *http,const char *expected_status,uint32_t start){
 uint32_t peer_seq=10000U+port,initial_server,client_next;
 const uint8_t *tcp;
 size_t len=strlen(http);
 init();packet(104U,port,peer_seq,0U,0x02U,0,0U);
 assert(rx(start)==1);assert(outbound[12]==0x81&&r16(outbound+14)==104U);
 tcp=outbound+38U;assert(tcp[13]==0x12U);initial_server=r32(tcp+4U);
 packet(104U,port,peer_seq+1U,initial_server+1U,0x10U,0,0U);
 assert(rx(start+1U)==0);
 packet(104U,port,peer_seq+1U,initial_server+1U,0x18U,http,len);
 assert(rx(start+2U)==1);assert(outbound[38U+13U]==0x18U);
 assert(outbound_len>58U+strlen(expected_status));
 assert(memcmp(outbound+58U,expected_status,strlen(expected_status))==0);
 client_next=peer_seq+1U+(uint32_t)len;
 /* Follow the native response until all segments and FIN are acknowledged. */
 while(engine.slots[0].state&&engine.slots[0].state!=4U){
  packet(104U,port,client_next,engine.slots[0].send_next,0x10U,0,0U);
  assert(rx(start+3U)==1);
 }
 assert(engine.slots[0].state==4U);
 packet(104U,port,client_next,engine.slots[0].send_next,0x11U,0,0U);
 assert(rx(start+4U)==1);assert(engine.slots[0].state==0U);
}
int main(void){
 transact(41000U,"GET /api/v1/health HTTP/1.0\r\n\r\n","HTTP/1.1 200 OK",100U);
 transact(41001U,"POST /api/v1/ups/status HTTP/1.0\r\n\r\n",
          "HTTP/1.1 405 Method Not Allowed",200U);
 puts("PASS: real native HTTP read-only service through VLAN TCP and FIN");
 return 0;
}
