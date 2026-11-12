#include "ou_vlan_record.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8_t mac[6]={2,3,4,5,6,7};
static ou_vlan_table table,recovered;
static uint8_t bytes[OU_JOURNAL_MAX_PAYLOAD];
static void init(unsigned count){
 ou_vlan_interface e[OU_VLAN_MAX_INTERFACES]={0};unsigned i;
 assert(count>0U&&count<=OU_VLAN_MAX_INTERFACES);
 for(i=0;i<count;i++) {
   e[i].vid=(uint16_t)(100U+i);
   e[i].ipv4[0]=10;e[i].ipv4[1]=(uint8_t)i;e[i].ipv4[2]=1;e[i].ipv4[3]=9;
   e[i].prefix_length=24;e[i].tx_pcp=5;e[i].allow_local_subnet=1;
   e[i].allowed_services=OU_VLAN_SERVICE_ARP|OU_VLAN_SERVICE_PING;
   e[i].acl_count=1;e[i].acl[0].address[0]=172;
   e[i].acl[0].address[1]=16;e[i].acl[0].address[2]=(uint8_t)i;
   e[i].acl[0].prefix_length=24;
 }
 assert(ou_vlan_configure(&table,mac,e,count,0,1,0,0,0)==0);
}
static void test_roundtrip(void){
 size_t n;unsigned i;
 for(i=1;i<=64;i++) {
   init(i);
   assert(ou_vlan_validate(&table));
   assert(ou_vlan_record_encode(&table,bytes,sizeof(bytes),&n)==0);
   assert(n==OU_VLAN_RECORD_HEADER+(size_t)i*OU_VLAN_RECORD_INTERFACE);
   assert(ou_vlan_record_decode(&recovered,bytes,n)==0);
   assert(recovered.count==i && recovered.trunk_enabled);
   assert(recovered.interfaces[i-1].vid==(uint16_t)(99U+i));
   assert(recovered.interfaces[i-1].acl_count==1);
   assert(recovered.interfaces[i-1].acl[0].address[2]==i-1U);
   assert(!recovered.received&&!recovered.statistics[0].received);
 }
 assert(n<=OU_JOURNAL_MAX_PAYLOAD);
}
static void test_corruption(void){
 size_t n;init(64);
 assert(ou_vlan_record_encode(&table,bytes,sizeof(bytes),&n)==0);
 bytes[8]^=1U;assert(ou_vlan_record_decode(&recovered,bytes,n)==-1);
 assert(!recovered.count);
 bytes[8]^=1U;
 bytes[n-1]^=1U;assert(ou_vlan_record_decode(&recovered,bytes,n)==-1);
 bytes[n-1]^=1U;
 assert(ou_vlan_record_decode(&recovered,bytes,n-1U)==-1);
 bytes[4]=2;assert(ou_vlan_record_decode(&recovered,bytes,n)==-1);
 bytes[4]=1;
 assert(ou_vlan_record_decode(&recovered,bytes,n)==0);
 assert(ou_vlan_record_encode(&table,bytes,n-1U,&n)==-1);
}
static void test_journal_gates(void){
 size_t n;ou_journal_io io={0};ou_journal_record m={0};
 init(4);
 assert(ou_vlan_record_encode(&table,bytes,sizeof(bytes),&n)==0);
 assert(ou_vlan_journal_save(&io,&table,bytes,sizeof(bytes),&m)==-1);
 assert(ou_vlan_journal_load(&io,&recovered,bytes,sizeof(bytes),&m)==-1);
 assert(!recovered.count);
}
int main(void){
 test_roundtrip();test_corruption();test_journal_gates();
 puts("PASS: VLAN 1..64 CRC record, ACL roundtrip and journal policy guards");
 return 0;
}
