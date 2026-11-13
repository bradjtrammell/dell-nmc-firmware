#include "ou_hid_report.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void real_shape(void){
 /* Synthetic descriptor for the MGE p14 report layout, NOT actual H910P
  * descriptor. Battery values are raw integers with no scaling guesses. */
 static const uint8_t desc[]={
  0x05,0x85, 0x85,0x16,
  0x15,0x00, 0x25,100, 0x75,8, 0x95,1,
  0x09,0x66, 0xb1,0x02,
  0x26,0xff,0x7f, 0x75,16, 0x95,1,
  0x09,0x68, 0xb1,0x02
 };
 static const uint8_t response[]={0x16,100,0x08,0x07};
 ou_hid_feature_map map;ou_hid_feature_value values[4];size_t count=0U;
 assert(ou_hid_feature_parse(desc,sizeof(desc),&map)==0);
 assert(map.field_count==2U&&map.has_report_ids);
 assert(map.fields[0].bit_offset==0U&&map.fields[1].bit_offset==8U);
 assert(ou_hid_feature_decode(&map,response,sizeof(response),values,4U,&count)==0);
 assert(count==2U&&values[0].usage_page==0x85U);
 assert(values[0].usage==0x66U&&values[0].value==100);
 assert(values[1].usage==0x68U&&values[1].value==1800);
 assert(!values[0].telemetry_verified&&!values[1].telemetry_verified);
 assert(ou_hid_feature_decode(&map,response,2U,values,4U,&count)==-1);
 assert(ou_hid_feature_decode(&map,response,sizeof(response),values,1U,&count)==-1);
 /* Invalid later field must not partially overwrite previous valid values. */
 {
  uint8_t invalid[]={0x16,99,0xff,0xff};
  ou_hid_feature_value unchanged[4];
  memcpy(unchanged,values,sizeof(values));
  assert(ou_hid_feature_decode(&map,invalid,sizeof(invalid),values,4U,&count)==-1);
  assert(count==0U && memcmp(unchanged,values,sizeof(values))==0);
 }
}
static void signed_and_padding(void){
 static const uint8_t desc[]={
  0x05,0x84, 0x15,0xf6, 0x25,10,
  0x75,4,0x95,2,0xb1,0x01, /* 8 constant padding bits */
  0x09,0x30,0x75,8,0x95,1,0xb1,0x02
 };
 static const uint8_t data[]={0x00,0xfc}; /* -4 */
 ou_hid_feature_map map;ou_hid_feature_value out[2];size_t n;
 assert(ou_hid_feature_parse(desc,sizeof(desc),&map)==0);
 assert(map.field_count==1U&&map.fields[0].bit_offset==8U);
 assert(ou_hid_feature_decode(&map,data,sizeof(data),out,2U,&n)==0);
 assert(n==1U&&out[0].value==-4);
}
static void multiple_reports(void){
 static const uint8_t desc[]={
  0x05,0x84,0x15,0,0x25,100,0x75,8,0x95,1,
  0x85,0x01,0x09,0x35,0xb1,0x02,
  0x85,0x02,0x09,0x36,0xb1,0x02
 };
 static const uint8_t r1[]={1,45},r2[]={2,20},bad[]={3,20};
 ou_hid_feature_map map;ou_hid_feature_value out[2];size_t n;
 assert(ou_hid_feature_parse(desc,sizeof(desc),&map)==0);
 assert(map.field_count==2U&&map.fields[0].bit_offset==0U);
 assert(map.fields[1].bit_offset==0U);
 assert(ou_hid_feature_decode(&map,r1,sizeof(r1),out,2U,&n)==0);
 assert(n==1U&&out[0].usage==0x35U&&out[0].value==45);
 assert(ou_hid_feature_decode(&map,r2,sizeof(r2),out,2U,&n)==0);
 assert(n==1U&&out[0].usage==0x36U&&out[0].value==20);
 assert(ou_hid_feature_decode(&map,bad,sizeof(bad),out,2U,&n)==-1);
}
static void adversarial(void){
 static const uint8_t truncated[]={0x05};
 static const uint8_t longitem[]={0xfe,0,0};
 static const uint8_t unbalanced[]={0xc0};
 static const uint8_t huge[]={0x05,0x85,0x15,0,0x25,100,0x75,32,
                               0x96,0x01,0x01,0x09,0x66,0xb1,2};
 static const uint8_t badpop[]={0xb4};
 static const uint8_t nofields[]={0x05,0x84};
 static const uint8_t idzero[]={0x05,0x84,0x85,0x00};
 ou_hid_feature_map m;
 assert(ou_hid_feature_parse(truncated,sizeof(truncated),&m)==-1);
 assert(ou_hid_feature_parse(longitem,sizeof(longitem),&m)==-1);
 assert(ou_hid_feature_parse(unbalanced,sizeof(unbalanced),&m)==-1);
 assert(ou_hid_feature_parse(huge,sizeof(huge),&m)==-1);
 assert(ou_hid_feature_parse(badpop,sizeof(badpop),&m)==-1);
 assert(ou_hid_feature_parse(nofields,sizeof(nofields),&m)==-1);
 assert(ou_hid_feature_parse(idzero,sizeof(idzero),&m)==-1);
}
static void fuzz(void){
 uint8_t raw[96];uint32_t seed=UINT32_C(0x8baba123);unsigned i,j;
 ou_hid_feature_map m;
 for(i=0U;i<6000U;i++){
  for(j=0U;j<sizeof(raw);j++){
   seed=seed*UINT32_C(1664525)+UINT32_C(1013904223);
   raw[j]=(uint8_t)(seed>>24);
  }
  assert(ou_hid_feature_parse(raw,sizeof(raw),&m)>=-1);
  assert(m.field_count<=OU_HID_FIELDS_CAP);
 }
}
int main(void){
 real_shape();signed_and_padding();multiple_reports();adversarial();fuzz();
 puts("PASS: bounded HID Power/Battery Feature descriptor fields, report IDs, signed values and malformed/fuzz");
 return 0;
}
