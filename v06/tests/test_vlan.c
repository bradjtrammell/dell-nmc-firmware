#include "ou_vlan.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8_t mac[6]={2,0x23,0x34,0x45,0x56,0x67};
static ou_vlan_table table;
static ou_vlan_interface iface(uint16_t vid,uint8_t a,uint8_t b,uint8_t c,
                               uint8_t d,uint32_t role) {
    ou_vlan_interface v={0};
    v.vid=vid;v.ipv4[0]=a;v.ipv4[1]=b;v.ipv4[2]=c;v.ipv4[3]=d;
    v.prefix_length=24U;v.allowed_services=role;
    v.allow_local_subnet=1U;v.tx_pcp=5U;
    return v;
}
static void init(void) {
    ou_vlan_interface e[3];
    e[0]=iface(10,192,168,10,17,OU_VLAN_SERVICE_ARP|
                              OU_VLAN_SERVICE_PING|OU_VLAN_SERVICE_HTTP_READ);
    e[1]=iface(20,10,20,0,9,OU_VLAN_SERVICE_ARP|
                          OU_VLAN_SERVICE_PING|OU_VLAN_SERVICE_CONTROL);
    e[2]=iface(30,192,168,10,17,OU_VLAN_SERVICE_ARP);
    assert(ou_vlan_configure(&table,mac,e,3,10,1,0,0,0)==0);
}
static uint16_t checksum(const uint8_t*p,size_t n) {
    size_t i;uint32_t sum=0;
    for(i=0;i+1U<n;i+=2U)sum+=((uint32_t)p[i]<<8)|p[i+1U];
    if(i<n)sum+=(uint32_t)p[i]<<8;
    while(sum>>16)sum=(sum&65535U)+(sum>>16);
    return (uint16_t)~sum;
}
static void arp(uint8_t *f,uint16_t target,uint16_t vid,uint8_t tagged) {
    static const uint8_t peer[6]={2,0xaa,0xbb,0xcc,0xdd,0xee};
    size_t offset=tagged?18U:14U;
    memset(f,0,64);memset(f,255,6);memcpy(f+6,peer,6);
    if(tagged){f[12]=0x81;f[14]=(uint8_t)(vid>>8);f[15]=(uint8_t)vid;
               f[16]=8;f[17]=6;}
    else{f[12]=8;f[13]=6;}
    f[offset+1]=1;f[offset+2]=8;f[offset+4]=6;f[offset+5]=4;
    f[offset+7]=1;memcpy(f+offset+8,peer,6);
    f[offset+14]=192;f[offset+15]=168;f[offset+16]=10;f[offset+17]=30;
    f[offset+24]=192;f[offset+25]=168;f[offset+26]=10;
    f[offset+27]=(uint8_t)target;
}
static void ping(uint8_t *f,const uint8_t dest[4],const uint8_t src[4],
                 uint16_t vid,uint8_t tagged) {
    uint8_t plain[60]={0};size_t i;uint16_t sum;
    static const uint8_t peer[6]={2,0xaa,0xbb,0xcc,0xdd,0xee};
    memcpy(plain,mac,6);memcpy(plain+6,peer,6);
    plain[12]=8;plain[14]=0x45;plain[17]=36;plain[22]=64;plain[23]=1;
    memcpy(plain+26,src,4);memcpy(plain+30,dest,4);
    sum=checksum(plain+14,20);plain[24]=(uint8_t)(sum>>8);plain[25]=(uint8_t)sum;
    plain[34]=8;plain[39]=10;plain[41]=11;
    plain[42]=1;plain[43]=2;plain[44]=3;plain[45]=4;
    sum=checksum(plain+34,16);plain[36]=(uint8_t)(sum>>8);plain[37]=(uint8_t)sum;
    if(tagged) {
        for(i=0;i<12;i++)f[i]=plain[i];
        f[12]=0x81;f[13]=0;f[14]=(uint8_t)(vid>>8);f[15]=(uint8_t)vid;
        for(i=12;i<60;i++)f[i+4]=plain[i];
    } else memcpy(f,plain,60);
}
static void test_config(void) {
    ou_vlan_interface e[65];size_t i;
    init();
    for(i=0;i<65;i++)e[i]=iface((uint16_t)(i+100U),10,(uint8_t)i,1,2,
                                OU_VLAN_SERVICE_ARP);
    assert(ou_vlan_configure(&table,mac,e,65,0,1,0,0,0)==-1);
    assert(table.count==0);
    assert(ou_vlan_configure(&table,mac,e,64,0,1,0,0,0)==0);
    assert(table.count==64 && !table.native_enabled);
    e[1]=e[0];
    assert(ou_vlan_configure(&table,mac,e,2,100,1,0,0,0)==-1);
    e[1]=iface(4095,10,1,1,2,OU_VLAN_SERVICE_ARP);
    assert(ou_vlan_configure(&table,mac,e,2,100,1,0,0,0)==-1);
    e[1]=iface(101,10,1,1,2,OU_VLAN_SERVICE_ARP);
    e[1].tx_pcp=8;
    assert(ou_vlan_configure(&table,mac,e,2,100,1,0,0,0)==-1);
    init();
}
static void test_parse(void) {
    uint8_t f[1519];ou_vlan_rx_context ctx;
    init();arp(f,17,0,0);
    assert(ou_vlan_classify(&table,f,60,&ctx)==1);
    assert(ctx.vlan_id==10 && !ctx.tagged);
    arp(f,17,20,1);
    assert(ou_vlan_classify(&table,f,64,&ctx)==1);
    assert(ctx.vlan_id==20 && ctx.tagged);
    arp(f,17,99,1);assert(ou_vlan_classify(&table,f,64,&ctx)==0);
    arp(f,17,10,1);assert(ou_vlan_classify(&table,f,64,&ctx)==0);
    arp(f,17,4095,1);assert(ou_vlan_classify(&table,f,64,&ctx)==0);
    arp(f,17,0,1);assert(ou_vlan_classify(&table,f,64,&ctx)==0);
    arp(f,17,20,1);f[12]=0x88;f[13]=0xa8;
    assert(ou_vlan_classify(&table,f,64,&ctx)==0);
    arp(f,17,20,1);f[16]=0x81;f[17]=0;
    assert(ou_vlan_classify(&table,f,64,&ctx)==0);
    arp(f,17,20,1);f[14]=0x10;
    assert(ou_vlan_classify(&table,f,64,&ctx)==0);
    arp(f,17,20,1);
    assert(ou_vlan_classify(&table,f,sizeof(f),&ctx)==-1);
    assert(ou_vlan_classify(&table,f,63,&ctx)==0);
    arp(f,17,0,0);
    assert(ou_vlan_classify(&table,f,1515,&ctx)==0);
}
static void test_acl(void) {
    uint8_t local[4]={10,20,0,44},remote[4]={172,19,8,2},bad[4]={172,19,9,2};
    ou_vlan_interface e=iface(20,10,20,0,9,OU_VLAN_SERVICE_ARP|
                                OU_VLAN_SERVICE_CONTROL);
    init();
    assert(ou_vlan_authorize(&table,20,local,OU_VLAN_SERVICE_CONTROL));
    assert(!ou_vlan_authorize(&table,20,local,OU_VLAN_SERVICE_HTTP_READ));
    assert(!ou_vlan_authorize(&table,30,local,OU_VLAN_SERVICE_PING));
    assert(!ou_vlan_authorize(&table,20,remote,OU_VLAN_SERVICE_CONTROL));
    e.allow_local_subnet=0;e.acl_count=1;
    e.acl[0].address[0]=172;e.acl[0].address[1]=19;e.acl[0].address[2]=8;
    e.acl[0].prefix_length=24;
    assert(ou_vlan_configure(&table,mac,&e,1,0,1,0,0,0)==0);
    assert(ou_vlan_authorize(&table,20,remote,OU_VLAN_SERVICE_CONTROL));
    assert(!ou_vlan_authorize(&table,20,bad,OU_VLAN_SERVICE_CONTROL));
    assert(!ou_vlan_authorize(&table,20,local,OU_VLAN_SERVICE_CONTROL));
    assert(ou_vlan_authorize(&table,20,local,OU_VLAN_SERVICE_ARP));
}
static void test_tagged_reply_and_isolation(void) {
    static const uint8_t ip20[4]={10,20,0,9};
    static const uint8_t src20[4]={10,20,0,44};
    static const uint8_t ip10[4]={192,168,10,17};
    uint8_t rx[64],tx[1518];size_t n=0;
    init();arp(rx,17,0,0);
    assert(ou_vlan_reply(&table,rx,60,tx,sizeof(tx),&n)==1);
    assert(n==60 && tx[12]==8 && tx[13]==6);
    arp(rx,17,30,1);
    assert(ou_vlan_reply(&table,rx,64,tx,sizeof(tx),&n)==1);
    assert(n==64 && tx[12]==0x81 && tx[14]==0xa0 && tx[15]==30);
    assert(tx[16]==8 && tx[17]==6);
    ping(rx,ip20,src20,20,1);
    assert(ou_vlan_reply(&table,rx,64,tx,sizeof(tx),&n)==1);
    assert(n==64 && tx[15]==20 && tx[16]==8 && tx[17]==0);
    assert(tx[38]==0 && tx[30]==10 && tx[31]==20 && tx[33]==9);
    assert(table.statistics[1].answered==1);
    ping(rx,ip10,src20,20,1);
    assert(ou_vlan_reply(&table,rx,64,tx,sizeof(tx),&n)==0 && n==0);
    ping(rx,ip10,src20,10,1);
    assert(ou_vlan_reply(&table,rx,64,tx,sizeof(tx),&n)==0 && n==0);
    assert(table.replied==3 && table.denied>=1);
}
static void test_native_priority(void) {
    uint8_t f[64],tx[1518];size_t n;
    ou_vlan_interface e=iface(10,192,168,10,17,OU_VLAN_SERVICE_ARP);
    assert(ou_vlan_configure(&table,mac,&e,1,10,1,1,1,1)==0);
    arp(f,17,0,1);
    assert(ou_vlan_reply(&table,f,64,tx,sizeof(tx),&n)==1);
    assert(n==64 && tx[12]==0x81 && tx[14]==0xa0 && tx[15]==0);
    arp(f,17,10,1);
    assert(ou_vlan_reply(&table,f,64,tx,sizeof(tx),&n)==1);
    assert(tx[15]==10);
}
static void test_malformed_source_does_not_fault_rx(void) {
    uint8_t frame[64],reply[1518];size_t n=123U;
    init();arp(frame,17,20,1);
    frame[6]=3U; /* invalid multicast source MAC */
    assert(ou_vlan_reply(&table,frame,64,reply,sizeof(reply),&n)==0);
    assert(n==0U);
}
int main(void) {
    test_config();test_parse();test_acl();
    test_tagged_reply_and_isolation();test_native_priority();test_malformed_source_does_not_fault_rx();
    puts("PASS: 64 independent VLAN interfaces, native/802.1p, tagged ARP/ICMP, ACL, isolation and strict frames");
    return 0;
}
