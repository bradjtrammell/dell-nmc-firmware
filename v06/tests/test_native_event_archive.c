#include "ou_native_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Separate synthetic event partition; NEVER a real H910P flash address. */
#define SLOT_BYTES 8192U
static uint8_t rom[2][SLOT_BYTES];
static unsigned writes,erases;
static int rd(void *ctx,unsigned slot,uint32_t off,uint8_t *out,size_t len){
 (void)ctx;
 if(slot>1U||off>SLOT_BYTES||len>SLOT_BYTES-off)return -1;
 memcpy(out,rom[slot]+off,len);return 0;
}
static int wr(void *ctx,unsigned slot,uint32_t off,const uint8_t *in,size_t len){
 size_t i;(void)ctx;
 if(slot>1U||off>SLOT_BYTES||len>SLOT_BYTES-off)return -1;
 for(i=0U;i<len;++i){
    if(((uint8_t)~rom[slot][off+i]&in[i])!=0U)return -1;
    rom[slot][off+i]&=in[i];
    ++writes;
 }
 return 0;
}
static int erase(void *ctx,unsigned slot) {
 (void)ctx;
 if(slot>1U)return -1;
 memset(rom[slot],0xff,SLOT_BYTES);++erases;return 0;
}
static ou_journal_io backend(void) {
 ou_journal_io io={0};
 io.read=rd;io.program=wr;io.erase=erase;
 io.slot_bytes=SLOT_BYTES;io.erase_block_bytes=SLOT_BYTES;
 io.geometry_verified=1U;io.dedicated_slots_verified=1U;
 io.writes_enabled=1U;return io;
}
static void expect(uint32_t clock_ms,const char *snippet){
 static const uint8_t req[]="GET /api/v1/events HTTP/1.0\r\n\r\n";
 char out[OU_HTTP_MAX_RESPONSE+1U];size_t n=0U;
 assert(ou_native_service_http(req,sizeof(req)-1U,clock_ms,
                                out,sizeof(out)-1U,&n)==0);
 out[n]='\0';assert(strstr(out,"HTTP/1.1 200")==out);
 assert(strstr(out,snippet)!=0);
}
int main(void){
 ou_journal_record record;ou_journal_io io;
 memset(rom,0xff,sizeof(rom));io=backend();
 ou_native_service_init();
 expect(1000U,"\"currentBootGeneration\":1");
 expect(1000U,"\"code\":\"SERVICE_START\"");
 assert(ou_native_service_events_restore(&io)==-1);
 io.writes_enabled=0U;
 assert(ou_native_service_events_checkpoint(&io,&record)==-1);
 assert(!writes&&!erases);
 io.writes_enabled=1U;
 assert(ou_native_service_events_checkpoint(&io,&record)==0);
 assert(record.valid && record.generation==1U);
 assert(writes && erases==1U);
 /* Simulated cold reset: no read of previously observed live UPS values. */
 ou_native_service_init();
 assert(ou_native_service_events_restore(&io)==0);
 expect(0U,"\"currentBootGeneration\":2");
 expect(0U,"\"bootGeneration\":1");
 expect(0U,"\"bootGeneration\":2");
 expect(0U,"\"retention\":\"volatile-until-explicit-checkpoint\"");
 /* Restored history is unavailable for a second initialization attempt,
  * but the current event ring is always readable on an unavailable UPS. */
 {
   ou_driver_report r={0};
   r.version=OU_REPORT_VERSION;r.verified=1U;r.sequence=1U;
   r.received_monotonic_ms=200U;
   r.battery_percent=99U;r.load_percent=20U;
   r.battery_present=1U;r.utility_present=1U;
   r.input_decivolts=1200U;r.output_decivolts=1200U;
   r.runtime_seconds=1000U;
   assert(ou_native_service_driver_report(&r,200U)==0);
   assert(ou_native_service_events_restore(&io)==-1);
 }
 expect(201U,"\"code\":\"TELEMETRY_ONLINE\"");
 assert(ou_native_service_events_checkpoint(&io,&record)==0);
 assert(record.generation==2U);
 puts("PASS: native event checkpoint/restore, boot generations, source guard, read-only HTTP and no automatic flash writes");
 return 0;
}
