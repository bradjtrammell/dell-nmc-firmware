#include "ou_update_verify.h"

static uint32_t rotate(uint32_t x,unsigned n){return (x>>n)|(x<<(32U-n));}
static uint32_t load32(const uint8_t *p){
    return ((uint32_t)p[0]<<24U)|((uint32_t)p[1]<<16U)|
           ((uint32_t)p[2]<<8U)|p[3];
}
static void put32(uint8_t *p,uint32_t v){
    p[0]=(uint8_t)(v>>24U);p[1]=(uint8_t)(v>>16U);
    p[2]=(uint8_t)(v>>8U);p[3]=(uint8_t)v;
}
static void put16(uint8_t *p,uint16_t v){
    p[0]=(uint8_t)(v>>8U);p[1]=(uint8_t)v;
}
static void sha_block(ou_sha256_ctx *c,const uint8_t block[64]){
    static const uint32_t k[64]={
        UINT32_C(0x428a2f98),UINT32_C(0x71374491),UINT32_C(0xb5c0fbcf),UINT32_C(0xe9b5dba5),
        UINT32_C(0x3956c25b),UINT32_C(0x59f111f1),UINT32_C(0x923f82a4),UINT32_C(0xab1c5ed5),
        UINT32_C(0xd807aa98),UINT32_C(0x12835b01),UINT32_C(0x243185be),UINT32_C(0x550c7dc3),
        UINT32_C(0x72be5d74),UINT32_C(0x80deb1fe),UINT32_C(0x9bdc06a7),UINT32_C(0xc19bf174),
        UINT32_C(0xe49b69c1),UINT32_C(0xefbe4786),UINT32_C(0x0fc19dc6),UINT32_C(0x240ca1cc),
        UINT32_C(0x2de92c6f),UINT32_C(0x4a7484aa),UINT32_C(0x5cb0a9dc),UINT32_C(0x76f988da),
        UINT32_C(0x983e5152),UINT32_C(0xa831c66d),UINT32_C(0xb00327c8),UINT32_C(0xbf597fc7),
        UINT32_C(0xc6e00bf3),UINT32_C(0xd5a79147),UINT32_C(0x06ca6351),UINT32_C(0x14292967),
        UINT32_C(0x27b70a85),UINT32_C(0x2e1b2138),UINT32_C(0x4d2c6dfc),UINT32_C(0x53380d13),
        UINT32_C(0x650a7354),UINT32_C(0x766a0abb),UINT32_C(0x81c2c92e),UINT32_C(0x92722c85),
        UINT32_C(0xa2bfe8a1),UINT32_C(0xa81a664b),UINT32_C(0xc24b8b70),UINT32_C(0xc76c51a3),
        UINT32_C(0xd192e819),UINT32_C(0xd6990624),UINT32_C(0xf40e3585),UINT32_C(0x106aa070),
        UINT32_C(0x19a4c116),UINT32_C(0x1e376c08),UINT32_C(0x2748774c),UINT32_C(0x34b0bcb5),
        UINT32_C(0x391c0cb3),UINT32_C(0x4ed8aa4a),UINT32_C(0x5b9cca4f),UINT32_C(0x682e6ff3),
        UINT32_C(0x748f82ee),UINT32_C(0x78a5636f),UINT32_C(0x84c87814),UINT32_C(0x8cc70208),
        UINT32_C(0x90befffa),UINT32_C(0xa4506ceb),UINT32_C(0xbef9a3f7),UINT32_C(0xc67178f2)
    };
    uint32_t w[64],a,b,d,e,f,g,h,t1,t2,q;
    uint32_t cc;
    unsigned i;
    for(i=0U;i<16U;++i)w[i]=load32(block+i*4U);
    for(i=16U;i<64U;++i){
        uint32_t x=w[i-15U],y=w[i-2U];
        w[i]=(rotate(y,17U)^rotate(y,19U)^(y>>10U))+w[i-7U]+
              (rotate(x,7U)^rotate(x,18U)^(x>>3U))+w[i-16U];
    }
    a=c->h[0];b=c->h[1];cc=c->h[2];d=c->h[3];
    e=c->h[4];f=c->h[5];g=c->h[6];h=c->h[7];
    for(i=0U;i<64U;++i){
        t1=h+(rotate(e,6U)^rotate(e,11U)^rotate(e,25U))+
            ((e&f)^(~e&g))+k[i]+w[i];
        t2=(rotate(a,2U)^rotate(a,13U)^rotate(a,22U))+
            ((a&b)^(a&cc)^(b&cc));
        h=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;
    }
    c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;
    c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=h;
    for(i=0U;i<64U;++i)w[i]=0U;
    q=0U;(void)q;
}
void ou_sha256_init(ou_sha256_ctx *c){
    size_t i;
    if(!c)return;
    for(i=0U;i<sizeof(*c);++i)((uint8_t *)c)[i]=0U;
    c->h[0]=UINT32_C(0x6a09e667);
    c->h[1]=UINT32_C(0xbb67ae85);
    c->h[2]=UINT32_C(0x3c6ef372);
    c->h[3]=UINT32_C(0xa54ff53a);
    c->h[4]=UINT32_C(0x510e527f);
    c->h[5]=UINT32_C(0x9b05688c);
    c->h[6]=UINT32_C(0x1f83d9ab);
    c->h[7]=UINT32_C(0x5be0cd19);
}
int ou_sha256_update(ou_sha256_ctx *c,const uint8_t *data,size_t count){
    size_t i;
    if(!c||(!data&&count)||count>(size_t)(UINT32_MAX-c->total_bytes)||
       c->used>=64U)return -1;
    for(i=0U;i<count;++i){
        c->block[c->used++]=data[i];
        if(c->used==64U){
            sha_block(c,c->block);
            c->used=0U;
        }
    }
    c->total_bytes+=(uint32_t)count;
    return 0;
}
int ou_sha256_final(ou_sha256_ctx *c,uint8_t digest[32]){
    uint32_t high,low;
    unsigned i;
    if(!c||!digest||c->used>=64U)return -1;
    high=c->total_bytes>>29U;
    low=c->total_bytes<<3U;
    c->block[c->used++]=0x80U;
    if(c->used>56U){
        while(c->used<64U)c->block[c->used++]=0U;
        sha_block(c,c->block);c->used=0U;
    }
    while(c->used<56U)c->block[c->used++]=0U;
    put32(c->block+56U,high);put32(c->block+60U,low);
    sha_block(c,c->block);
    for(i=0U;i<8U;++i)put32(digest+i*4U,c->h[i]);
    for(i=0U;i<64U;++i)c->block[i]=0U;
    c->used=0U;
    return 0;
}
static int same_repo(const char input[sizeof(OU_UPDATE_REPOSITORY)]){
    static const char repo[]=OU_UPDATE_REPOSITORY;
    size_t i;
    for(i=0U;i<sizeof(repo);++i)if(input[i]!=repo[i])return 0;
    return 1;
}
static int nonzero(const uint8_t *data,size_t length){
    size_t i;uint32_t accum=0U;
    for(i=0U;i<length;++i)accum|=data[i];
    return accum!=0U;
}
static int newer(const ou_update_manifest *m,const ou_update_trust *t){
    if(m->major!=t->installed_major)return m->major>t->installed_major;
    if(m->minor!=t->installed_minor)return m->minor>t->installed_minor;
    return m->patch>t->installed_patch;
}
int ou_update_canonical(const ou_update_manifest *m,
                        uint8_t *output,size_t size,size_t *written){
    static const char repo[]=OU_UPDATE_REPOSITORY;
    size_t i,at=0U;
    if(written)*written=0U;
    if(!m||!output||!written||!same_repo(m->repository)||
       m->hardware_id!=OU_UPDATE_HARDWARE_ID||
       m->format_version!=OU_UPDATE_SIGNED_FORMAT_VERSION||
       m->prerelease>1U||m->image_bytes<OU_UPDATE_MIN_IMAGE||
       (m->major==0U&&m->minor==0U&&m->patch==0U)||
       !nonzero(m->sha256,32U)||!nonzero(m->signature,64U)||
       !nonzero(m->key_id,8U)||
       size<OU_UPDATE_MESSAGE_MAX)return -1;
    output[at++]='O';output[at++]='6';output[at++]='U';output[at++]='P';
    output[at++]=m->format_version;
    output[at++]=m->prerelease;
    put16(output+at,m->hardware_id);at+=2U;
    put16(output+at,m->major);at+=2U;
    put16(output+at,m->minor);at+=2U;
    put16(output+at,m->patch);at+=2U;
    put32(output+at,m->image_bytes);at+=4U;
    output[at++]=(uint8_t)(sizeof(repo)-1U);
    for(i=0U;i<sizeof(repo)-1U;++i)output[at++]=(uint8_t)repo[i];
    for(i=0U;i<8U;++i)output[at++]=m->key_id[i];
    for(i=0U;i<32U;++i)output[at++]=m->sha256[i];
    *written=at;
    return 0;
}
int ou_update_begin(ou_update_verifier *out,const ou_update_manifest *m,
                    const ou_update_trust *trust){
    uint8_t message[OU_UPDATE_MESSAGE_MAX];
    size_t length=0U,i;
    if(out)*out=(ou_update_verifier){0};
    if(!out||!m||!trust||!trust->verify_signature||
       trust->board_verified!=1U||trust->flash_layout_verified!=1U||
       trust->boot_recovery_verified!=1U||
       trust->trusted_github_tls_verified!=1U||
       trust->signing_keys_verified!=1U||
       m->image_bytes>trust->verified_application_capacity||
       !newer(m,trust)||
       ou_update_canonical(m,message,sizeof(message),&length)!=0)return -1;
    if(trust->verify_signature(trust->ctx,message,length,
                               m->signature,m->key_id)!=1)return -1;
    out->manifest=*m;
    ou_sha256_init(&out->sha);
    out->verified_manifest=1U;
    out->image_complete=0U;
    out->received=0U;
    for(i=0U;i<sizeof(message);++i)message[i]=0U;
    return 0;
}
int ou_update_feed(ou_update_verifier *v,const uint8_t *image,size_t count){
    if(!v||v->verified_manifest!=1U||v->image_complete||
       (!image&&count)||count>(size_t)(v->manifest.image_bytes-v->received))
        return -1;
    if(ou_sha256_update(&v->sha,image,count)!=0)return -1;
    v->received+=(uint32_t)count;
    return 0;
}
int ou_update_finish(ou_update_verifier *v){
    uint8_t hash[32];
    uint32_t difference=0U;
    size_t i;
    if(!v||v->verified_manifest!=1U||v->image_complete||
       v->received!=v->manifest.image_bytes)return -1;
    if(ou_sha256_final(&v->sha,hash)!=0)return -1;
    for(i=0U;i<32U;++i)difference|=(uint32_t)(hash[i]^v->manifest.sha256[i]);
    for(i=0U;i<32U;++i)hash[i]=0U;
    if(difference)return -1;
    v->image_complete=1U;
    return 0;
}
