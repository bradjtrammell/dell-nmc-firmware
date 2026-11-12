#include "ou_tcp_ingress.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static ou_vlan_table vlans;
static const uint8_t mac[6]={2,3,4,5,6,7};
static void wr16(uint8_t *p,uint16_t n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static uint16_t sum(uint32_t v,const uint8_t*p,size_t n){size_t i;
 for(i=0;i+1<n;i+=2)v+=((uint16_t)p[i]<<8)|p[i+1];
 if(i<n)v+=(uint32_t)p[i]<<8;
 while(v>>16)v=(v&65535U)+(v>>16);
 return (uint16_t)~v;
}
static void init(void) {
 ou_vlan_interface x[2]={{0}};
 x[0].vid=100;x[0].ipv4[0]=192;x[0].ipv4[1]=168;x[0].ipv4[2]=10;x[0].ipv4[3]=2;
 x[0].prefix_length=24;x[0].allowed_services=OU_VLAN_SERVICE_ARP|OU_VLAN_SERVICE_HTTP_READ;
 x[0].allow_local_subnet=1;
 x[1]=x[0];x[1].vid=200;x[1].ipv4[0]=10;x[1].ipv4[1]=20;x[1].ipv4[2]=0;x[1].ipv4[3]=2;
 x[1].allowed_services=OU_VLAN_SERVICE_ARP;
 assert(ou_vlan_configure(&vlans,mac,x,2,0U,1U,0U,0U,0U)==0);
}
static void build(uint8_t *f,uint16_t vid,uint8_t target[4],uint8_t src[4]) {
 uint8_t *ip=f+18,*tcp=ip+20;uint16_t c;
 memset(f,0,64);
 memcpy(f,mac,6);f[6]=2;f[7]=40;f[8]=60;f[9]=44;f[10]=16;f[11]=8;
 f[12]=0x81;f[13]=0;wr16(f+14,vid);f[16]=8;f[17]=0;
 ip[0]=0x45;wr16(ip+2,40);ip[8]=64;ip[9]=6;
 memcpy(ip+12,src,4);memcpy(ip+16,target,4);
 c=sum(0,ip,20);wr16(ip+10,c);
 wr16(tcp,1234);wr16(tcp+2,80);tcp[7]=33;tcp[12]=0x50;tcp[13]=0x02;
 wr16(tcp+14,2048);
 c=sum(6U+20U,ip+12,8);c=sum((uint16_t)~c,tcp,20);wr16(tcp+16,c);
}
int main(void) {
 uint8_t f[64],src[4]={192,168,10,21},target[4]={192,168,10,2};
 ou_tcp_ingress in;
 init();build(f,100,target,src);
 assert(ou_tcp_ingress_check(&vlans,f,sizeof(f),&in)==1);
 assert(in.vlan.vlan_id==100 && in.source_port==1234 && in.destination_port==80);
 assert(in.sequence==33U && in.flags==2U && !in.payload_length);
 f[18+20+16]^=1U;assert(ou_tcp_ingress_check(&vlans,f,64,&in)==0);
 build(f,100,target,(uint8_t[4]){172,16,2,2});
 assert(ou_tcp_ingress_check(&vlans,f,64,&in)==0);
 build(f,200,target,src);assert(ou_tcp_ingress_check(&vlans,f,64,&in)==0);
 build(f,100,target,src);f[12]=0x88;f[13]=0xa8;
 assert(ou_tcp_ingress_check(&vlans,f,64,&in)==0);
 build(f,100,target,src);f[18+9]=17;assert(ou_tcp_ingress_check(&vlans,f,64,&in)==0);
 build(f,100,target,src);f[18+6]=0x20;assert(ou_tcp_ingress_check(&vlans,f,64,&in)==0);
 build(f,100,target,src);f[18+20+12]=0x40;assert(ou_tcp_ingress_check(&vlans,f,64,&in)==0);
 build(f,100,target,src);f[18+20+2]=0;f[18+20+3]=81;assert(ou_tcp_ingress_check(&vlans,f,64,&in)==0);
 assert(ou_tcp_ingress_check(&vlans,f,64,NULL)==-1);
 assert(ou_tcp_ingress_check(NULL,f,64,&in)==-1);
 assert(ou_tcp_ingress_check(&vlans,f,20,&in)==0);
 puts("PASS: 802.1Q TCP/IPv4 checksum, per-VLAN HTTP ACL, ports and malformed segments");
 return 0;
}
