#include "ou_hid_telemetry.h"
#include "ou_native_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Synthetic descriptor: three HID Feature reports containing nine distinct
 * values. This is NOT a claim about the H910P's actual UPS HID descriptor. */
static const uint8_t descriptor[]={
 0x05,0x85,0x85,1,0x15,0,0x26,0xff,0,0x75,8,0x95,1,
 0x09,0x66,0xb1,2,
 0x05,0x84,0x09,0x35,0xb1,2,
 0x85,2,0x05,0x85,0x26,0xff,0x7f,0x75,16,
 0x09,0x68,0xb1,2,
 0x05,0x84,0x09,0x30,0xb1,2,
 0x09,0x31,0xb1,2,
 0x85,3,0x25,1,0x75,8,
 0x09,0x40,0xb1,2,
 0x05,0x85,0x09,0x41,0xb1,2,
 0x05,0x84,0x09,0x42,0xb1,2,
 0x09,0x43,0xb1,2
};
static const ou_hid_metric_binding binds[OU_HID_TELEMETRY_METRICS]={
 {0x85,0x66,1,1,1}, {0x84,0x35,1,1,1},
 {0x85,0x68,2,1,1}, {0x84,0x30,2,1,1},
 {0x84,0x31,2,1,1}, {0x84,0x40,3,1,1},
 {0x85,0x41,3,1,1}, {0x84,0x42,3,1,1},
 {0x84,0x43,3,1,1}
};
static const ou_hid_source_evidence proof={1,1,1,1};
static ou_hid_telemetry state;
static void initialize(void){
 assert(ou_hid_telemetry_init(&state,descriptor,sizeof(descriptor),
                              binds,&proof)==0);
}
static void feed(const uint8_t *bytes,size_t n,uint32_t timestamp){
 ou_shut_rx_message m={0};
 assert(n<=sizeof(m.payload));
 memcpy(m.payload,bytes,n);
 m.length=(uint16_t)n;
 m.monotonic_ms=timestamp;
 m.packet_type=OU_SHUT_RX_RESPONSE;
 assert(ou_hid_telemetry_ingest(&state,&m,timestamp)==0);
}
static void three(uint32_t first) {
 const uint8_t r1[]={1,87,43};
 const uint8_t r2[]={2,0xb0,0x04,0xa6,0x04,0xb0,0x04};
 const uint8_t r3[]={3,1,1,1,0};
 feed(r1,sizeof(r1),first);
 feed(r2,sizeof(r2),first+1U);
 feed(r3,sizeof(r3),first+2U);
}
static void verify_http(uint32_t ms,int code,const char *needle) {
 static const uint8_t request[]="GET /api/v1/ups/status HTTP/1.0\r\n\r\n";
 char out[OU_HTTP_MAX_RESPONSE+1U];size_t used=0U;
 char expected[32];
 assert(ou_native_service_http(request,sizeof(request)-1U,ms,
                                out,sizeof(out)-1U,&used)==0);
 out[used]='\0';snprintf(expected,sizeof(expected),"HTTP/1.1 %d",code);
 assert(strstr(out,expected)==out && strstr(out,needle)!=NULL);
}
static void normal_and_service(void){
 ou_driver_report r;
 ou_native_service_init();initialize();
 verify_http(1000U,503,"TELEMETRY_UNAVAILABLE");
 assert(ou_hid_telemetry_take(&state,1000U,&r)==-1);
 three(1000U);
 assert(ou_hid_telemetry_take(&state,1003U,&r)==0);
 assert(r.version==OU_REPORT_VERSION && r.verified==1U);
 assert(r.sequence==1U && r.battery_percent==87U && r.load_percent==43U);
 assert(r.runtime_seconds==1200U && r.input_decivolts==1190U);
 assert(r.output_decivolts==1200U && r.utility_present==1U);
 assert(r.battery_present==1U && r.segment_1_on==1U);
 assert(r.segment_2_on==0U);
 assert(ou_native_service_driver_report(&r,1003U)==0);
 verify_http(1004U,200,"\"batteryPercent\":87");
 assert(ou_hid_telemetry_take(&state,1004U,&r)==-1);
 assert(!r.verified); /* No reuse after publication. */
 three(1100U);
 assert(ou_hid_telemetry_take(&state,1104U,&r)==0);
 assert(r.sequence==2U);
 assert(ou_native_service_driver_report(&r,1104U)==0);
 verify_http(1105U,200,"\"batteryPercent\":87");
 verify_http(6105U,503,"TELEMETRY_UNAVAILABLE");
}
static void evidence_and_bindings(void){
 ou_hid_source_evidence bad=proof;
 ou_hid_metric_binding changed[OU_HID_TELEMETRY_METRICS];
 size_t i;
 bad.transaction_origin_verified=0U;
 assert(ou_hid_telemetry_init(&state,descriptor,sizeof(descriptor),
                              binds,&bad)==-1);
 assert(!state.initialized);
 initialize();
 ou_hid_telemetry_revoke(&state);
 assert(!state.initialized && !state.seen);
 for(i=0U;i<OU_HID_TELEMETRY_METRICS;++i)changed[i]=binds[i];
 changed[1]=changed[0];
 assert(ou_hid_telemetry_init(&state,descriptor,sizeof(descriptor),
                              changed,&proof)==-1);
 changed[1]=binds[1];changed[4].usage=0xfa;
 assert(ou_hid_telemetry_init(&state,descriptor,sizeof(descriptor),
                              changed,&proof)==-1);
 changed[4]=binds[4];changed[0].divide=10;
 assert(ou_hid_telemetry_init(&state,descriptor,sizeof(descriptor),
                              changed,&proof)==0);
 {
  const uint8_t r1[]={1,87,43};
  ou_shut_rx_message m={0};
  memcpy(m.payload,r1,sizeof(r1));m.length=sizeof(r1);
  m.packet_type=OU_SHUT_RX_RESPONSE;m.monotonic_ms=500U;
  /* 87 / 10 cannot be represented exactly, so reject without rounding. */
  assert(ou_hid_telemetry_ingest(&state,&m,500U)==-1 && !state.seen);
 }
}
static void invalid_and_replay(void){
 ou_driver_report r;
 ou_shut_rx_message m={0};
 const uint8_t r1[]={1,87,43};
 const uint8_t r2[]={2,0xb0,0x04,0xa6,0x04,0xb0,0x04};
 const uint8_t invalid[]={1,105,43}; /* Above declared battery range. */
 initialize();
 feed(r1,sizeof(r1),1000U);
 m.packet_type=OU_SHUT_RX_NOTIFY;m.monotonic_ms=1001U;
 memcpy(m.payload,r1,sizeof(r1));m.length=sizeof(r1);
 assert(ou_hid_telemetry_ingest(&state,&m,1001U)==-1 && !state.seen);
 feed(r2,sizeof(r2),1002U);
 assert(ou_hid_telemetry_take(&state,1003U,&r)==-1);
 m.packet_type=OU_SHUT_RX_RESPONSE;
 memcpy(m.payload,invalid,sizeof(invalid));m.length=sizeof(invalid);
 m.monotonic_ms=1003U;
 assert(ou_hid_telemetry_ingest(&state,&m,1003U)==-1 && !state.seen);
 three(1010U);
 assert(ou_hid_telemetry_take(&state,1013U,&r)==0);
 m.monotonic_ms=1010U;memcpy(m.payload,r1,sizeof(r1));
 m.length=sizeof(r1);
 assert(ou_hid_telemetry_ingest(&state,&m,1015U)==-1);
 assert(!state.seen);
}
static void skew_and_wrap(void){
 ou_driver_report r;
 initialize();
 three(1000U);
 assert(ou_hid_telemetry_take(&state,5010U,&r)==-1);
 assert(!r.verified && !state.seen);
 initialize();
 three(UINT32_MAX-3U);
 assert(ou_hid_telemetry_take(&state,0U,&r)==0);
 assert(r.sequence==1U && r.verified);
}
int main(void) {
 normal_and_service();evidence_and_bindings();
 invalid_and_replay();skew_and_wrap();
 puts("PASS: descriptor-bound, source-gated SHUT HID telemetry -> native HTTP; atomic, replay, stale and missing fields");
 return 0;
}
