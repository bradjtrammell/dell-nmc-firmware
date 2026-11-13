#include "ou_update_verify.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned signature_checks;
static int authentic(void *ctx,const uint8_t *message,size_t length,
                      const uint8_t signature[64],const uint8_t key[8]){
    static const uint8_t expected[]="bradjtrammell/dell-nmc-firmware";
    (void)ctx;++signature_checks;
    if(length<sizeof(expected)+24U ||
       message[0]!='O'||message[1]!='6'||message[2]!='U'||message[3]!='P'||
       signature[0]!=0xA5U||key[0]!=0x37U)return 0;
    {
        size_t i,match=0U;
        for(i=0U;i+sizeof(expected)-1U<=length;++i){
            if(memcmp(message+i,expected,sizeof(expected)-1U)==0)match=1U;
        }
        return match?1:0;
    }
}
static ou_update_manifest manifest(void){
    static const uint8_t digest[32]={
      0xf3,0x02,0x95,0x7d,0xa5,0x22,0x09,0x38,
      0xa7,0xe3,0xe5,0x1a,0x87,0x18,0xc7,0x9b,
      0x9e,0x00,0xdc,0x13,0xab,0x21,0x19,0xe8,
      0xcf,0xc9,0x78,0xf0,0x41,0x72,0x03,0x82
    };
    ou_update_manifest m={0};
    m.hardware_id=OU_UPDATE_HARDWARE_ID;
    m.major=0U;m.minor=6U;m.patch=1U;
    m.image_bytes=4096U;
    m.format_version=OU_UPDATE_SIGNED_FORMAT_VERSION;
    memcpy(m.repository,OU_UPDATE_REPOSITORY,sizeof(OU_UPDATE_REPOSITORY));
    memcpy(m.sha256,digest,sizeof(digest));
    m.key_id[0]=0x37U;
    memset(m.signature,0xA5,sizeof(m.signature));
    return m;
}
static ou_update_trust trust(void){
    ou_update_trust t={0};
    t.verify_signature=authentic;
    t.installed_major=0U;t.installed_minor=6U;t.installed_patch=0U;
    t.verified_application_capacity=8192U;
    t.board_verified=1U;t.flash_layout_verified=1U;
    t.boot_recovery_verified=1U;
    t.trusted_github_tls_verified=1U;
    t.signing_keys_verified=1U;
    return t;
}
static void sha_hex(const uint8_t digest[32],const char *expected){
    static const char hex[]="0123456789abcdef";
    unsigned i;
    char actual[65];
    for(i=0U;i<32U;++i){
        actual[i*2U]=hex[digest[i]>>4U];
        actual[i*2U+1U]=hex[digest[i]&15U];
    }
    actual[64]='\0';
    assert(strcmp(actual,expected)==0);
}
static void sha_vectors(void){
    ou_sha256_ctx c;
    uint8_t digest[32],many[1000];
    unsigned i;
    ou_sha256_init(&c);
    assert(ou_sha256_update(&c,(const uint8_t *)"abc",3U)==0);
    assert(ou_sha256_final(&c,digest)==0);
    sha_hex(digest,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    ou_sha256_init(&c);
    memset(many,'a',sizeof(many));
    for(i=0U;i<1000U;++i)
        assert(ou_sha256_update(&c,many,sizeof(many))==0);
    assert(ou_sha256_final(&c,digest)==0);
    sha_hex(digest,"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    assert(ou_sha256_update(&c,0,1U)==-1);
}
static void policy_and_full_image(void){
    ou_update_manifest m=manifest();
    ou_update_trust t=trust();
    ou_update_verifier v;
    uint8_t raw[4096U],message[OU_UPDATE_MESSAGE_MAX];
    size_t length=0U;
    memset(raw,'Z',sizeof(raw));
    signature_checks=0U;
    assert(ou_update_canonical(&m,message,sizeof(message),&length)==0);
    assert(length>64U&&length<OU_UPDATE_MESSAGE_MAX);
    assert(ou_update_begin(&v,&m,&t)==0);
    assert(signature_checks==1U&&v.verified_manifest==1U);
    assert(ou_update_finish(&v)==-1);
    assert(ou_update_feed(&v,raw,1000U)==0);
    assert(ou_update_feed(&v,raw+1000U,3096U)==0);
    assert(ou_update_feed(&v,raw,1U)==-1);
    assert(ou_update_finish(&v)==0&&v.image_complete==1U);
    assert(ou_update_finish(&v)==-1);
    assert(ou_update_feed(&v,raw,1U)==-1);
    assert(ou_update_begin(&v,&m,&t)==0);
    raw[2]^=1U;
    assert(ou_update_feed(&v,raw,sizeof(raw))==0);
    assert(ou_update_finish(&v)==-1);
    assert(v.image_complete==0U);
    m.image_bytes=9000U;
    assert(ou_update_begin(&v,&m,&t)==-1);
    assert(v.verified_manifest==0U);
    m=manifest();m.hardware_id=0x1234U;
    assert(ou_update_begin(&v,&m,&t)==-1);
    m=manifest();m.repository[0]='D';
    assert(ou_update_begin(&v,&m,&t)==-1);
    m=manifest();m.major=0U;m.minor=6U;m.patch=0U;
    assert(ou_update_begin(&v,&m,&t)==-1);
    m=manifest();m.format_version=99U;
    assert(ou_update_begin(&v,&m,&t)==-1);
    m=manifest();m.signature[0]=0U;
    assert(ou_update_begin(&v,&m,&t)==-1);
    m=manifest();
    t.board_verified=0U;
    assert(ou_update_begin(&v,&m,&t)==-1);
    t.board_verified=1U;t.flash_layout_verified=0U;
    assert(ou_update_begin(&v,&m,&t)==-1);
    t.flash_layout_verified=1U;t.boot_recovery_verified=0U;
    assert(ou_update_begin(&v,&m,&t)==-1);
    t.boot_recovery_verified=1U;t.trusted_github_tls_verified=0U;
    assert(ou_update_begin(&v,&m,&t)==-1);
    t.trusted_github_tls_verified=1U;t.signing_keys_verified=0U;
    assert(ou_update_begin(&v,&m,&t)==-1);
    t.signing_keys_verified=1U;
    t.verify_signature=0;
    assert(ou_update_begin(&v,&m,&t)==-1);
}
int main(void){
    sha_vectors();
    policy_and_full_image();
    puts("PASS: real SHA256 vectors, pinned GitHub/H910P signed manifest gate, streamed integrity, downgrade and flash/recovery refusal");
    return 0;
}
