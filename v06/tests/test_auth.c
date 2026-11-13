#include "ou_auth.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct {
    uint32_t seed;
    int fail_random;
    int zero_random;
    int invalid_role;
    unsigned verifications;
} fake;
static int random_bytes(void *ctx,uint8_t *out,size_t length){
    size_t i;(void)ctx;
    if(fake.fail_random)return -1;
    for(i=0U;i<length;++i)
        out[i]=fake.zero_random?0U:(uint8_t)((fake.seed+i*29U)&255U);
    ++fake.seed;
    return 1;
}
static int verify_password(void *ctx,const char *username,
                           const uint8_t *password,size_t length,
                           uint8_t *role){
    static const uint8_t expected[]="CorrectHorse42!";
    (void)ctx;++fake.verifications;
    if(length!=sizeof(expected)-1U ||
       memcmp(password,expected,sizeof(expected)-1U)!=0)return -1;
    if(strcmp(username,"admin")==0)*role=OU_AUTH_ADMIN;
    else if(strcmp(username,"operator")==0)*role=OU_AUTH_OPERATOR;
    else if(strcmp(username,"reader")==0)*role=OU_AUTH_VIEWER;
    else return -1;
    if(fake.invalid_role)*role=99U;
    return 1;
}
static ou_auth_provider provider(void) {
    ou_auth_provider p={0};
    p.csprng=random_bytes;
    p.verify_password_kdf=verify_password;
    p.entropy_qualified=1U;
    p.credential_store_qualified=1U;
    p.tls_qualified=1U;
    return p;
}
static void new_state(ou_auth_state *s){
    ou_auth_provider p=provider();
    memset(&fake,0,sizeof(fake));
    fake.seed=7U;
    ou_auth_init(s,&p);
    assert(s->initialized==1U);
}
static int login(ou_auth_state *s,const char *user,uint32_t time,
                 ou_auth_grant *g){
    static const uint8_t password[]="CorrectHorse42!";
    return ou_auth_login(s,user,password,sizeof(password)-1U,time,1U,g);
}
static void qualification(void) {
    ou_auth_state s;
    ou_auth_provider p=provider();
    ou_auth_grant grant;
    p.entropy_qualified=0U;
    ou_auth_init(&s,&p);
    assert(s.initialized==0U);
    assert(login(&s,"admin",0U,&grant)==-1);
    p.entropy_qualified=1U;
    p.tls_qualified=0U;ou_auth_init(&s,&p);assert(!s.initialized);
    p.tls_qualified=1U;
    p.credential_store_qualified=0U;
    ou_auth_init(&s,&p);assert(!s.initialized);
    p.credential_store_qualified=1U;p.csprng=0;
    ou_auth_init(&s,&p);assert(!s.initialized);
    new_state(&s);
    assert(ou_auth_login(&s,"admin",(const uint8_t *)"short",5U,
                         0U,1U,&grant)==-1);
    assert(s.failures==1U);
    assert(login(&s,"admin",10U,&grant)==0);
    assert(grant.role==OU_AUTH_ADMIN);
    assert(ou_auth_login(&s,"admin",(const uint8_t *)"CorrectHorse42!",
                         15U,20U,0U,&grant)==-1);
    assert(grant.role==0U);
    ou_auth_revoke_all(&s);
}
static void permissions_and_replay(void){
    ou_auth_state s;
    ou_auth_grant reader,operator,admin;
    uint8_t incorrect[OU_AUTH_TOKEN_BYTES];
    new_state(&s);
    assert(login(&s,"reader",100U,&reader)==0);
    assert(login(&s,"operator",200U,&operator)==0);
    assert(login(&s,"admin",300U,&admin)==0);
    assert(ou_auth_check(&s,reader.token,0,0U,OU_AUTH_READ,500U,1U)==1);
    assert(ou_auth_check(&s,reader.token,reader.csrf,1U,
                         OU_AUTH_CONFIG_WRITE,600U,1U)==0);
    assert(ou_auth_check(&s,operator.token,operator.csrf,1U,
                         OU_AUTH_CONFIG_WRITE,600U,1U)==1);
    assert(ou_auth_check(&s,operator.token,operator.csrf,1U,
                         OU_AUTH_CONFIG_WRITE,700U,1U)==0);
    assert(ou_auth_check(&s,operator.token,operator.csrf,0U,
                         OU_AUTH_CONFIG_WRITE,700U,1U)==0);
    assert(ou_auth_check(&s,operator.token,operator.csrf,2U,
                         OU_AUTH_POWER_CONTROL,700U,1U)==0);
    assert(ou_auth_check(&s,admin.token,admin.csrf,1U,
                         OU_AUTH_POWER_CONTROL,700U,1U)==1);
    assert(ou_auth_check(&s,admin.token,admin.csrf,2U,
                         OU_AUTH_FIRMWARE_UPDATE,800U,1U)==1);
    memset(incorrect,0xf3,sizeof(incorrect));
    assert(ou_auth_check(&s,admin.token,incorrect,3U,
                         OU_AUTH_CONFIG_WRITE,810U,1U)==0);
    assert(ou_auth_check(&s,admin.token,0,3U,
                         OU_AUTH_CONFIG_WRITE,810U,1U)==0);
    assert(ou_auth_check(&s,admin.token,admin.csrf,3U,
                         OU_AUTH_CONFIG_WRITE,820U,0U)==0);
    assert(ou_auth_check(&s,admin.token,admin.csrf,3U,
                         (enum ou_auth_permission)99,820U,1U)==0);
    assert(ou_auth_check(&s,admin.token,admin.csrf,3U,
                         OU_AUTH_CONFIG_WRITE,830U,1U)==1);
    assert(ou_auth_check(&s,admin.token,admin.csrf,3U,
                         OU_AUTH_CONFIG_WRITE,831U,1U)==0);
    assert(ou_auth_logout(&s,reader.token,reader.csrf,1U,900U,1U)==0);
    assert(ou_auth_check(&s,reader.token,0,0U,OU_AUTH_READ,900U,1U)==0);
    assert(ou_auth_logout(&s,operator.token,incorrect,2U,950U,1U)==-1);
    assert(ou_auth_check(&s,operator.token,0,0U,OU_AUTH_READ,960U,1U)==1);
    assert(ou_auth_revoke_sessions(&s,operator.token,operator.csrf,2U,
                                   1000U,1U)==-1);
    assert(ou_auth_revoke_sessions(&s,admin.token,admin.csrf,4U,
                                   1000U,1U)==0);
    assert(ou_auth_check(&s,admin.token,0,0U,OU_AUTH_READ,1001U,1U)==0);
    assert(ou_auth_check(&s,operator.token,0,0U,OU_AUTH_READ,1001U,1U)==0);
}
static void lockout_and_entropy(void) {
    ou_auth_state s;
    ou_auth_grant grant;
    const uint8_t wrong[]="BadPassword123";
    uint32_t i;
    new_state(&s);
    for(i=0U;i<OU_AUTH_MAX_FAILURES;++i)
        assert(ou_auth_login(&s,"admin",wrong,sizeof(wrong)-1U,
                             i*1000U,1U,&grant)==-1);
    assert(s.lockout_active==1U);
    assert(login(&s,"admin",64000U-1U,&grant)==-1);
    assert(login(&s,"admin",64000U,&grant)==0);
    assert(s.failures==0U);
    ou_auth_revoke_all(&s);
    fake.fail_random=1;
    assert(login(&s,"admin",65000U,&grant)==-1);
    assert(grant.role==0U);
    fake.fail_random=0;
    fake.zero_random=1;
    assert(login(&s,"admin",66000U,&grant)==-1);
    assert(grant.role==0U);
    fake.zero_random=0;
    assert(login(&s,"admin",67000U,&grant)==0);
    /* The deterministic test source must be recognized as unsafe if it
     * repeats tokens from an existing session. */
    fake.seed-=2U;
    assert(login(&s,"admin",68000U,&grant)==-1);
    assert(grant.role==0U);
}
static void expiry_and_capacity(void) {
    ou_auth_state s;
    ou_auth_grant sessions[OU_AUTH_SESSIONS];
    unsigned i;
    new_state(&s);
    for(i=0U;i<OU_AUTH_SESSIONS;++i)
        assert(login(&s,"admin",100U,&sessions[i])==0);
    {
      ou_auth_grant extra;
      assert(login(&s,"admin",101U,&extra)==-1);
      assert(extra.role==0U);
    }
    assert(ou_auth_check(&s,sessions[0].token,0,0U,
                         OU_AUTH_READ,100U+OU_AUTH_IDLE_MS-1U,1U)==1);
    assert(ou_auth_check(&s,sessions[0].token,0,0U,
                         OU_AUTH_READ,100U+OU_AUTH_IDLE_MS+200U,1U)==1);
    assert(ou_auth_check(&s,sessions[1].token,0,0U,
                         OU_AUTH_READ,100U+OU_AUTH_IDLE_MS,1U)==0);
    assert(ou_auth_check(&s,sessions[0].token,0,0U,
                         OU_AUTH_READ,100U+OU_AUTH_ABSOLUTE_MS,1U)==0);
    assert(login(&s,"admin",100U+OU_AUTH_ABSOLUTE_MS+1U,&sessions[0])==0);
    /* Monotonic 32-bit wrap is supported for short windows. */
    new_state(&s);
    assert(login(&s,"admin",UINT32_MAX-1000U,&sessions[0])==0);
    assert(ou_auth_check(&s,sessions[0].token,0,0U,
                         OU_AUTH_READ,500U,1U)==1);
    assert(ou_auth_check(&s,sessions[0].token,0,0U,
                         OU_AUTH_READ,UINT32_C(0x80000500),1U)==0);
}
static void config_gate_cannot_replay(void){
    ou_auth_state s;
    ou_auth_grant viewer,operator;
    ou_auth_config_gate gate={0};
    new_state(&s);
    assert(login(&s,"reader",100U,&viewer)==0);
    assert(login(&s,"operator",200U,&operator)==0);
    gate.state=&s;
    gate.token=viewer.token;
    gate.csrf=viewer.csrf;
    gate.request_id=1U;
    gate.now_ms=300U;
    gate.encrypted_transport=1U;
    assert(ou_auth_config_authorize(&gate)==0);
    gate.token=operator.token;gate.csrf=operator.csrf;
    assert(ou_auth_config_authorize(&gate)==1);
    assert(ou_auth_config_authorize(&gate)==0);
    gate.request_id=2U;
    assert(ou_auth_config_authorize(&gate)==1);
    gate.request_id=3U;gate.encrypted_transport=0U;
    assert(ou_auth_config_authorize(&gate)==0);
    gate.encrypted_transport=1U;
    assert(ou_auth_config_authorize(&gate)==1);
    assert(ou_auth_config_authorize(0)==0);
    ou_auth_revoke_all(&s);
    gate.request_id=4U;
    assert(ou_auth_config_authorize(&gate)==0);
}
int main(void){
    config_gate_cannot_replay();
    qualification();
    permissions_and_replay();
    lockout_and_entropy();
    expiry_and_capacity();
    puts("PASS: fail-closed TLS/KDF/RNG session core, lockout, roles, CSRF, replay, logout, revocation and rollover");
    return 0;
}
