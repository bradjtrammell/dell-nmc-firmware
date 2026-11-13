#include "ou_time_sync.h"

static uint32_t u32(const uint8_t *p){
    return ((uint32_t)p[0]<<24U)|((uint32_t)p[1]<<16U)|
           ((uint32_t)p[2]<<8U)|(uint32_t)p[3];
}
static int nonzero(const uint8_t *p,size_t count){
    uint8_t any=0U;size_t i;
    for(i=0U;i<count;++i)any|=p[i];
    return any!=0U;
}
static int same_nonce(const uint8_t *a,const uint8_t *b,size_t count){
    uint32_t diff=0U;size_t i;
    for(i=0U;i<count;++i)diff|=(uint32_t)(a[i]^b[i]);
    return diff==0U;
}
static int trusted(const ou_time_source *s) {
    return s && s->secure_nonce &&
           s->source_acl_qualified==1U &&
           s->isolated_trusted_network==1U &&
           s->monotonic_clock_qualified==1U &&
           s->era_anchored==1U && s->explicit_era<=1U;
}
int ou_time_init(ou_time_sync *sync,const ou_time_source *source){
    if(!sync)return -1;
    *sync=(ou_time_sync){0};
    if(!trusted(source))return -1;
    sync->source=*source;
    return 0;
}
int ou_time_make_request(ou_time_sync *sync,uint32_t now_ms,
                          uint8_t request[OU_NTP_WIRE_BYTES]){
    uint8_t nonce[8],other=0U;
    size_t i;
    if(!sync||!request||!trusted(&sync->source)||sync->outstanding)return -1;
    for(i=0U;i<sizeof(nonce);++i)nonce[i]=0U;
    if(sync->source.secure_nonce(sync->source.context,nonce)!=1||
       !nonzero(nonce,sizeof(nonce))||
       same_nonce(nonce,sync->nonce,sizeof(nonce)))return -1;
    for(i=0U;i<OU_NTP_WIRE_BYTES;++i)request[i]=0U;
    request[0]=0x23U; /* LI=0, VN=4, MODE=client(3) */
    request[2]=6U; /* minimum interval negotiated by external scheduler */
    request[3]=0xecU; /* precision estimate 2^-20 seconds */
    for(i=0U;i<8U;++i){
        request[40U+i]=nonce[i];
        sync->nonce[i]=nonce[i];
        other|=nonce[i];
    }
    if(!other)return -1;
    sync->request_ms=now_ms;
    sync->outstanding=1U;
    return 0;
}
int ou_time_accept(ou_time_sync *sync,const uint8_t *reply,size_t length,
                   uint32_t now_ms,int source_address_verified){
    uint32_t rtt,root_dispersion,tx_sec,tx_frac,recv_sec;
    uint32_t offset_ms,msec,half;
    uint64_t era_seconds,utc;
    uint8_t version,mode,li;
    uint32_t difference=0U;
    size_t i;
    if(!sync||!sync->outstanding||!reply||
       !trusted(&sync->source)||source_address_verified!=1||
       length!=OU_NTP_WIRE_BYTES)return -1;
    rtt=now_ms-sync->request_ms;
    if(rtt>=UINT32_C(0x80000000)||rtt>OU_NTP_MAX_RTT_MS){
        sync->outstanding=0U;
        return -1;
    }
    li=(uint8_t)(reply[0]>>6U);
    version=(uint8_t)((reply[0]>>3U)&7U);
    mode=(uint8_t)(reply[0]&7U);
    if(li==3U || (version!=3U&&version!=4U) ||
       mode!=4U || reply[1]==0U || reply[1]>15U)return -1;
    /* Originate must echo our CSPRNG transmit nonce exactly. */
    for(i=0U;i<8U;++i)difference|=(uint32_t)(reply[24U+i]^sync->nonce[i]);
    if(difference)return -1;
    if(!nonzero(reply+40U,8U)||!nonzero(reply+32U,8U))return -1;
    recv_sec=u32(reply+32U);
    tx_sec=u32(reply+40U);
    tx_frac=u32(reply+44U);
    if(tx_sec<recv_sec || tx_sec-recv_sec>10U)return -1;
    root_dispersion=u32(reply+8U);
    /* Reject servers uncertain by >=5 seconds. 16.16 seconds. */
    if(root_dispersion>=UINT32_C(5)*UINT32_C(65536))return -1;
    offset_ms=(root_dispersion>>16U)*1000U+
              (uint32_t)(((uint64_t)(root_dispersion&65535U)*1000U)>>16U);
    half=rtt/2U;
    era_seconds=((uint64_t)sync->source.explicit_era<<32U)|tx_sec;
    if(era_seconds<OU_NTP_UNIX_OFFSET)return -1;
    utc=era_seconds-OU_NTP_UNIX_OFFSET;
    /* Explicit era anchoring avoids the 2036 NTP wrap ambiguity. */
    if(utc<UINT64_C(1577836800)||utc>UINT64_C(4102444800))return -1;
    msec=(uint32_t)(((uint64_t)tx_frac*1000U)>>32U)+half;
    utc+=(uint64_t)(msec/1000U);
    msec%=1000U;
    sync->last.utc_unix_seconds=utc;
    sync->last.utc_millisecond=msec;
    sync->last.round_trip_ms=rtt;
    sync->last.uncertainty_ms=offset_ms+half+1U;
    sync->last.synchronized_ms=now_ms;
    sync->last.source_verified=1U;
    sync->synchronized=1U;
    sync->outstanding=0U;
    return 0;
}
int ou_time_now(const ou_time_sync *sync,uint32_t now_ms,
                ou_time_sample *estimated){
    uint32_t elapsed,total;
    if(!sync||!estimated||!sync->synchronized||
       !sync->last.source_verified)return -1;
    elapsed=now_ms-sync->last.synchronized_ms;
    if(elapsed>=UINT32_C(0x80000000)||elapsed>OU_NTP_MAX_AGE_MS)return -1;
    *estimated=sync->last;
    total=estimated->utc_millisecond+elapsed;
    estimated->utc_unix_seconds+=(uint64_t)(total/1000U);
    estimated->utc_millisecond=total%1000U;
    return 0;
}
