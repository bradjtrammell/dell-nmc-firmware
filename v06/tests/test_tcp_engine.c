#include "ou_tcp_engine.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static ou_tcp_engine server;
static ou_vlan_table vlans;
static uint8_t input[1518], output[1518];
static size_t out_len;
static uint32_t peerseq=40000U,serverseq;
static uint16_t v16(const uint8_t *p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static uint32_t v32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void w16(uint8_t*p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void w32(uint8_t*p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static size_t in_len;
static void packet(uint16_t vid,uint16_t srcport,uint32_t seq,uint32_t ack,uint8_t flags,const char *data,size_t length){
 uint8_t *ip=input+18,*tcp=ip+20;size_t n=18+40+length;
 assert(n<=sizeof(input));memset(input,0,n>64?n:64);
 input[0]=2;input[1]=3;input[2]=4;input[3]=5;input[4]=6;input[5]=7;
 input[6]=2;input[7]=11;input[8]=22;input[9]=33;input[10]=44;input[11]=55;
 input[12]=0x81;w16(input+14,vid);w16(input+16,0x800);
 ip[0]=0x45;w16(ip+2,(uint16_t)(40+length));ip[8]=64;ip[9]=6;
 ip[12]=10;ip[13]=5;ip[14]=0;ip[15]=15;
 ip[16]=10;ip[17]=5;ip[18]=0;ip[19]=17;
 w16(tcp,srcport);w16(tcp+2,80);w32(tcp+4,seq);w32(tcp+8,ack);
 tcp[12]=0x50;tcp[13]=flags;
 if(length)memcpy(tcp+20,data,length);
 in_len=n>64?n:64;
}
/* Mock ONLY the previously tested VLAN/TCP packet verifier, so these tests
 * independently exercise session sequencing and output framing. */
int ou_tcp_ingress_check(const ou_vlan_table*t,const uint8_t *f,size_t n,ou_tcp_ingress*in){
 const uint8_t *tcp=f+38,*ip=f+18;int idx=-1;size_t i;
 if(n<58||!t||!in)return 0;
 for(i=0;i<t->count;i++)if(t->interfaces[i].vid==v16(f+14)){idx=(int)i;break;}
 if(idx<0||f[12]!=0x81)return 0;
 memset(in,0,sizeof(*in));in->vlan.vlan_id=v16(f+14);in->vlan.tagged=1;
 in->vlan.interface_index=(uint8_t)idx;in->source_port=v16(tcp);
 in->destination_port=v16(tcp+2);in->advertised_window=4096U;
 in->sequence=v32(tcp+4);in->acknowledgment=v32(tcp+8);in->flags=tcp[13];
 memcpy(in->source_mac,f+6,6);memcpy(in->source_ipv4,ip+12,4);
 memcpy(in->destination_ipv4,ip+16,4);
 in->payload=tcp+20;in->payload_length=v16(ip+2)-40;
 return 1;
}
static int app(void*ctx,const uint8_t*req,size_t len,uint32_t now,char*out,size_t cap,size_t*written){
 size_t i;const char *key="GET /api/v1/health HTTP/1.1";
 (void)ctx;(void)now;
 if(len<strlen(key)||memcmp(req,key,strlen(key))!=0||cap<2300)return -1;
 for(i=0;i<2300;i++)out[i]='X';
 *written=2300;return 0;
}
static void initialize(void){
 memset(&vlans,0,sizeof(vlans));
 vlans.count=2;vlans.mac[0]=2;vlans.mac[1]=3;vlans.mac[2]=4;
 vlans.mac[3]=5;vlans.mac[4]=6;vlans.mac[5]=7;
 vlans.interfaces[0].vid=100;vlans.interfaces[0].tx_pcp=5;
 vlans.interfaces[1].vid=200;vlans.interfaces[1].tx_pcp=2;
 assert(ou_tcp_engine_init(&server,123456U)==0);
}
static int send(uint32_t t){return ou_tcp_engine_receive(&server,&vlans,input,in_len,t,app,0,output,sizeof(output),&out_len);}
static void test_normal(void){
 const char *part1="GET /api/v1/health HTTP/1.1\r\nHost:";
 const char *part2=" test\r\n\r\n";
 uint32_t seq;
 initialize();packet(100,3456,peerseq,0,2,0,0);
 assert(send(100)==1);assert(output[12]==0x81&&v16(output+14)==(uint16_t)(5U<<13|100));
 assert(output[18+20+13]==0x12);serverseq=v32(output+38+4);
 assert(v32(output+38+8)==peerseq+1U);assert(out_len>=64);
 packet(100,3456,peerseq+1U,serverseq+1U,0x10,0,0);
 assert(send(110)==0);assert(server.slots[0].state==2);
 packet(100,3456,peerseq+1U,serverseq+1U,0x18,part1,strlen(part1));
 assert(send(120)==1);assert(output[38+13]==0x10);
 seq=peerseq+1U+(uint32_t)strlen(part1);
 packet(100,3456,seq,serverseq+1U,0x18,part2,strlen(part2));
 assert(send(125)==1);assert(output[38+13]==0x18);
 assert(v16(output+18+2)==40+1024);assert(v32(output+38+4)==serverseq+1U);
 assert(server.slots[0].response_len==2300);
 seq+=(uint32_t)strlen(part2);
 packet(100,3456,seq,serverseq+1025U,0x10,0,0);
 assert(send(130)==1);assert(v16(output+18+2)==40+1024);
 packet(100,3456,seq,serverseq+2049U,0x10,0,0);
 assert(send(135)==1);assert(v16(output+18+2)==40+252);
 packet(100,3456,seq,serverseq+2301U,0x10,0,0);
 assert(send(140)==1);assert(output[38+13]==0x11);
 packet(100,3456,seq,serverseq+2302U,0x11,0,0);
 assert(send(145)==1);assert(output[38+13]==0x10);
 assert(!server.slots[0].state);
}
static void test_timeout_and_isolation(void){
 size_t n;initialize();
 packet(100,3456,peerseq,0,2,0,0);assert(send(100)==1);
 assert(ou_tcp_engine_tick(&server,800,output,sizeof(output),&n)==0);
 assert(ou_tcp_engine_tick(&server,851,output,sizeof(output),&n)==1);
 assert(n>=64&&output[38+13]==0x12);
 packet(200,3456,peerseq,0,2,0,0);assert(send(860)==1);
 assert(server.slots[0].vid==100&&server.slots[1].vid==200);
 assert(v16(output+14)==(uint16_t)(2U<<13|200));
 packet(100,3457,peerseq+100U,0,2,0,0);assert(send(870)==1);
 packet(100,3458,peerseq+200U,0,2,0,0);assert(send(880)==1);
 packet(100,3459,peerseq+300U,0,2,0,0);assert(send(890)==0);
 assert(ou_tcp_engine_tick(&server,22000,output,sizeof(output),&n)==0);
 assert(server.slots[0].state==0&&server.slots[1].state==0);
}
int main(void){test_normal();test_timeout_and_isolation();puts("PASS: TCP handshake, segmented HTTP response, FIN, retransmission, 4-slot VLAN isolation");return 0;}
