#include "ou_shut_request.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 uint8_t b[13],old[13];size_t count=999U;unsigned id,i;
 for(id=0U;id<256U;++id){
  uint8_t checksum=0U;
  memset(b,0xa5,sizeof(b));
  assert(ou_shut_build_get_report((uint8_t)id,b+1,11U,&count)==0);
  assert(count==11U && b[0]==0xa5U && b[12]==0xa5U);
  assert(b[1]==0x81U && b[2]==0x88U && b[3]==0xa1U && b[4]==0x01U);
  assert(b[5]==(uint8_t)id && b[6]==0x03U);
  for(i=7U;i<=10U;++i)assert(b[i]==0U);
  for(i=3U;i<=10U;++i)checksum^=b[i];
  assert(b[11]==checksum && checksum==(uint8_t)(0xa3U^id));
 }
 /* MGE 51029473ZA AC p.14 published GET REPORT 0x16 for 8 bytes:
  * 81 88 A1 01 16 03 00 00 08 00 BD. */
 {
  static const uint8_t published[]={0x81,0x88,0xa1,0x01,0x16,0x03,
                                    0x00,0x00,0x08,0x00,0xbd};
  memset(b,0xa5,sizeof(b));
  assert(ou_shut_build_get_report_sized(0x16U,8U,b+1,11U,&count)==0);
  assert(count==11U && memcmp(b+1,published,11U)==0);
  assert(b[0]==0xa5U && b[12]==0xa5U);
 }
 memset(b,0xa5,sizeof(b));memcpy(old,b,sizeof(b));
 assert(ou_shut_build_get_report_sized(3,257U,b,11U,&count)==-1&&!count);
 assert(!memcmp(b,old,sizeof(b)));
 assert(ou_shut_build_get_report_sized(3,256U,b,10U,&count)==-1&&!count);
 assert(!memcmp(b,old,sizeof(b)));
 assert(ou_shut_build_get_report_sized(3,0U,0,11U,&count)==-1&&!count);
 assert(ou_shut_build_get_report(3,b,10U,&count)==-1&&!count);
 assert(!memcmp(b,old,sizeof(b)));
 assert(ou_shut_build_get_report(3,0,11U,&count)==-1&&!count);
 assert(ou_shut_build_get_report(3,b,11U,0)==-1);
 puts("PASS: SHUT GET REPORT 256 IDs, published-sized frame, checksum and boundaries");
 return 0;
}
