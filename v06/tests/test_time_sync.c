#include "ou_time_sync.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct {unsigned calls;int fail;int constant;} entropy;
static int nonce(void *ctx,uint8_t out[8]){
    unsigned i;(void)ctx;
    if(entropy.fail)return -1;
    for(i=0U;i<8U;++i)
        out[i]=(uint8_t)(entropy.constant?0x37U:(entropy.calls+i+17U));
    ++entropy.calls;
    return 1;
}
static ou_time_source source(void){
    ou_time_source s={0};
    s.secure_nonce=nonce;
    s.source_acl_qualified=1U;
    s.isolated_trusted_network=1U;
    s.monotonic_clock_qualified=1U;
    s.era_anchored=1U;
    s.explicit_era=0U;
    return s;
}
static void put32(uint8_t *p,uint32_t n){
    p[0]=(uint8_t)(n>>24U);p[1]=(uint8_t)(n>>16U);
    p[2]=(uint8_t)(n>>8U);p[3]=(uint8_t)n;
}
static void reply(uint8_t out[OU_NTP_WIRE_BYTES],
                  const uint8_t request[OU_NTP_WIRE_BYTES],
                  uint32_t seconds){
    memset(out,0,OU_NTP_WIRE_BYTES);
    out[0]=0x24U;out[1]=2U;
    out[2]=6U;out[3]=0xecU;
    put32(out+8U,UINT32_C(65536)); /* dispersion 1 second */
    memcpy(out+24U,request+40U,8U);
    put32(out+32U,seconds);put32(out+40U,seconds);
    put32(out+44U,UINT32_C(0x80000000)); /* 500ms fraction */
}
static void qualification_and_nonce(void){
    ou_time_sync sync;
    ou_time_source s=source();
    uint8_t query[48];
    memset(&entropy,0,sizeof(entropy));
    s.source_acl_qualified=0U;
    assert(ou_time_init(&sync,&s)==-1);
    s.source_acl_qualified=1U;
    s.era_anchored=0U;
    assert(ou_time_init(&sync,&s)==-1);
    s.era_anchored=1U;s.explicit_era=2U;
    assert(ou_time_init(&sync,&s)==-1);
    s.explicit_era=0U;
    assert(ou_time_init(&sync,&s)==0);
    entropy.fail=1;
    assert(ou_time_make_request(&sync,100U,query)==-1);
    entropy.fail=0;
    assert(ou_time_make_request(&sync,100U,query)==0);
    assert(query[0]==0x23U);
    assert(query[2]==6U);
    assert(ou_time_make_request(&sync,101U,query)==-1);
    assert(query[40]!=0U);
}
static void verification_and_clock(void){
    ou_time_sync sync;
    ou_time_source src=source();
    uint8_t req[48],wire[48];
    ou_time_sample clock;
    memset(&entropy,0,sizeof(entropy));
    assert(ou_time_init(&sync,&src)==0);
    assert(ou_time_make_request(&sync,UINT32_MAX-50U,req)==0);
    reply(wire,req,UINT32_C(3913056000)); /* 2024-01-01 UTC */
    assert(ou_time_accept(&sync,wire,48U,149U,0)==-1);
    assert(sync.synchronized==0U);
    wire[24]^=1U;
    assert(ou_time_accept(&sync,wire,48U,149U,1)==-1);
    wire[24]^=1U;
    wire[0]=0xe4U;
    assert(ou_time_accept(&sync,wire,48U,149U,1)==-1);
    wire[0]=0x24U;wire[1]=0U;
    assert(ou_time_accept(&sync,wire,48U,149U,1)==-1);
    wire[1]=2U;
    assert(ou_time_accept(&sync,wire,48U,149U,1)==0);
    assert(sync.synchronized==1U);
    assert(ou_time_now(&sync,149U,&clock)==0);
    assert(clock.utc_unix_seconds==UINT64_C(1704067200));
    assert(clock.utc_millisecond==600U);
    assert(clock.round_trip_ms==200U);
    assert(clock.uncertainty_ms==1101U);
    assert(ou_time_now(&sync,649U,&clock)==0);
    assert(clock.utc_unix_seconds==UINT64_C(1704067201));
    assert(clock.utc_millisecond==100U);
    assert(ou_time_now(&sync,149U+OU_NTP_MAX_AGE_MS,&clock)==0);
    assert(ou_time_now(&sync,150U+OU_NTP_MAX_AGE_MS,&clock)==-1);
}
static void hostile_packets_and_era(void){
    ou_time_sync sync;
    ou_time_source src=source();
    uint8_t req[48],wire[48];
    ou_time_sample clock;
    memset(&entropy,0,sizeof(entropy));
    src.explicit_era=1U;
    assert(ou_time_init(&sync,&src)==0);
    assert(ou_time_make_request(&sync,1000U,req)==0);
    reply(wire,req,UINT32_C(28402304)); /* 2037-01-01 UTC, era 1 */
    assert(ou_time_accept(&sync,wire,48U,7000U,1)==-1);
    assert(sync.outstanding==0U);
    assert(ou_time_make_request(&sync,8000U,req)==0);
    reply(wire,req,UINT32_C(28402304));
    wire[0]=0x23U; /* client, never server */
    assert(ou_time_accept(&sync,wire,48U,8200U,1)==-1);
    wire[0]=0x24U;
    put32(wire+8U,UINT32_C(6)*UINT32_C(65536));
    assert(ou_time_accept(&sync,wire,48U,8200U,1)==-1);
    put32(wire+8U,UINT32_C(65536));
    put32(wire+32U,UINT32_C(28402305));
    assert(ou_time_accept(&sync,wire,48U,8200U,1)==-1);
    put32(wire+32U,UINT32_C(28402304));
    assert(ou_time_accept(&sync,wire,47U,8200U,1)==-1);
    assert(ou_time_accept(&sync,wire,48U,8200U,1)==0);
    assert(ou_time_now(&sync,8200U,&clock)==0);
    assert(clock.utc_unix_seconds==UINT64_C(2114380800));
    assert(clock.utc_millisecond==600U);
    assert(ou_time_make_request(&sync,8300U,req)==0);
    reply(wire,req,0U); /* wrong calendar epoch under era=1 */
    assert(ou_time_accept(&sync,wire,48U,8400U,1)==-1);
    assert(sync.synchronized==1U); /* previous valid sample survives */
}
int main(void){
    qualification_and_nonce();
    verification_and_clock();
    hostile_packets_and_era();
    puts("PASS: native NTPv4 48-byte requests, nonce/source binding, strict response validation, 2036 era and monotonic UTC");
    return 0;
}
