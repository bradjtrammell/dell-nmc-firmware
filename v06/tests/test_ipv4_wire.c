#include "ou_ipv4_wire.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const ou_net_identity id = {{0x02,0x11,0x22,0x33,0x44,0x55},{192,168,9,17},1};
static uint16_t be16(const uint8_t *p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static void p16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static uint16_t sum(const uint8_t *buf,size_t n) {
 uint32_t s=0;size_t i;for(i=0;i+1<n;i+=2)s+=be16(buf+i);
 if(i<n)s+=(uint32_t)buf[i]<<8;
 while(s>>16)s=(s&65535U)+(s>>16);
 return (uint16_t)~s;
}
static void arp(uint8_t *p) {
 memset(p,0,60);memset(p,0xff,6);
 static const uint8_t src[]={2,0xaa,0xbb,0xcc,0xdd,0xee};
 memcpy(p+6,src,6);p16(p+12,0x0806);p16(p+14,1);p16(p+16,0x0800);
 p[18]=6;p[19]=4;p16(p+20,1);memcpy(p+22,src,6);
 p[28]=192;p[29]=168;p[30]=9;p[31]=20;memcpy(p+38,id.ipv4,4);
}
static void ping(uint8_t *p) {
 memset(p,0,74);memcpy(p,id.mac,6);
 static const uint8_t src[]={2,0xaa,0xbb,0xcc,0xdd,0xee};
 memcpy(p+6,src,6);p16(p+12,0x0800);
 p[14]=0x45;p16(p+16,36);p[22]=64;p[23]=1;
 p[26]=192;p[27]=168;p[28]=9;p[29]=20;
 memcpy(p+30,id.ipv4,4);
 p16(p+24,sum(p+14,20));
 p[34]=8;p[35]=0;p16(p+38,10);p16(p+40,11);
 p[42]=1;p[43]=2;p[44]=3;p[45]=4;
 p16(p+36,sum(p+34,16));
}
static void test_arp(void){
 uint8_t rx[60],tx[1518];size_t n=1;arp(rx);
 assert(ou_net_reply(&id,rx,sizeof(rx),tx,sizeof(tx),&n)==1);
 assert(n==60);assert(memcmp(tx,rx+6,6)==0);assert(memcmp(tx+6,id.mac,6)==0);
 assert(be16(tx+20)==2);assert(memcmp(tx+22,id.mac,6)==0);
 assert(memcmp(tx+28,id.ipv4,4)==0);assert(memcmp(tx+38,rx+28,4)==0);
 rx[38]^=1;n=99;assert(ou_net_reply(&id,rx,sizeof(rx),tx,sizeof(tx),&n)==0&&n==0);
}
static void test_ping(void){
 uint8_t rx[74],tx[1518];size_t n=0;ping(rx);
 assert(ou_net_reply(&id,rx,sizeof(rx),tx,sizeof(tx),&n)==1);
 assert(n==60);assert(be16(tx+16)==36);assert(tx[34]==0 && tx[35]==0);
 assert(sum(tx+14,20)==0);assert(sum(tx+34,16)==0);
 assert(memcmp(tx+26,id.ipv4,4)==0);assert(tx[22]==64);
 ping(rx);rx[24]^=1;assert(ou_net_reply(&id,rx,74,tx,sizeof(tx),&n)==0&&n==0);
 ping(rx);rx[36]^=1;assert(ou_net_reply(&id,rx,74,tx,sizeof(tx),&n)==0&&n==0);
 ping(rx);p16(rx+20,0x2000);assert(ou_net_reply(&id,rx,74,tx,sizeof(tx),&n)==0&&n==0);
}
static void test_invalid(void){
 uint8_t rx[74],tx[1518];size_t n=99;ou_net_identity invalid=id;
 ping(rx);
 invalid.enabled=0;assert(ou_net_reply(&invalid,rx,74,tx,sizeof(tx),&n)==-1&&n==0);
 assert(ou_net_reply(&id,rx,74,tx,12,&n)==-1&&n==0);
 assert(ou_net_reply(&id,rx,74,tx,1518,NULL)==-1);
 invalid=id;invalid.mac[0]=1;
 assert(ou_net_reply(&invalid,rx,74,tx,1518,&n)==-1);
 invalid=id;invalid.ipv4[0]=224;
 assert(ou_net_reply(&invalid,rx,74,tx,1518,&n)==-1);
}
int main(void){test_arp();test_ping();test_invalid();puts("PASS: ARP reply / IPv4 ICMP echo frames, malformed packets and checksums");return 0;}
