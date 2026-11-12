#include "ou_ipv4_wire.h"
static uint16_t be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static void putbe16(uint8_t *p, uint16_t value) {
    p[0]=(uint8_t)(value>>8); p[1]=(uint8_t)value;
}
static void copy(uint8_t *dst, const uint8_t *src, size_t n) {
    while(n--) *dst++=*src++;
}
static void zero(uint8_t *dst,size_t n) {
    while(n--) *dst++=0U;
}
static int same(const uint8_t *a,const uint8_t *b,size_t n) {
    size_t i;for(i=0;i<n;++i)if(a[i]!=b[i])return 0;return 1;
}
static int all(const uint8_t *src,size_t n,uint8_t b) {
    size_t i;for(i=0;i<n;++i)if(src[i]!=b)return 0;return 1;
}
static uint16_t checksum(const uint8_t *data,size_t size) {
    uint32_t sum=0;size_t i;
    for(i=0;i+1U<size;i+=2U)sum+=(uint32_t)be16(data+i);
    if(i<size)sum+=(uint32_t)data[i]<<8;
    while(sum>>16)sum=(sum&65535U)+(sum>>16);
    return (uint16_t)~sum;
}
static int valid(const ou_net_identity *n) {
    if(!n || n->enabled!=1U || (n->mac[0]&1U) ||
       all(n->mac,6U,0U) || all(n->mac,6U,255U) ||
       n->ipv4[0]==0U || n->ipv4[0]==127U ||
       n->ipv4[0]>=224U || all(n->ipv4,4U,255U))return 0;
    return 1;
}
int ou_net_reply(const ou_net_identity *identity,const uint8_t *frame,
                 size_t frame_len,uint8_t *reply,size_t capacity,
                 size_t *reply_len) {
    const uint8_t *ip; size_t i,ihl,total,icmp_len,out_len;
    uint16_t typ;
    if(!reply_len)return -1;
    *reply_len=0U;
    if(!valid(identity)||!frame||!reply||frame_len<42U||
       frame_len>OU_NET_MAX_FRAME||capacity<OU_NET_MIN_FRAME||
       (frame[6]&1U)||all(frame+6,6U,0U))return -1;
    typ=be16(frame+12U);
    if(typ==UINT16_C(0x0806)) {
        if(frame_len<42U || (!same(frame+0U,identity->mac,6U) &&
           !all(frame+0U,6U,255U)))return 0;
        if(be16(frame+14U)!=1U||be16(frame+16U)!=UINT16_C(0x0800)||
           frame[18U]!=6U||frame[19U]!=4U||be16(frame+20U)!=1U||
           !same(frame+22U,frame+6U,6U)||
           !same(frame+38U,identity->ipv4,4U)||
           all(frame+28U,4U,0U))return 0;
        zero(reply,OU_NET_MIN_FRAME);
        copy(reply,frame+6U,6U);
        copy(reply+6U,identity->mac,6U);
        putbe16(reply+12U,UINT16_C(0x0806));
        putbe16(reply+14U,1U);putbe16(reply+16U,UINT16_C(0x0800));
        reply[18]=6U;reply[19]=4U;putbe16(reply+20U,2U);
        copy(reply+22U,identity->mac,6U);
        copy(reply+28U,identity->ipv4,4U);
        copy(reply+32U,frame+22U,6U);
        copy(reply+38U,frame+28U,4U);
        *reply_len=OU_NET_MIN_FRAME;
        return 1;
    }
    if(typ!=UINT16_C(0x0800)||frame_len<14U+20U+8U)return 0;
    ip=frame+14U;
    if((ip[0]>>4)!=4U)return 0;
    ihl=(size_t)(ip[0]&15U)*4U;
    if(ihl<20U||ihl>60U||frame_len<14U+ihl+8U)return 0;
    total=be16(ip+2U);
    if(total<ihl+8U||total>OU_NET_MTU||total>frame_len-14U)return 0;
    if(be16(ip+6U)&UINT16_C(0x3fff))return 0;
    if(ip[8U]==0U||ip[9U]!=1U||checksum(ip,ihl)!=0U||
       !same(ip+16U,identity->ipv4,4U)||ip[12U]==0U||
       ip[12U]>=224U||all(ip+12U,4U,255U))return 0;
    if(!same(frame+0U,identity->mac,6U))return 0;
    icmp_len=total-ihl;
    if(ip[ihl]!=8U||ip[ihl+1U]!=0U||
       checksum(ip+ihl,icmp_len)!=0U)return 0;
    out_len=14U+total;
    if(out_len<OU_NET_MIN_FRAME)out_len=OU_NET_MIN_FRAME;
    if(out_len>capacity)return -1;
    zero(reply,out_len);
    copy(reply,frame+6U,6U);
    copy(reply+6U,identity->mac,6U);
    putbe16(reply+12U,UINT16_C(0x0800));
    copy(reply+14U,ip,total);
    copy(reply+14U+12U,identity->ipv4,4U);
    copy(reply+14U+16U,ip+12U,4U);
    reply[14U+8U]=64U;
    reply[14U+10U]=0U;reply[14U+11U]=0U;
    putbe16(reply+14U+10U,checksum(reply+14U,ihl));
    reply[14U+ihl]=0U;
    reply[14U+ihl+2U]=0U;reply[14U+ihl+3U]=0U;
    putbe16(reply+14U+ihl+2U,checksum(reply+14U+ihl,icmp_len));
    for(i=14U+total;i<out_len;i++)reply[i]=0U;
    *reply_len=out_len;
    return 1;
}
