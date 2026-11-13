#include "ou_auth.h"

/* Explicit volatile wipe: no libc, zeroing cannot be elided. */
static void wipe(void *data,size_t size){
    volatile uint8_t *p=(volatile uint8_t *)data;
    while(size--)*p++=0U;
}
static int ct_equal(const uint8_t *a,const uint8_t *b,size_t length){
    uint32_t diff=0U;
    size_t i;
    if(!a||!b)return 0;
    for(i=0U;i<length;++i)diff|=(uint32_t)(a[i]^b[i]);
    return diff==0U;
}
static int ct_nonzero(const uint8_t *bytes,size_t length){
    uint32_t bits=0U;size_t i;
    for(i=0U;i<length;++i)bits|=bytes[i];
    return bits!=0U;
}
static int username_valid(const char *name){
    size_t i;
    if(!name)return 0;
    for(i=0U;i<OU_AUTH_MAX_USERNAME+1U;++i){
        unsigned char c=(unsigned char)name[i];
        if(!c)return i>0U;
        if(i>=OU_AUTH_MAX_USERNAME)return 0;
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
             (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))
            return 0;
    }
    return 0;
}
static int provider_valid(const ou_auth_state *s){
    return s&&s->initialized==1U&&
           s->provider.csprng&&s->provider.verify_password_kdf &&
           s->provider.entropy_qualified==1U &&
           s->provider.credential_store_qualified==1U &&
           s->provider.tls_qualified==1U;
}
static int span(uint32_t now,uint32_t then,uint32_t *age){
    *age=now-then;
    return *age<UINT32_C(0x80000000);
}
static int active(ou_auth_session *session,uint32_t now_ms){
    uint32_t idle,absolute;
    if(!session->occupied)return 0;
    if(!span(now_ms,session->issued_ms,&absolute) ||
       !span(now_ms,session->last_seen_ms,&idle) ||
       absolute>=OU_AUTH_ABSOLUTE_MS || idle>=OU_AUTH_IDLE_MS){
        wipe(session,sizeof(*session));return 0;
    }
    return 1;
}
static int locked(ou_auth_state *s,uint32_t now_ms){
    uint32_t duration;
    if(!s->lockout_active)return 0;
    if(!span(now_ms,s->failure_start_ms,&duration))return 1;
    if(duration<OU_AUTH_LOCKOUT_MS)return 1;
    s->lockout_active=0U;
    s->failures=0U;
    return 0;
}
static void fail(ou_auth_state *s,uint32_t now_ms){
    if(s->failures<OU_AUTH_MAX_FAILURES)++s->failures;
    if(s->failures>=OU_AUTH_MAX_FAILURES){
        s->lockout_active=1U;
        s->failure_start_ms=now_ms;
    }
}
static int role_allows(uint8_t role,enum ou_auth_permission permission){
    if(role<OU_AUTH_VIEWER||role>OU_AUTH_ADMIN)return 0;
    switch(permission){
      case OU_AUTH_READ:return 1;
      case OU_AUTH_CONFIG_WRITE:return role>=OU_AUTH_OPERATOR;
      case OU_AUTH_POWER_CONTROL:
      case OU_AUTH_FIRMWARE_UPDATE:
      case OU_AUTH_USER_ADMIN:return role==OU_AUTH_ADMIN;
      default:return 0;
    }
}
static int session_index(ou_auth_state *s,
                         const uint8_t token[OU_AUTH_TOKEN_BYTES],
                         uint32_t now_ms){
    size_t i;
    int idx=-1;
    if(!token)return -1;
    for(i=0U;i<OU_AUTH_SESSIONS;++i){
        ou_auth_session *session=&s->sessions[i];
        if(active(session,now_ms) &&
           ct_equal(session->token,token,OU_AUTH_TOKEN_BYTES))
            idx=(int)i;
    }
    return idx;
}
static int check(ou_auth_state *s,
                 const uint8_t token[OU_AUTH_TOKEN_BYTES],
                 const uint8_t csrf[OU_AUTH_TOKEN_BYTES],
                 uint32_t request_id,enum ou_auth_permission permission,
                 uint32_t now_ms,uint8_t encrypted_transport,
                 int mutation){
    int idx;
    ou_auth_session *session;
    if(!provider_valid(s)||encrypted_transport!=1U)return 0;
    idx=session_index(s,token,now_ms);
    if(idx<0)return 0;
    session=&s->sessions[idx];
    if(!role_allows(session->role,permission))return 0;
    if(mutation){
        if(!csrf||!ct_equal(session->csrf,csrf,OU_AUTH_TOKEN_BYTES)||
           request_id==0U||request_id<=session->request_counter)return 0;
        session->request_counter=request_id;
    }
    session->last_seen_ms=now_ms;
    return 1;
}
void ou_auth_init(ou_auth_state *state,const ou_auth_provider *provider){
    if(!state)return;
    wipe(state,sizeof(*state));
    if(!provider||!provider->csprng||!provider->verify_password_kdf ||
       provider->entropy_qualified!=1U||
       provider->credential_store_qualified!=1U||
       provider->tls_qualified!=1U)return;
    state->provider=*provider;
    state->initialized=1U;
}
void ou_auth_revoke_all(ou_auth_state *state){
    size_t i;
    if(!state)return;
    for(i=0U;i<OU_AUTH_SESSIONS;++i)
        wipe(&state->sessions[i],sizeof(state->sessions[i]));
}
int ou_auth_login(ou_auth_state *state,const char *username,
                  const uint8_t *password,size_t password_len,
                  uint32_t now_ms,uint8_t encrypted_transport,
                  ou_auth_grant *grant){
    size_t i;
    int available=-1;
    uint8_t role=0U;
    uint8_t token[OU_AUTH_TOKEN_BYTES],csrf[OU_AUTH_TOKEN_BYTES];
    int valid;
    if(grant)wipe(grant,sizeof(*grant));
    if(!provider_valid(state)||!grant||encrypted_transport!=1U)return -1;
    if(locked(state,now_ms))return -1;
    if(!username_valid(username)||!password||
       password_len<8U||password_len>OU_AUTH_MAX_PASSWORD){
        fail(state,now_ms);
        return -1;
    }
    for(i=0U;i<OU_AUTH_SESSIONS;++i){
        if(!active(&state->sessions[i],now_ms) && available<0)
            available=(int)i;
    }
    if(available<0)return -1;
    valid=state->provider.verify_password_kdf(
        state->provider.context,username,password,password_len,&role);
    if(valid!=1||role<OU_AUTH_VIEWER||role>OU_AUTH_ADMIN){
        fail(state,now_ms);
        return -1;
    }
    wipe(token,sizeof(token));wipe(csrf,sizeof(csrf));
    if(state->provider.csprng(state->provider.context,token,sizeof(token))!=1||
       state->provider.csprng(state->provider.context,csrf,sizeof(csrf))!=1||
       !ct_nonzero(token,sizeof(token))||
       !ct_nonzero(csrf,sizeof(csrf))||
       ct_equal(token,csrf,sizeof(token))){
        wipe(token,sizeof(token));wipe(csrf,sizeof(csrf));
        return -1;
    }
    for(i=0U;i<OU_AUTH_SESSIONS;++i)
        if(state->sessions[i].occupied &&
           (ct_equal(state->sessions[i].token,token,sizeof(token))||
            ct_equal(state->sessions[i].csrf,csrf,sizeof(csrf)))){
            wipe(token,sizeof(token));wipe(csrf,sizeof(csrf));
            return -1;
        }
    {
        ou_auth_session *session=&state->sessions[available];
        wipe(session,sizeof(*session));
        for(i=0U;i<OU_AUTH_TOKEN_BYTES;++i){
            session->token[i]=token[i];
            session->csrf[i]=csrf[i];
            grant->token[i]=token[i];
            grant->csrf[i]=csrf[i];
        }
        session->issued_ms=now_ms;
        session->last_seen_ms=now_ms;
        session->role=role;
        session->occupied=1U;
        grant->role=role;
    }
    state->failures=0U;
    state->lockout_active=0U;
    wipe(token,sizeof(token));wipe(csrf,sizeof(csrf));
    return 0;
}
int ou_auth_check(ou_auth_state *state,
                  const uint8_t token[OU_AUTH_TOKEN_BYTES],
                  const uint8_t csrf[OU_AUTH_TOKEN_BYTES],
                  uint32_t request_id,
                  enum ou_auth_permission permission,
                  uint32_t now_ms,uint8_t encrypted_transport){
    return check(state,token,csrf,request_id,permission,now_ms,
                 encrypted_transport,permission!=OU_AUTH_READ);
}
int ou_auth_logout(ou_auth_state *state,
                   const uint8_t token[OU_AUTH_TOKEN_BYTES],
                   const uint8_t csrf[OU_AUTH_TOKEN_BYTES],
                   uint32_t request_id,uint32_t now_ms,
                   uint8_t encrypted_transport){
    int idx;
    if(!check(state,token,csrf,request_id,OU_AUTH_READ,now_ms,
              encrypted_transport,1))return -1;
    idx=session_index(state,token,now_ms);
    if(idx<0)return -1;
    wipe(&state->sessions[idx],sizeof(state->sessions[idx]));
    return 0;
}
int ou_auth_revoke_sessions(ou_auth_state *state,
                   const uint8_t admin_token[OU_AUTH_TOKEN_BYTES],
                   const uint8_t admin_csrf[OU_AUTH_TOKEN_BYTES],
                   uint32_t request_id,uint32_t now_ms,
                   uint8_t encrypted_transport){
    if(!check(state,admin_token,admin_csrf,request_id,OU_AUTH_USER_ADMIN,
              now_ms,encrypted_transport,1))return -1;
    ou_auth_revoke_all(state);
    return 0;
}

/* Same signature as the transactional journal/activation authorization
 * callback. Requires a live OPERATOR+ session, correct CSRF and non-replayed
 * request counter. Never auto-increments a client-controlled counter. */
int ou_auth_config_authorize(void *context){
    const ou_auth_config_gate *gate=(const ou_auth_config_gate *)context;
    if(!gate || !gate->state)return 0;
    return ou_auth_check(gate->state,gate->token,gate->csrf,
                 gate->request_id,OU_AUTH_CONFIG_WRITE,gate->now_ms,
                 gate->encrypted_transport);
}
