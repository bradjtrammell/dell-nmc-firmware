#include "ou_config_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SLOT_BYTES 8192U
static struct {
    uint8_t slots[2][SLOT_BYTES];
    unsigned erases,writes;
    int fail_after;
} nor;
static void blank(void){
    memset(&nor,0,sizeof(nor));
    memset(nor.slots,0xff,sizeof(nor.slots));
    nor.fail_after=-1;
}
static int nr(void *ctx,unsigned slot,uint32_t pos,uint8_t *output,size_t n){
    (void)ctx;
    if(slot>1U||pos>SLOT_BYTES||n>SLOT_BYTES-pos)return -1;
    memcpy(output,nor.slots[slot]+pos,n);return 0;
}
static int np(void *ctx,unsigned slot,uint32_t pos,const uint8_t *input,size_t n){
    size_t i;(void)ctx;
    if(slot>1U||pos>SLOT_BYTES||n>SLOT_BYTES-pos)return -1;
    for(i=0U;i<n;++i){
        if(nor.fail_after==0)return -1;
        if(((uint8_t)~nor.slots[slot][pos+i]&input[i])!=0U)return -1;
        nor.slots[slot][pos+i]&=input[i];
        ++nor.writes;
        if(nor.fail_after>0)--nor.fail_after;
    }
    return 0;
}
static int ne(void *ctx,unsigned slot){
    (void)ctx;
    if(slot>1U)return -1;
    memset(nor.slots[slot],0xff,SLOT_BYTES);
    ++nor.erases;return 0;
}
static ou_journal_io backend(void){
    ou_journal_io io={0};
    io.read=nr;io.program=np;io.erase=ne;
    io.slot_bytes=SLOT_BYTES;io.erase_block_bytes=SLOT_BYTES;
    io.geometry_verified=1U;io.dedicated_slots_verified=1U;
    io.writes_enabled=1U;return io;
}
static int allow(void *ctx){
    return *(const int *)ctx;
}
static void set(ou_config_model *m,const char *key,const char *value){
    assert(ou_config_model_set(m,key,value)==0);
}
static ou_config_model build(void){
    ou_config_model m;
    ou_config_model_init(&m);
    set(&m,"System.Network.HostName","h910p-lab");
    set(&m,"System.Network.DomainName","lab.example");
    set(&m,"System.Network.DHCP","false");
    set(&m,"System.Network.IPAddress","192.168.10.24");
    set(&m,"System.Network.IPMask","255.255.255.0");
    set(&m,"System.Network.IPGateway","192.168.10.1");
    set(&m,"System.Contact","Operations desk");
    set(&m,"System.Location","Rack 02 - Row A");
    set(&m,"System.Language","ENG");
    assert(ou_config_model_validate(&m)==0);
    return m;
}
static void all_fields_and_canonical(void){
    ou_config_model m=build(),copy,before;
    uint8_t buf[OU_CONFIG_RECORD_MAX];
    size_t written=0U;
    unsigned i;
    for(i=0U;i<OU_CONFIG_FIELD_COUNT;++i){
        assert(ou_config_key_name(i)!=NULL);
        assert(ou_config_key_type(i)!=NULL);
        assert(ou_config_key_index(ou_config_key_name(i))==(int)i);
    }
    assert(ou_config_key_name(OU_CONFIG_FIELD_COUNT)==NULL);
    assert(ou_config_key_index("System.Password")==-1);
    assert(ou_config_model_set(&m,"System.Network.SmtpServer.Password","test")==-1);
    assert(ou_config_model_set(&m,"System.Outlet[1].Toggle","120")==-1);
    assert(ou_config_model_set(&m,"System.Contact","Bad\nHeader")==-1);
    assert(ou_config_model_set(&m,"System.Network.HostName","bad..host")==-1);
    assert(ou_config_model_set(&m,"System.Network.IPAddress","192.168.10.024")==-1);
    assert(ou_config_model_set(&m,"System.Network.IPGateway","127.0.0.1")==-1);
    assert(ou_config_model_set(&m,"System.Network.IPMask","255.0.255.0")==-1);
    assert(ou_config_model_set(&m,"System.Language","KLINGON")==-1);
    assert(ou_config_model_validate(&m)==0);
    assert(ou_config_model_encode(&m,buf,sizeof(buf),&written)==0);
    assert(written<OU_CONFIG_RECORD_MAX);
    assert(buf[0]=='O'&&buf[1]=='6'&&buf[2]=='C'&&buf[3]=='F');
    assert(buf[5]==OU_CONFIG_FIELD_COUNT);
    ou_config_model_init(&copy);
    assert(ou_config_model_decode(&copy,buf,written)==0);
    assert(memcmp(&copy,&m,sizeof(m))==0);
    before=copy;
    buf[4]=99U;
    assert(ou_config_model_decode(&copy,buf,written)==-1);
    assert(memcmp(&copy,&before,sizeof(copy))==0);
    buf[4]=OU_CONFIG_RECORD_VERSION;
    /* Duplicate/out-of-order key ID must not mutate loaded state. */
    buf[12U]=8U;
    assert(ou_config_model_decode(&copy,buf,written)==-1);
    assert(memcmp(&copy,&before,sizeof(copy))==0);
}
static void invalid_profiles(void){
    ou_config_model m;
    ou_config_model_init(&m);
    set(&m,"System.Contact","Operations");
    assert(ou_config_model_validate(&m)==0);
    set(&m,"System.Network.DHCP","false");
    assert(ou_config_model_validate(&m)==-1);
    set(&m,"System.Network.IPAddress","10.55.3.20");
    assert(ou_config_model_validate(&m)==-1);
    set(&m,"System.Network.IPMask","255.255.255.0");
    assert(ou_config_model_validate(&m)==0);
    set(&m,"System.Network.IPGateway","10.56.3.1");
    assert(ou_config_model_validate(&m)==-1);
    set(&m,"System.Network.IPGateway","10.55.3.20");
    assert(ou_config_model_validate(&m)==-1);
    set(&m,"System.Network.IPGateway","10.55.3.1");
    assert(ou_config_model_validate(&m)==0);
    set(&m,"System.Network.IPAddress","10.55.3.0");
    assert(ou_config_model_validate(&m)==-1);
    set(&m,"System.Network.IPAddress","10.55.3.255");
    assert(ou_config_model_validate(&m)==-1);
    set(&m,"System.Network.IPAddress","10.55.3.20");
    assert(ou_config_model_validate(&m)==0);
    assert(ou_config_model_clear(&m,"System.Network.IPAddress")==0);
    assert(ou_config_model_validate(&m)==-1);
    set(&m,"System.Network.DHCP","true");
    assert(ou_config_model_validate(&m)==-1);
    assert(ou_config_model_clear(&m,"System.Network.IPMask")==0);
    assert(ou_config_model_clear(&m,"System.Network.IPGateway")==0);
    assert(ou_config_model_validate(&m)==0);
    assert(ou_config_model_clear(&m,"System.Network.DHCP")==0);
    assert(ou_config_model_validate(&m)==0);
}
static void atomic_restore_and_qualification(void){
    ou_config_model m=build(),loaded,before;
    ou_journal_io io;
    ou_journal_record meta;
    int authorization=0;
    unsigned writes,erases;
    blank();io=backend();
    writes=nor.writes;erases=nor.erases;
    assert(ou_config_model_save(&m,&io,allow,&authorization,&meta)==-1);
    assert(nor.writes==writes&&nor.erases==erases);
    authorization=1;
    io.writes_enabled=0U;
    assert(ou_config_model_save(&m,&io,allow,&authorization,&meta)==-1);
    io.writes_enabled=1U;
    io.dedicated_slots_verified=0U;
    assert(ou_config_model_save(&m,&io,allow,&authorization,&meta)==-1);
    io.dedicated_slots_verified=1U;io.geometry_verified=0U;
    assert(ou_config_model_save(&m,&io,allow,&authorization,&meta)==-1);
    assert(nor.writes==writes&&nor.erases==erases);
    io.geometry_verified=1U;
    assert(ou_config_model_save(&m,&io,allow,&authorization,&meta)==0);
    assert(meta.valid&&meta.slot==0U&&meta.generation==1U);
    ou_config_model_init(&loaded);
    assert(ou_config_model_load(&loaded,&io,&meta)==0);
    assert(memcmp(&loaded,&m,sizeof(m))==0);
    set(&m,"System.Location","Rack 03 - Row B");
    nor.fail_after=50;
    assert(ou_config_model_save(&m,&io,allow,&authorization,&meta)==-1);
    nor.fail_after=-1;
    assert(ou_config_model_load(&loaded,&io,&meta)==0);
    assert(strcmp(loaded.values[OU_CONFIG_LOCATION],"Rack 02 - Row A")==0);
    assert(ou_config_model_save(&m,&io,allow,&authorization,&meta)==0);
    assert(meta.valid&&meta.slot==1U&&meta.generation==2U);
    assert(ou_config_model_load(&loaded,&io,&meta)==0);
    assert(strcmp(loaded.values[OU_CONFIG_LOCATION],"Rack 03 - Row B")==0);
    /* Corrupt the latest committed slot; fall back to older proven record. */
    nor.slots[1][OU_JOURNAL_HEADER_BYTES+15U]^=1U;
    assert(ou_config_model_load(&loaded,&io,&meta)==0);
    assert(strcmp(loaded.values[OU_CONFIG_LOCATION],"Rack 02 - Row A")==0);
    before=loaded;
    nor.slots[0][OU_JOURNAL_HEADER_BYTES+15U]^=1U;
    assert(ou_config_model_load(&loaded,&io,&meta)==-1);
    assert(memcmp(&loaded,&before,sizeof(loaded))==0);
}
int main(void){
    all_fields_and_canonical();
    invalid_profiles();
    atomic_restore_and_qualification();
    puts("PASS: 9 typed configuration keys, network safety, canonical record, authorization, atomic journal and recovery");
    return 0;
}
