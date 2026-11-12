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
 memset(b,0xa5,sizeof(b));memcpy(old,b,sizeof(b));
 assert(ou_shut_build_get_report(3,b,10U,&count)==-1&&!count);
 assert(!memcmp(b,old,sizeof(b)));
 assert(ou_shut_build_get_report(3,0,11U,&count)==-1&&!count);
 assert(ou_shut_build_get_report(3,b,11U,0)==-1);
 puts("PASS: SHUT GET REPORT read-only packet builder, 256 IDs, checksum, boundaries");
 return 0;
}
