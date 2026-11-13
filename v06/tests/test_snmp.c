#include "ou_snmp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t battery_oid[]={0x2b,6,1,2,1,33,1,2,1,0};
static const uint8_t output_oid[]={0x2b,6,1,2,1,33,1,4,4,1,2,1};
static unsigned checks;
static int auth(void *ctx,const uint8_t *community,size_t len){
    int enabled=*(int *)ctx;
    static const uint8_t expected[]="private-monitor";
    size_t i;
    unsigned difference=(unsigned)(len ^ (sizeof(expected)-1U));
    ++checks;
    for(i=0U;i<sizeof(expected)-1U;++i){
        uint8_t value=i<len?community[i]:0U;
        difference|=(unsigned)(value^expected[i]);
    }
    return enabled&&difference==0U;
}
static ou_snmp_access qualified(int *enabled){
    ou_snmp_access a={0};
    a.authorize=auth;
    a.context=enabled;
    a.source_acl_qualified=1U;
    a.community_store_qualified=1U;
    a.isolated_network_qualified=1U;
    return a;
}
static ou_snapshot sample(void){
    ou_snapshot s={0};
    s.origin=OU_ORIGIN_NATIVE;s.sample_ready=1U;
    s.battery_percent=83U;s.estimated_runtime_seconds=1234U;
    s.input_voltage_decivolts=1210U;
    s.output_voltage_decivolts=1190U;
    s.output_load_percent=48U;s.ac_present=1U;
    return s;
}
static size_t tlv(uint8_t *out,uint8_t tag,const uint8_t *bytes,size_t count){
    assert(count<128U);
    out[0]=tag;out[1]=(uint8_t)count;
    if(count)memcpy(out+2U,bytes,count);
    return count+2U;
}
static size_t request(uint8_t *out,uint8_t op,const uint8_t *oid,size_t oid_len,
                      const char *community){
    uint8_t vb[72],varbinds[96],pdu[128],message[192];
    uint8_t body[88],id[]={2,1,55},zero[]={2,1,0};
    size_t at=0U,vl,pl,ml=0U,comm_len=strlen(community);
    assert(comm_len<32U);
    at+=tlv(body+at,0x06U,oid,oid_len);
    at+=tlv(body+at,0x05U,0,0U);
    vl=tlv(vb,0x30U,body,at);
    vl=tlv(varbinds,0x30U,vb,vl);
    pl=0U;
    memcpy(pdu+pl,id,sizeof(id));pl+=sizeof(id);
    memcpy(pdu+pl,zero,sizeof(zero));pl+=sizeof(zero);
    memcpy(pdu+pl,zero,sizeof(zero));pl+=sizeof(zero);
    pl+=tlv(pdu+pl,0x30U,varbinds+2U,vl-2U);
    message[ml++]=2U;message[ml++]=1U;message[ml++]=1U;
    ml+=tlv(message+ml,0x04U,(const uint8_t *)community,comm_len);
    ml+=tlv(message+ml,op,pdu,pl);
    return tlv(out,0x30U,message,ml);
}
static size_t bulk_request(uint8_t *out,const uint8_t *oid,size_t oid_len,
                           uint8_t nonrepeaters,uint8_t maxrepetitions){
    size_t i,n=request(out,0xa1U,oid,oid_len,"private-monitor");
    /* A standard single-varbind GETNEXT fixture has exact two canonical
     * zero INTEGER fields following its 3-byte request ID. */
    for(i=0U;i+10U<n;++i){
        if(out[i]==0xa1U&&out[i+2U]==0x02U&&out[i+3U]==0x01U&&
           out[i+4U]==55U&&out[i+5U]==0x02U&&out[i+6U]==0x01U&&
           out[i+8U]==0x02U&&out[i+9U]==0x01U){
            out[i]=0xa5U;
            out[i+7U]=nonrepeaters;
            out[i+10U]=maxrepetitions;
            return n;
        }
    }
    assert(0&&"Cannot locate PDU fixture");
    return 0U;
}
static int has(const uint8_t *data,size_t n,const uint8_t *pattern,size_t len){
    size_t i;
    for(i=0U;i+len<=n;++i)
        if(memcmp(data+i,pattern,len)==0)return 1;
    return 0;
}
static void bulk_get_walk(void){
    uint8_t req[240],resp[OU_SNMP_MAX_PACKET];
    size_t n,written=0U;
    int enabled=1;
    ou_snmp_access a=qualified(&enabled);
    ou_snapshot native=sample();
    static const uint8_t before_first[]={0x2b,6,1,2,1,33,1,1,0};
    static const uint8_t after_last[]={0x2b,6,1,2,1,33,2};
    static const uint8_t end_of_mib[]={0x82,0x00};
    static const uint8_t no_instance[]={0x81,0x00};
    n=bulk_request(req,before_first,sizeof(before_first),0U,8U);
    assert(ou_snmp_respond(&a,&native,req,n,resp,sizeof(resp),&written)==0);
    assert(written<OU_SNMP_MAX_PACKET);
    assert(has(resp,written,battery_oid,sizeof(battery_oid)));
    assert(has(resp,written,output_oid,sizeof(output_oid)));
    assert(has(resp,written,end_of_mib,sizeof(end_of_mib)));
    /* MaxRepetitions=0 returns a legal empty varbind list, not an error. */
    n=bulk_request(req,before_first,sizeof(before_first),0U,0U);
    assert(ou_snmp_respond(&a,&native,req,n,resp,sizeof(resp),&written)==0);
    assert(written>0U&&!has(resp,written,battery_oid,sizeof(battery_oid)));
    /* NonRepeaters=1 returns one GETNEXT, even when MaxRepetitions>0. */
    n=bulk_request(req,before_first,sizeof(before_first),1U,8U);
    assert(ou_snmp_respond(&a,&native,req,n,resp,sizeof(resp),&written)==0);
    assert(has(resp,written,battery_oid,sizeof(battery_oid)));
    assert(!has(resp,written,output_oid,sizeof(output_oid)));
    n=bulk_request(req,before_first,sizeof(before_first),0U,100U);
    assert(ou_snmp_respond(&a,&native,req,n,resp,sizeof(resp),&written)==0);
    assert(written<=OU_SNMP_MAX_PACKET);
    n=bulk_request(req,after_last,sizeof(after_last),0U,4U);
    assert(ou_snmp_respond(&a,&native,req,n,resp,sizeof(resp),&written)==0);
    assert(has(resp,written,end_of_mib,sizeof(end_of_mib)));
    native.sample_ready=0U;
    n=bulk_request(req,before_first,sizeof(before_first),0U,8U);
    assert(ou_snmp_respond(&a,&native,req,n,resp,sizeof(resp),&written)==0);
    assert(has(resp,written,no_instance,sizeof(no_instance)));
    enabled=0;
    assert(ou_snmp_respond(&a,&native,req,n,resp,sizeof(resp),&written)==1);
    assert(written==0U);
    enabled=1;
    req[n-1U]=0xffU;
    assert(ou_snmp_respond(&a,&native,req,n,resp,sizeof(resp),&written)==-1);
    assert(written==0U);
}
static void decoded_responses(void){
    uint8_t req[240],resp[OU_SNMP_MAX_PACKET],bad[240];
    size_t n,written=0U;
    int enabled=1;
    ou_snmp_access a=qualified(&enabled);
    ou_snapshot s=sample();
    static const uint8_t expected_charge[]={0x02,0x01,0x53};
    static const uint8_t expected_battery[]={0x02,0x01,0x02};
    static const uint8_t expected_minutes[]={0x02,0x01,0x14};
    static const uint8_t expected_output[]={0x02,0x01,0x77};
    static const uint8_t no_instance[]={0x81,0x00};
    static const uint8_t no_object[]={0x80,0x00};
    static const uint8_t end_of_mib[]={0x82,0x00};
    static const uint8_t before_first[]={0x2b,6,1,2,1,33,1,1,0};
    static const uint8_t after_last[]={0x2b,6,1,2,1,33,2};
    n=request(req,0xa0U,battery_oid,sizeof(battery_oid),"private-monitor");
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==0);
    assert(written>n&&resp[0]==0x30U);
    assert(has(resp,written,expected_battery,sizeof(expected_battery)));
    assert(resp[0]!=0x02U);
    s.battery_percent=83U;
    n=request(req,0xa0U,(const uint8_t[]){0x2b,6,1,2,1,33,1,2,4,0},
              10U,"private-monitor");
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==0);
    assert(has(resp,written,expected_charge,sizeof(expected_charge)));
    n=request(req,0xa0U,(const uint8_t[]){0x2b,6,1,2,1,33,1,2,3,0},10U,
              "private-monitor");
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==0);
    assert(has(resp,written,expected_minutes,sizeof(expected_minutes)));
    n=request(req,0xa0U,output_oid,sizeof(output_oid),"private-monitor");
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==0);
    assert(has(resp,written,expected_output,sizeof(expected_output)));
    s.sample_ready=0U;
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==0);
    assert(has(resp,written,no_instance,sizeof(no_instance)));
    s=sample();s.origin=OU_ORIGIN_SIMULATOR;
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==0);
    assert(has(resp,written,no_instance,sizeof(no_instance)));
    s=sample();
    n=request(req,0xa0U,after_last,sizeof(after_last),"private-monitor");
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==0);
    assert(has(resp,written,no_object,sizeof(no_object)));
    n=request(req,0xa1U,before_first,sizeof(before_first),"private-monitor");
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==0);
    assert(has(resp,written,battery_oid,sizeof(battery_oid)));
    n=request(req,0xa1U,after_last,sizeof(after_last),"private-monitor");
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==0);
    assert(has(resp,written,end_of_mib,sizeof(end_of_mib)));
    n=request(req,0xa3U,battery_oid,sizeof(battery_oid),"private-monitor");
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==1);
    assert(written==0U);
    n=request(req,0xa0U,battery_oid,sizeof(battery_oid),"incorrect");
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==1);
    assert(written==0U);
    n=request(req,0xa0U,battery_oid,sizeof(battery_oid),"private-monitor");
    enabled=0;
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==1);
    assert(written==0U);
    enabled=1;a.source_acl_qualified=0U;
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==1);
    a.source_acl_qualified=1U;
    a.community_store_qualified=0U;
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==1);
    a.community_store_qualified=1U;
    a.isolated_network_qualified=0U;
    assert(ou_snmp_respond(&a,&s,req,n,resp,sizeof(resp),&written)==1);
    a.isolated_network_qualified=1U;
    memcpy(bad,req,n);
    bad[n-1U]=0xffU; /* NULL value must be zero-length */
    assert(ou_snmp_respond(&a,&s,bad,n,resp,sizeof(resp),&written)==-1);
    assert(written==0U);
    assert(ou_snmp_respond(&a,&s,req,n-1U,resp,sizeof(resp),&written)==-1);
    assert(ou_snmp_respond(&a,&s,req,n,resp,4U,&written)==-1);
    assert(written==0U);
    assert(checks>10U);
}
int main(void){
    decoded_responses();
    bulk_get_walk();
    puts("PASS: strict SNMPv2c read-only BER GET/GETNEXT/GETBULK, seven UPS-MIB OIDs, verified-only telemetry, ACL/community and SET refusal");
    return 0;
}
