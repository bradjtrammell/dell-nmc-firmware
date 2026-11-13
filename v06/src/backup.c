#include "ou_backup.h"

/* Header: O6BK, version, sections=3, flags=0, reserved=0,
 * total(16be), config(16be), shutdown(16be), weekly(16be),
 * SHA256(header[0..15] || payload). The digest excludes its own field.
 * Sections have a fixed order; strict lengths prevent ambiguities.
 * The digest is not an authentication mechanism. */
static void put16(uint8_t *p,uint16_t v){
    p[0]=(uint8_t)(v>>8U);p[1]=(uint8_t)v;
}
static uint16_t get16(const uint8_t *p){
    return (uint16_t)(((uint16_t)p[0]<<8U)|p[1]);
}
static int hash(const uint8_t *head,const uint8_t *payload,size_t len,
                uint8_t out[OU_UPDATE_SHA256_BYTES]){
    ou_sha256_ctx ctx;
    ou_sha256_init(&ctx);
    if(ou_sha256_update(&ctx,head,16U)!=0 ||
       ou_sha256_update(&ctx,payload,len)!=0 ||
       ou_sha256_final(&ctx,out)!=0)return -1;
    return 0;
}
int ou_backup_export(const ou_backup_snapshot *snapshot,
                     ou_backup_permission_fn authorize,void *context,
                     uint8_t *output,size_t capacity,size_t *written){
    uint8_t temp[OU_BACKUP_MAX_BYTES];
    size_t config_length=0U,at;
    unsigned i;
    if(written)*written=0U;
    if(!snapshot||!authorize||authorize(context)!=1||!output||
       !written||capacity<OU_BACKUP_HEADER_BYTES+
                             OU_SHUTDOWN_RECORD_BYTES+
                             OU_WEEK_RECORD_BYTES||
       ou_config_model_validate(&snapshot->configuration)!=0 ||
       ou_shutdown_policy_validate(&snapshot->shutdown)!=0 ||
       ou_weekly_schedule_validate(&snapshot->weekly)!=0)return -1;
    for(i=0U;i<OU_BACKUP_HEADER_BYTES;++i)temp[i]=0U;
    at=OU_BACKUP_HEADER_BYTES;
    if(ou_config_model_encode(&snapshot->configuration,
                              temp+at,
                              OU_BACKUP_MAX_BYTES-at,
                              &config_length)!=0 ||
       config_length==0U ||config_length>OU_CONFIG_RECORD_MAX)
        return -1;
    at+=config_length;
    if(ou_shutdown_encode(&snapshot->shutdown,temp+at,
                          OU_BACKUP_MAX_BYTES-at)!=
                          (int)OU_SHUTDOWN_RECORD_BYTES)return -1;
    at+=OU_SHUTDOWN_RECORD_BYTES;
    if(ou_weekly_schedule_encode(&snapshot->weekly,temp+at,
                                 OU_BACKUP_MAX_BYTES-at)!=
                                 (int)OU_WEEK_RECORD_BYTES)return -1;
    at+=OU_WEEK_RECORD_BYTES;
    if(at>capacity||at>UINT16_MAX)return -1;
    temp[0]='O';temp[1]='6';temp[2]='B';temp[3]='K';
    temp[4]=OU_BACKUP_VERSION;temp[5]=3U;
    put16(temp+8U,(uint16_t)at);
    put16(temp+10U,(uint16_t)config_length);
    put16(temp+12U,(uint16_t)OU_SHUTDOWN_RECORD_BYTES);
    put16(temp+14U,(uint16_t)OU_WEEK_RECORD_BYTES);
    if(hash(temp,temp+OU_BACKUP_HEADER_BYTES,
            at-OU_BACKUP_HEADER_BYTES,temp+16U)!=0)return -1;
    for(i=0U;i<(unsigned)at;++i)output[i]=temp[i];
    *written=at;
    return 0;
}
int ou_backup_preview(ou_backup_snapshot *destination,
                      const uint8_t *input,size_t length){
    ou_backup_snapshot decoded;
    uint8_t digest[OU_UPDATE_SHA256_BYTES];
    size_t c,s,w,at;
    uint32_t mismatch=0U;
    unsigned i;
    if(!destination||!input||
       length<OU_BACKUP_HEADER_BYTES+OU_SHUTDOWN_RECORD_BYTES+
              OU_WEEK_RECORD_BYTES ||
       length>OU_BACKUP_MAX_BYTES||
       input[0]!='O'||input[1]!='6'||input[2]!='B'||input[3]!='K'||
       input[4]!=OU_BACKUP_VERSION||input[5]!=3U||
       input[6]!=0U||input[7]!=0U||
       get16(input+8U)!=(uint16_t)length)return -1;
    c=get16(input+10U);
    s=get16(input+12U);
    w=get16(input+14U);
    if(c==0U||c>OU_CONFIG_RECORD_MAX||
       s!=OU_SHUTDOWN_RECORD_BYTES||w!=OU_WEEK_RECORD_BYTES||
       OU_BACKUP_HEADER_BYTES+c+s+w!=length ||
       hash(input,input+OU_BACKUP_HEADER_BYTES,
            length-OU_BACKUP_HEADER_BYTES,digest)!=0)return -1;
    for(i=0U;i<OU_UPDATE_SHA256_BYTES;++i)
        mismatch|=(uint32_t)(digest[i]^input[16U+i]);
    if(mismatch)return -1;
    at=OU_BACKUP_HEADER_BYTES;
    ou_config_model_init(&decoded.configuration);
    if(ou_config_model_decode(&decoded.configuration,input+at,c)!=0)
        return -1;
    at+=c;
    ou_shutdown_policy_init(&decoded.shutdown);
    if(ou_shutdown_decode(&decoded.shutdown,input+at,s)!=0)
        return -1;
    at+=s;
    ou_weekly_schedule_init(&decoded.weekly);
    if(ou_weekly_schedule_decode(&decoded.weekly,input+at,w)!=0)
        return -1;
    *destination=decoded;
    return 0;
}
