#include "ou_vlan_record.h"

static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static uint16_t get16(const uint8_t *p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static uint32_t get32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void copy(uint8_t *d,const uint8_t*s,size_t n){size_t i;for(i=0U;i<n;++i)d[i]=s[i];}
static int all_zero(const uint8_t*s,size_t n){size_t i;for(i=0U;i<n;++i)if(s[i]!=0U)return 0;return 1;}
static uint32_t crc32(const uint8_t *p,size_t n){size_t i;unsigned bit;uint32_t crc=UINT32_MAX;for(i=0U;i<n;++i){
    uint8_t v=(i>=20U&&i<24U)?0U:p[i];
    crc^=v;for(bit=0U;bit<8U;++bit)crc=(crc>>1)^((crc&1U)?UINT32_C(0xedb88320):0U);
}return crc^UINT32_MAX;}
int ou_vlan_record_encode(const ou_vlan_table *policy,uint8_t *out,
                          size_t capacity,size_t *written) {
    size_t count,i,j,at,total;
    uint8_t flags;
    if(!written)return -1;
    *written=0U;
    if(!policy||!out||!ou_vlan_validate(policy))return -1;
    count=policy->count;
    total=OU_VLAN_RECORD_HEADER+count*OU_VLAN_RECORD_INTERFACE;
    if(total>OU_JOURNAL_MAX_PAYLOAD||capacity<total)return -1;
    for(i=0U;i<total;++i)out[i]=0U;
    out[0]='O';out[1]='U';out[2]='V';out[3]='L';
    out[4]=OU_VLAN_RECORD_VERSION;
    out[5]=(uint8_t)count;
    flags=(uint8_t)(policy->trunk_enabled|
        (policy->native_enabled<<1)|
        (policy->accept_tagged_native<<2)|
        (policy->accept_priority_tags<<3)|
        (policy->priority_reply_tagged<<4));
    out[6]=flags;
    put16(out+8U,policy->native_vid);
    copy(out+10U,policy->mac,6U);
    put16(out+16U,(uint16_t)total);
    at=OU_VLAN_RECORD_HEADER;
    for(i=0U;i<count;++i) {
        const ou_vlan_interface *c=&policy->interfaces[i];
        put16(out+at,c->vid);at+=2U;
        copy(out+at,c->ipv4,4U);at+=4U;
        out[at++]=c->prefix_length;
        copy(out+at,c->gateway,4U);at+=4U;
        put32(out+at,c->allowed_services);at+=4U;
        out[at++]=c->tx_pcp;
        out[at++]=c->allow_dei;
        out[at++]=c->allow_local_subnet;
        out[at++]=c->acl_count;
        for(j=0U;j<OU_VLAN_ACL_MAX;++j) {
            if(j<c->acl_count) {
                copy(out+at,c->acl[j].address,4U);
                out[at+4U]=c->acl[j].prefix_length;
            }
            at+=5U;
        }
    }
    if(at!=total)return -1;
    put32(out+20U,crc32(out,total));
    *written=total;
    return 0;
}
int ou_vlan_record_decode(ou_vlan_table *policy,const uint8_t *data,
                          size_t length) {
    size_t i,j,at;
    uint8_t flags,count;
    if(!policy)return -1;
    *policy=(ou_vlan_table){0};
    if(!data||length<OU_VLAN_RECORD_HEADER||length>OU_JOURNAL_MAX_PAYLOAD||
       data[0]!='O'||data[1]!='U'||data[2]!='V'||data[3]!='L'||
       data[4]!=OU_VLAN_RECORD_VERSION||data[7]!=0U||
       data[18]!=0U||data[19]!=0U)return -1;
    count=data[5];flags=data[6];
    if(count==0U||count>OU_VLAN_MAX_INTERFACES||
       (flags&0xe0U)!=0U||
       length!=OU_VLAN_RECORD_HEADER+(size_t)count*OU_VLAN_RECORD_INTERFACE||
       get16(data+16U)!=length||get32(data+20U)!=crc32(data,length))return -1;
    policy->count=count;
    policy->trunk_enabled=flags&1U;
    policy->native_enabled=(uint8_t)((flags>>1)&1U);
    policy->accept_tagged_native=(uint8_t)((flags>>2)&1U);
    policy->accept_priority_tags=(uint8_t)((flags>>3)&1U);
    policy->priority_reply_tagged=(uint8_t)((flags>>4)&1U);
    policy->native_vid=get16(data+8U);
    copy(policy->mac,data+10U,6U);
    at=OU_VLAN_RECORD_HEADER;
    for(i=0U;i<count;++i) {
        ou_vlan_interface *c=&policy->interfaces[i];
        c->vid=get16(data+at);at+=2U;
        copy(c->ipv4,data+at,4U);at+=4U;
        c->prefix_length=data[at++];
        copy(c->gateway,data+at,4U);at+=4U;
        c->allowed_services=get32(data+at);at+=4U;
        c->tx_pcp=data[at++];
        c->allow_dei=data[at++];
        c->allow_local_subnet=data[at++];
        c->acl_count=data[at++];
        for(j=0U;j<OU_VLAN_ACL_MAX;++j) {
            if(j<c->acl_count) {
                copy(c->acl[j].address,data+at,4U);
                c->acl[j].prefix_length=data[at+4U];
            } else if(!all_zero(data+at,5U))goto error;
            at+=5U;
        }
    }
    if(at!=length||!ou_vlan_validate(policy))goto error;
    return 0;
error:
    *policy=(ou_vlan_table){0};
    return -1;
}
int ou_vlan_journal_save(const ou_journal_io *io,const ou_vlan_table *policy,
                         uint8_t *scratch,size_t capacity,
                         ou_journal_record *meta) {
    size_t written=0U;
    if(!io||!scratch||!meta||capacity<OU_VLAN_RECORD_MAX_SIZE||
       ou_vlan_record_encode(policy,scratch,capacity,&written)!=0)return -1;
    return ou_journal_commit(io,scratch,written,meta);
}
int ou_vlan_journal_load(const ou_journal_io *io,ou_vlan_table *policy,
                         uint8_t *scratch,size_t capacity,
                         ou_journal_record *meta) {
    size_t received=0U;
    if(!policy)return -1;
    *policy=(ou_vlan_table){0};
    if(!io||!scratch||!meta||capacity<OU_VLAN_RECORD_MAX_SIZE||
       capacity>OU_JOURNAL_MAX_PAYLOAD||
       ou_journal_load(io,scratch,capacity,&received,meta)!=0)return -1;
    if (ou_vlan_record_decode(policy,scratch,received)!=0) {
        *meta=(ou_journal_record){0};
        return -1;
    }
    return 0;
}
