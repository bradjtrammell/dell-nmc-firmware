#include "ou_snmp.h"

typedef struct {const uint8_t *p;size_t n,at;} reader;
typedef struct {uint8_t *p;size_t n,at;} writer;
static int field(reader *r,uint8_t *tag,reader *value) {
    size_t len;uint8_t first;
    if(!r||!tag||!value||r->at+2U>r->n)return -1;
    *tag=r->p[r->at++];
    first=r->p[r->at++];
    if(first<128U)len=first;
    else if(first==0x81U) {
        if(r->at>=r->n)return -1;
        len=r->p[r->at++];
        if(len<128U)return -1;
    } else if(first==0x82U){
        if(r->at+2U>r->n)return -1;
        len=((size_t)r->p[r->at]<<8U)|r->p[r->at+1U];r->at+=2U;
        if(len<=255U)return -1;
    } else return -1;
    if(len>r->n-r->at)return -1;
    value->p=r->p+r->at;value->n=len;value->at=0U;
    r->at+=len;
    return 0;
}
static int next(reader *r,uint8_t expected,reader *v){
    uint8_t tag;
    if(field(r,&tag,v)!=0||tag!=expected)return -1;
    return 0;
}
static int put(writer *w,uint8_t ch){
    if(!w||w->at>=w->n)return -1;
    w->p[w->at++]=ch;return 0;
}
static int data(writer *w,const uint8_t *p,size_t len){
    size_t i;
    if(!w||(!p&&len)||len>w->n-w->at)return -1;
    for(i=0U;i<len;++i)w->p[w->at++]=p[i];
    return 0;
}
static int tlv(writer *w,uint8_t tag,const uint8_t *p,size_t len){
    if(!w||len>65535U || put(w,tag)!=0)return -1;
    if(len<128U){
        if(put(w,(uint8_t)len)!=0)return -1;
    }else if(len<=255U){
        if(put(w,0x81U)!=0||put(w,(uint8_t)len)!=0)return -1;
    }else {
        if(put(w,0x82U)!=0||put(w,(uint8_t)(len>>8U))!=0||
           put(w,(uint8_t)len)!=0)return -1;
    }
    return data(w,p,len);
}
static int int32(reader *r,int allow_negative,int32_t *v){
    size_t i;uint32_t val;
    if(!r||!v||r->n==0U||r->n>4U)return -1;
    if(r->n>1U &&
       ((r->p[0]==0U&&(r->p[1]&0x80U)==0U) ||
        (r->p[0]==255U&&(r->p[1]&0x80U)!=0U)))return -1;
    if(!allow_negative&&(r->p[0]&0x80U))return -1;
    val=(r->p[0]&0x80U)?UINT32_MAX:0U;
    for(i=0U;i<r->n;++i)val=(val<<8U)|r->p[i];
    *v=(int32_t)val;
    return 0;
}
static int put_integer(writer *w,uint32_t v){
    uint8_t bytes[5];size_t first=0U;
    bytes[0]=0U;
    bytes[1]=(uint8_t)(v>>24U);
    bytes[2]=(uint8_t)(v>>16U);
    bytes[3]=(uint8_t)(v>>8U);
    bytes[4]=(uint8_t)v;
    while(first<4U && bytes[first]==0U && (bytes[first+1U]&0x80U)==0U)
        ++first;
    return tlv(w,0x02U,bytes+first,5U-first);
}
static int compare(const uint8_t *a,size_t a_len,const uint8_t *b,size_t b_len){
    size_t i,limit=a_len<b_len?a_len:b_len;
    for(i=0U;i<limit;++i)if(a[i]!=b[i])return a[i]<b[i]?-1:1;
    if(a_len==b_len)return 0;
    return a_len<b_len?-1:1;
}
/* DER-compatible, canonical subidentifier sequences for RFC1628 UPS-MIB.
 * All OIDs are supported only by independently validated UPS telemetry. */
static const uint8_t oid_battery[]={0x2b,6,1,2,1,33,1,2,1,0};
static const uint8_t oid_minutes[]={0x2b,6,1,2,1,33,1,2,3,0};
static const uint8_t oid_charge[]={0x2b,6,1,2,1,33,1,2,4,0};
static const uint8_t oid_input[]={0x2b,6,1,2,1,33,1,3,3,1,3,1};
static const uint8_t oid_source[]={0x2b,6,1,2,1,33,1,4,1,0};
static const uint8_t oid_output[]={0x2b,6,1,2,1,33,1,4,4,1,2,1};
static const uint8_t oid_load[]={0x2b,6,1,2,1,33,1,4,4,1,5,1};
typedef struct {const uint8_t *bytes;size_t length;uint8_t metric;} mib_entry;
static const mib_entry mib[OU_SNMP_OID_COUNT]={
    {oid_battery,sizeof(oid_battery),0U},
    {oid_minutes,sizeof(oid_minutes),1U},
    {oid_charge,sizeof(oid_charge),2U},
    {oid_input,sizeof(oid_input),3U},
    {oid_source,sizeof(oid_source),4U},
    {oid_output,sizeof(oid_output),5U},
    {oid_load,sizeof(oid_load),6U}
};
static uint32_t metric_value(uint8_t metric,const ou_snapshot *s){
    switch(metric){
        case 0U:return s->battery_percent<=20U?3U:2U;
        case 1U:return s->estimated_runtime_seconds/60U;
        case 2U:return s->battery_percent;
        case 3U:return s->input_voltage_decivolts/10U;
        case 4U:return s->ac_present?3U:5U; /* normal / battery */
        case 5U:return s->output_voltage_decivolts/10U;
        default:return s->output_load_percent;
    }
}
static int oid_valid(const uint8_t *bytes,size_t len){
    size_t i=0U;
    if(!bytes||len==0U||len>32U)return 0;
    while(i<len){
        size_t start=i;
        while(i<len && (bytes[i]&0x80U)) {
            /* Bounded BER base-128 encoding; reject overlong zero. */
            if(i==start&&bytes[i]==0x80U)return 0;
            ++i;
            if(i-start>4U)return 0;
        }
        if(i>=len||i-start>4U)return 0;
        ++i;
    }
    return 1;
}
/* RFC 3416 read-only GET or lexicographic successor. For GETBULK the
 * caller retains the successor OID as the next repetition cursor. */
static int append_bind(writer *out,const ou_snapshot *telemetry,
                       uint8_t oid_bytes[32],uint8_t *oid_length,
                       int successor){
    uint8_t one[64];
    writer cell=(writer){one,sizeof(one),0U};
    const mib_entry *match=0;
    uint8_t exception=successor?0x82U:0x80U;
    size_t j,i;
    for(j=0U;j<OU_SNMP_OID_COUNT;++j){
        int cmp=compare(oid_bytes,*oid_length,mib[j].bytes,mib[j].length);
        if((successor&&cmp<0)||(!successor&&cmp==0)){
            match=&mib[j];break;
        }
    }
    if(match){
        if(tlv(&cell,0x06U,match->bytes,match->length)!=0)return -1;
        if(telemetry->origin==OU_ORIGIN_NATIVE&&telemetry->sample_ready==1U){
            if(put_integer(&cell,metric_value(match->metric,telemetry))!=0)
                return -1;
        }else if(tlv(&cell,0x81U,0,0U)!=0)return -1;
        if(successor){
            for(i=0U;i<match->length;++i)oid_bytes[i]=match->bytes[i];
            *oid_length=(uint8_t)match->length;
        }
    }else{
        if(tlv(&cell,0x06U,oid_bytes,*oid_length)!=0 ||
           tlv(&cell,exception,0,0U)!=0)return -1;
    }
    return tlv(out,0x30U,one,cell.at);
}
int ou_snmp_respond(const ou_snmp_access *access,
                    const ou_snapshot *telemetry,
                    const uint8_t *request,size_t request_len,
                    uint8_t *response,size_t response_capacity,
                    size_t *written){
    reader packet,outer,version,community,pdu,field_view,list,bind,oid,nullvalue;
    uint8_t pdu_type,request_id[4],ids[OU_SNMP_MAX_VARBINDS][32];
    uint8_t lengths[OU_SNMP_MAX_VARBINDS];
    uint8_t bindings[512],pdu_body[640],outer_body[OU_SNMP_MAX_PACKET];
    writer bind_writer,pdu_writer,outer_writer,result;
    int32_t value;
    size_t count=0U,i,id_len;
    int32_t field_status,non_repeaters=0,max_repetitions=0;
    size_t output_count=0U,non_repeat_count,rep;
    if(written)*written=0U;
    if(!written||!access||!telemetry||!request||!response||
       request_len>OU_SNMP_MAX_PACKET||
       response_capacity>UINT32_MAX||request_len<16U)return -1;
    if(!access->authorize||access->source_acl_qualified!=1U||
       access->community_store_qualified!=1U||
       access->isolated_network_qualified!=1U)return 1;
    packet=(reader){request,request_len,0U};
    if(next(&packet,0x30U,&outer)!=0||packet.at!=packet.n ||
       next(&outer,0x02U,&version)!=0||
       int32(&version,0,&value)!=0||value!=1 ||
       next(&outer,0x04U,&community)!=0 ||
       community.n==0U||community.n>OU_SNMP_MAX_COMMUNITY)return -1;
    /* The callback must check verified source ACL and compare the provisioned
     * secret in constant time. Wrong communities receive no response. */
    if(access->authorize(access->context,community.p,community.n)!=1)return 1;
    if(field(&outer,&pdu_type,&pdu)!=0||outer.at!=outer.n)return -1;
    if(pdu_type==0xa3U)return 1; /* SET is *never* enabled */
    if(pdu_type!=0xa0U&&pdu_type!=0xa1U&&pdu_type!=0xa5U)return -1;
    if(next(&pdu,0x02U,&field_view)!=0 ||
       field_view.n==0U||field_view.n>4U)return -1;
    id_len=field_view.n;
    for(i=0U;i<id_len;++i)request_id[i]=field_view.p[i];
    if(next(&pdu,0x02U,&field_view)!=0||
       int32(&field_view,0,&field_status)!=0)return -1;
    if(pdu_type==0xa5U)non_repeaters=field_status;
    else if(field_status!=0)return -1;
    if(next(&pdu,0x02U,&field_view)!=0||
       int32(&field_view,0,&field_status)!=0)return -1;
    if(pdu_type==0xa5U)max_repetitions=field_status;
    else if(field_status!=0)return -1;
    if(next(&pdu,0x30U,&list)!=0||pdu.at!=pdu.n)return -1;
    while(list.at<list.n){
        if(count>=OU_SNMP_MAX_VARBINDS ||
           next(&list,0x30U,&bind)!=0 ||
           next(&bind,0x06U,&oid)!=0 ||
           !oid_valid(oid.p,oid.n) ||
           next(&bind,0x05U,&nullvalue)!=0 ||
           nullvalue.n!=0U||bind.at!=bind.n)return -1;
        lengths[count]=(uint8_t)oid.n;
        for(i=0U;i<oid.n;++i)ids[count][i]=oid.p[i];
        ++count;
    }
    if(count==0U)return -1;
    bind_writer=(writer){bindings,sizeof(bindings),0U};
    if(pdu_type==0xa5U){
        non_repeat_count=(size_t)non_repeaters;
        if(non_repeat_count>count)non_repeat_count=count;
        for(i=0U;i<non_repeat_count;++i){
            if(append_bind(&bind_writer,telemetry,ids[i],&lengths[i],1)!=0)
                return -1;
            ++output_count;
        }
        /* Bound expansion to 8 response entries, even for a huge
         * max-repetitions value or a tiny requester buffer. This both
         * protects the ARM7 stack and prevents amplification. */
        for(rep=0U;rep<(size_t)max_repetitions &&
                    output_count<OU_SNMP_MAX_VARBINDS;++rep){
            for(i=non_repeat_count;i<count &&
                 output_count<OU_SNMP_MAX_VARBINDS;++i){
                if(append_bind(&bind_writer,telemetry,ids[i],&lengths[i],1)!=0)
                    return -1;
                ++output_count;
            }
        }
    }else{
        for(i=0U;i<count;++i)
            if(append_bind(&bind_writer,telemetry,ids[i],&lengths[i],
                           pdu_type==0xa1U)!=0)return -1;
    }
    pdu_writer=(writer){pdu_body,sizeof(pdu_body),0U};
    if(tlv(&pdu_writer,0x02U,request_id,id_len)!=0 ||
       put_integer(&pdu_writer,0U)!=0 || put_integer(&pdu_writer,0U)!=0 ||
       tlv(&pdu_writer,0x30U,bindings,bind_writer.at)!=0)return -1;
    outer_writer=(writer){outer_body,sizeof(outer_body),0U};
    if(put_integer(&outer_writer,1U)!=0 ||
       tlv(&outer_writer,0x04U,community.p,community.n)!=0 ||
       tlv(&outer_writer,0xa2U,pdu_body,pdu_writer.at)!=0)return -1;
    result=(writer){response,response_capacity,0U};
    if(tlv(&result,0x30U,outer_body,outer_writer.at)!=0)return -1;
    *written=result.at;
    return 0;
}
