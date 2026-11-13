#include "ou_config_model.h"

/* Freestanding: no malloc, printf, sscanf, networking or libc required. */
#define HEADER 12U
static const char *const keys[OU_CONFIG_FIELD_COUNT]={
    "System.Network.HostName",
    "System.Network.DomainName",
    "System.Network.DHCP",
    "System.Network.IPAddress",
    "System.Network.IPMask",
    "System.Network.IPGateway",
    "System.Contact",
    "System.Location",
    "System.Language"
};
static const char *const types[OU_CONFIG_FIELD_COUNT]={
    "String:49","String:49","Boolean","IPv4","IPv4","IPv4",
    "String:49","String:31","List:AUTO,FRE,ENG,SPA,GER,ITA"
};
static int eq(const char *a,const char *b){
    if(!a||!b)return 0;
    while(*a&&*b){if(*a!=*b)return 0;++a;++b;}
    return *a==*b;
}
static void zero(void *buf,size_t size){
    size_t i;
    uint8_t *p=buf;
    for(i=0U;i<size;++i)p[i]=0U;
}
static void copy(char *to,const char *from,size_t size){
    size_t i;
    for(i=0U;i<size;++i)to[i]=from[i];
}
static size_t bounded(const char *value,size_t max) {
    size_t len=0U;
    if(!value)return max;
    while(len<max && value[len])++len;
    return len;
}
static int text_ok(const char *s,size_t max) {
    size_t i,n=bounded(s,OU_CONFIG_VALUE_BYTES);
    if(n==0U||n>max)return 0;
    for(i=0U;i<n;++i)
        if((unsigned char)s[i]<32U||(unsigned char)s[i]>126U)
            return 0;
    return 1;
}
static int hostname(const char *s,int dots){
    size_t i,n=bounded(s,OU_CONFIG_VALUE_BYTES);
    int last_separator=1;
    if(n==0U||n>49U)return 0;
    for(i=0U;i<n;++i){
        char ch=s[i];
        int alnum=(ch>='A'&&ch<='Z')||
                  (ch>='a'&&ch<='z')||
                  (ch>='0'&&ch<='9');
        if(alnum){last_separator=0;continue;}
        if(ch=='-' && !last_separator && i+1U<n &&
           s[i+1U]!='.' && s[i+1U]!='-')continue;
        if(dots && ch=='.' && !last_separator && i+1U<n &&
           s[i+1U]!='.' && s[i+1U]!='-'){
            last_separator=1;
            continue;
        }
        return 0;
    }
    return !last_separator && s[n-1U]!='-';
}
static int parse_ipv4(const char *s,uint32_t *out){
    uint32_t address=0U,value=0U;
    size_t i=0U,chars=0U;
    unsigned octet;
    if(!s||!out)return 0;
    for(octet=0U;octet<4U;++octet){
        value=0U;chars=0U;
        while(i<OU_CONFIG_VALUE_BYTES && s[i]>='0'&&s[i]<='9'){
            if(chars==0U && s[i]=='0' && s[i+1U]>='0' && s[i+1U]<='9')
                return 0;
            value=value*10U+(uint32_t)(s[i++]-'0');
            if(++chars>3U||value>255U)return 0;
        }
        if(chars==0U)return 0;
        address=(address<<8U)|value;
        if(octet<3U){
            if(i>=OU_CONFIG_VALUE_BYTES || s[i++]!='.')return 0;
        }else if(i>=OU_CONFIG_VALUE_BYTES||s[i]!='\0')return 0;
    }
    *out=address;
    return 1;
}
static int unicast_ipv4(uint32_t ip){
    uint32_t first=ip>>24U;
    return first>0U&&first<224U&&first!=127U;
}
static int netmask_valid(uint32_t mask){
    uint32_t inverse=~mask;
    return mask && (inverse&(inverse+1U))==0U;
}
static int field_valid(unsigned key,const char *value){
    uint32_t ipv4;
    if(!value||bounded(value,OU_CONFIG_VALUE_BYTES)>=OU_CONFIG_VALUE_BYTES)
        return 0;
    switch(key){
      case OU_CONFIG_HOSTNAME:return hostname(value,0);
      case OU_CONFIG_DOMAIN:return hostname(value,1);
      case OU_CONFIG_DHCP:return eq(value,"true")||eq(value,"false");
      case OU_CONFIG_IPV4:
      case OU_CONFIG_GATEWAY:
        return parse_ipv4(value,&ipv4) && unicast_ipv4(ipv4);
      case OU_CONFIG_NETMASK:
        return parse_ipv4(value,&ipv4) && netmask_valid(ipv4);
      case OU_CONFIG_CONTACT:return text_ok(value,49U);
      case OU_CONFIG_LOCATION:return text_ok(value,31U);
      case OU_CONFIG_LANGUAGE:
        return eq(value,"AUTO")||eq(value,"FRE")||eq(value,"ENG")||
               eq(value,"SPA")||eq(value,"GER")||eq(value,"ITA");
      default:return 0;
    }
}
static int catalog_matches(void){
    size_t i;
    unsigned key;
    for(key=0U;key<OU_CONFIG_FIELD_COUNT;++key) {
        int found=0;
        for(i=0U;i<ou_schema_field_count;++i)
            if(eq(ou_schema_fields[i].name,keys[key]) &&
               eq(ou_schema_fields[i].type_expression,types[key]) &&
               !ou_schema_fields[i].hidden_in_oem){
                found=1;
                break;
            }
        if(!found)return 0;
    }
    return 1;
}
const char *ou_config_key_name(unsigned key){
    return key<OU_CONFIG_FIELD_COUNT?keys[key]:(const char *)0;
}
const char *ou_config_key_type(unsigned key){
    return key<OU_CONFIG_FIELD_COUNT?types[key]:(const char *)0;
}
int ou_config_key_index(const char *name){
    unsigned i;
    if(!name)return -1;
    for(i=0U;i<OU_CONFIG_FIELD_COUNT;++i)
        if(eq(keys[i],name))return (int)i;
    return -1;
}
void ou_config_model_init(ou_config_model *model){
    if(model)zero(model,sizeof(*model));
}
int ou_config_model_set(ou_config_model *model,const char *name,
                        const char *value){
    int key=ou_config_key_index(name);
    size_t n;
    if(!model||key<0||!catalog_matches()||
       !field_valid((unsigned)key,value))return -1;
    n=bounded(value,OU_CONFIG_VALUE_BYTES);
    zero(model->values[key],OU_CONFIG_VALUE_BYTES);
    copy(model->values[key],value,n);
    model->present[key]=1U;
    return 0;
}
int ou_config_model_clear(ou_config_model *model,const char *name){
    int key=ou_config_key_index(name);
    if(!model||key<0)return -1;
    model->present[key]=0U;
    zero(model->values[key],OU_CONFIG_VALUE_BYTES);
    return 0;
}
int ou_config_model_validate(const ou_config_model *model){
    unsigned i;
    uint32_t ip=0U,mask=0U,gw=0U;
    int has_ip,has_mask,has_gw,has_dhcp;
    if(!model||!catalog_matches())return -1;
    for(i=0U;i<OU_CONFIG_FIELD_COUNT;++i){
        size_t j;
        if(model->present[i]>1U)return -1;
        if(model->present[i]){
            if(!field_valid(i,model->values[i]))return -1;
        }else {
            for(j=0U;j<OU_CONFIG_VALUE_BYTES;++j)
                if(model->values[i][j])return -1;
        }
    }
    has_ip=model->present[OU_CONFIG_IPV4];
    has_mask=model->present[OU_CONFIG_NETMASK];
    has_gw=model->present[OU_CONFIG_GATEWAY];
    has_dhcp=model->present[OU_CONFIG_DHCP];
    /* Never commit an incomplete or potentially inaccessible management
     * network profile. DHCP-only and identity-only records are valid.
     * A gateway requires a complete static IP+mask, even with DHCP on. */
    if((has_ip||has_mask||has_gw) && (!has_ip||!has_mask||!has_dhcp))
        return -1;
    if(has_dhcp&&eq(model->values[OU_CONFIG_DHCP],"false") &&
       (!has_ip||!has_mask))return -1;
    if(has_ip) {
        if(!parse_ipv4(model->values[OU_CONFIG_IPV4],&ip)||
           !parse_ipv4(model->values[OU_CONFIG_NETMASK],&mask))
            return -1;
        if(mask!=UINT32_MAX) {
            if((ip&~mask)==0U||(ip&~mask)==(~mask))return -1;
        }
        if(has_gw) {
            if(!parse_ipv4(model->values[OU_CONFIG_GATEWAY],&gw) ||
               (ip&mask)!=(gw&mask)||gw==ip)return -1;
            if(mask!=UINT32_MAX &&
               ((gw&~mask)==0U||(gw&~mask)==(~mask)))return -1;
        }
    }
    return 0;
}
static void put16(uint8_t *p,uint16_t n) {
    p[0]=(uint8_t)(n>>8U);p[1]=(uint8_t)n;
}
static uint16_t get16(const uint8_t *p){
    return (uint16_t)(((uint16_t)p[0]<<8U)|p[1]);
}
int ou_config_model_encode(const ou_config_model *model,uint8_t *out,
                           size_t cap,size_t *written){
    unsigned key;size_t at=HEADER,count=0U,i,n;
    if(written)*written=0U;
    if(!out||!written||cap<HEADER||cap>OU_JOURNAL_MAX_PAYLOAD||
       ou_config_model_validate(model)!=0)return -1;
    for(i=0U;i<cap;++i)out[i]=0U;
    for(key=0U;key<OU_CONFIG_FIELD_COUNT;++key)
        if(model->present[key]){
            n=bounded(model->values[key],OU_CONFIG_VALUE_BYTES);
            if(n==0U||n>255U||at+2U+n>cap||
               at+2U+n>OU_CONFIG_RECORD_MAX)return -1;
            out[at++]=(uint8_t)key;
            out[at++]=(uint8_t)n;
            for(i=0U;i<n;++i)out[at++]=(uint8_t)model->values[key][i];
            ++count;
        }
    if(count==0U)return -1;
    out[0]='O';out[1]='6';out[2]='C';out[3]='F';
    out[4]=OU_CONFIG_RECORD_VERSION;
    out[5]=(uint8_t)count;
    put16(out+6U,(uint16_t)at);
    *written=at;
    return 0;
}
int ou_config_model_decode(ou_config_model *model,const uint8_t *input,
                           size_t length){
    ou_config_model candidate;
    size_t at=HEADER,i,n;
    unsigned last=0U,key,count;
    if(!model||!input||length<HEADER||length>OU_CONFIG_RECORD_MAX||
       input[0]!='O'||input[1]!='6'||input[2]!='C'||input[3]!='F'||
       input[4]!=OU_CONFIG_RECORD_VERSION ||
       input[5]==0U||input[5]>OU_CONFIG_FIELD_COUNT ||
       get16(input+6U)!=length ||
       input[8]!=0U||input[9]!=0U||input[10]!=0U||input[11]!=0U)
        return -1;
    ou_config_model_init(&candidate);
    count=input[5];
    for(i=0U;i<count;++i){
        if(at+2U>length)return -1;
        key=input[at++];n=input[at++];
        if(key>=OU_CONFIG_FIELD_COUNT || n==0U ||
           n>=OU_CONFIG_VALUE_BYTES || n>length-at ||
           (i!=0U&&key<=last))return -1;
        last=key;
        for(size_t j=0U;j<n;++j){
            if(input[at+j]==0U)return -1;
            candidate.values[key][j]=(char)input[at+j];
        }
        candidate.present[key]=1U;
        at+=n;
    }
    if(at!=length||ou_config_model_validate(&candidate)!=0)return -1;
    *model=candidate;
    return 0;
}
int ou_config_model_save(const ou_config_model *model,
                         const ou_journal_io *io,
                         ou_config_authorize_fn authorize,void *auth_context,
                         ou_journal_record *meta){
    uint8_t encoded[OU_CONFIG_RECORD_MAX];
    size_t length=0U;
    if(meta)*meta=(ou_journal_record){0};
    if(!meta||!io||!authorize||authorize(auth_context)!=1 ||
       ou_config_model_encode(model,encoded,sizeof(encoded),&length)!=0)
        return -1;
    return ou_journal_commit(io,encoded,length,meta);
}
int ou_config_model_load(ou_config_model *model,
                         const ou_journal_io *io,
                         ou_journal_record *meta){
    uint8_t encoded[OU_CONFIG_RECORD_MAX];
    ou_journal_record selected;
    ou_config_model candidate;
    size_t length=0U;
    if(meta)*meta=(ou_journal_record){0};
    if(!model||!io||!meta)return -1;
    if(ou_journal_load(io,encoded,sizeof(encoded),&length,&selected)!=0 ||
       ou_config_model_decode(&candidate,encoded,length)!=0)return -1;
    *model=candidate;
    *meta=selected;
    return 0;
}
