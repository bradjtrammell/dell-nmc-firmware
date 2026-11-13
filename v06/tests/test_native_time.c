#include "ou_native_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned nonce_sequence;
static int nonce(void *ctx,uint8_t out[8]){
    unsigned i;(void)ctx;
    for(i=0U;i<8U;++i)out[i]=(uint8_t)(0x21U+nonce_sequence+i);
    ++nonce_sequence;
    return 1;
}
static void put32(uint8_t *p,uint32_t value){
    p[0]=(uint8_t)(value>>24U);p[1]=(uint8_t)(value>>16U);
    p[2]=(uint8_t)(value>>8U);p[3]=(uint8_t)value;
}
static void get(const char *path,uint32_t now,int code,const char *body){
    char request[160],response[OU_HTTP_MAX_RESPONSE+1U],prefix[24];
    size_t length=0U;
    int n=snprintf(request,sizeof(request),"GET %s HTTP/1.0\r\n\r\n",path);
    assert(n>0&&(size_t)n<sizeof(request));
    assert(ou_native_service_http((const uint8_t *)request,(size_t)n,now,response,
                                  sizeof(response)-1U,&length)==0);
    response[length]=0U;
    (void)snprintf(prefix,sizeof(prefix),"HTTP/1.1 %d",code);
    assert(strstr(response,prefix)==response);
    assert(strstr(response,body)!=0);
}
int main(void){
    ou_time_source source={0};
    uint8_t request[OU_NTP_WIRE_BYTES],reply[OU_NTP_WIRE_BYTES];
    ou_native_service_init();
    get("/api/v1/time/capabilities",0U,200,"\"ntpWireClientCompiled\":true");
    get("/api/v1/time/status",0U,503,"TIME_NOT_SYNCHRONIZED");
    assert(ou_native_service_time_request(1000U,request)==-1);
    source.secure_nonce=nonce;
    source.source_acl_qualified=1U;
    source.isolated_trusted_network=1U;
    source.monotonic_clock_qualified=1U;
    source.era_anchored=1U;
    source.explicit_era=0U;
    assert(ou_native_service_time_bind(&source)==0);
    assert(ou_native_service_time_request(1000U,request)==0);
    memset(reply,0,sizeof(reply));
    reply[0]=0x24U;reply[1]=2U;
    put32(reply+8U,65536U);
    memcpy(reply+24U,request+40U,8U);
    put32(reply+32U,3913056000U);
    put32(reply+40U,3913056000U);
    assert(ou_native_service_time_response(reply,sizeof(reply),1200U,0)==-1);
    get("/api/v1/time/status",1200U,503,"TIME_NOT_SYNCHRONIZED");
    assert(ou_native_service_time_response(reply,sizeof(reply),1200U,1)==0);
    get("/api/v1/time/status",1300U,200,"\"utcUnixSeconds\":1704067200");
    get("/api/v1/time/status",1300U,200,"\"utcMilliseconds\":200");
    get("/api/v1/time/status",1300U,200,"\"sourceVerified\":true");
    get("/api/v1/time/status",1300U,200,"\"rtcSet\":false");
    get("/api/v1/time/status",1201U+OU_NTP_MAX_AGE_MS,503,"TIME_NOT_SYNCHRONIZED");
    ou_native_service_init();
    get("/api/v1/time/status",0U,503,"TIME_NOT_SYNCHRONIZED");
    puts("PASS: native NTP service source qualification, bootstrap, truthful UTC JSON, stale clock withdrawal and restart");
    return 0;
}
