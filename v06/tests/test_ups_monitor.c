#include "ou_ups_monitor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Device-independent test: synthetic HID descriptor and NS7520 MMIO model.
 * Never identifies a real H910P UPS bus, HID descriptor, or wire bitrate. */
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
static const ou_hid_metric_binding bindings[OU_HID_TELEMETRY_METRICS]={
 {0x85,0x66,1,1,1}, {0x84,0x35,1,1,1},
 {0x85,0x68,2,1,1}, {0x84,0x30,2,1,1},
 {0x84,0x31,2,1,1}, {0x84,0x40,3,1,1},
 {0x85,0x41,3,1,1}, {0x84,0x42,3,1,1},
 {0x84,0x43,3,1,1}
};
static const ou_hid_source_evidence fixture_proof={1,1,1,1};
static const uint8_t r1[]={1,87,43};
static const uint8_t r2[]={2,0xb0,0x04,0xa6,0x04,0xb0,0x04};
static const uint8_t r3[]={3,1,1,1,0};
static ou_ups_monitor mon;
static struct {
 uint8_t incoming[256],outgoing[256];
 size_t received,consumed,sent;
 unsigned stage,forbidden,selected_channel;
 int fail_read,corrupt;
} hw;
static uint32_t now_ms;

static uint32_t base(void) {
 return hw.selected_channel==1U?OU_NS7520_SER1_STATUS_A:
                               OU_NS7520_SER2_STATUS_A;
}
static int rd(void *ctx,uint32_t addr,uint32_t *out) {
 size_t count,i;(void)ctx;
 if(hw.fail_read)return -1;
 if(addr==base()-8U){*out=UINT32_C(0x83000000);return 0;}
 if(addr==base()-4U){*out=0U;return 0;}
 if(addr==base()){
    *out=9U;
    count=hw.received-hw.consumed;
    if(count) {
      if(count>4U)count=4U;
      *out|=OU_NS7520_SER_RX_READY|
           ((uint32_t)(count==4U?0U:count)<<20U);
    }
    return 0;
 }
 if(addr==base()+8U) {
   count=hw.received-hw.consumed;
   if(!count)return -1;
   if(count>4U)count=4U;
   *out=0U;
   for(i=0U;i<count;++i)
      *out|=(uint32_t)hw.incoming[hw.consumed++]<<(24U-8U*i);
   return 0;
 }
 ++hw.forbidden;return -1;
}
static int wr8(void *ctx,uint32_t addr,uint8_t value){
 (void)ctx;
 if(addr!=base()+8U || hw.sent>=sizeof(hw.outgoing)){
  ++hw.forbidden;return -1;
 }
 hw.outgoing[hw.sent++]=value;
 return 0;
}
static int wr32(void *ctx,uint32_t addr,uint32_t value){
 (void)ctx;(void)addr;(void)value;
 ++hw.forbidden;return -1;
}
static ou_ns7520_serial_rx source(unsigned ch) {
 ou_ns7520_serial_rx s={0};
 hw.selected_channel=ch;
 s.read32=rd;s.channel=(uint8_t)ch;
 s.peripheral_mapping_verified=1U;
 s.uart_fifo_mode_verified=1U;
 s.receive_byte_order_verified=1U;
 s.exclusive_channel_ownership=1U;
 s.ups_physical_wiring_verified=1U;
 return s;
}
static void inject(const uint8_t *bytes,size_t len){
 assert(hw.received+len<=sizeof(hw.incoming));
 memcpy(hw.incoming+hw.received,bytes,len);hw.received+=len;
}
static void next_rx(void) {
 const ou_shut_session *s=&mon.link.session;
 const uint8_t *data=0;size_t size=0,i;
 uint8_t packet[12],crc=0U;
 static const uint8_t sync[]={0x16},ack[]={0x06};
 if(s->state==OU_SHUT_SESSION_SYNC_READY && hw.stage==3U)hw.stage=0U;
 if(s->state==OU_SHUT_SESSION_SYNC_WAIT && hw.stage==0U) {
    inject(sync,sizeof(sync));hw.stage=1U;
 } else if(s->state==OU_SHUT_SESSION_REQUEST_WAIT && hw.stage==1U) {
    inject(ack,sizeof(ack));hw.stage=2U;
 } else if(s->state==OU_SHUT_SESSION_RESPONSE_WAIT && hw.stage==2U) {
    if(s->report_id==1U){data=r1;size=sizeof(r1);}
    if(s->report_id==2U){data=r2;size=sizeof(r2);}
    if(s->report_id==3U){data=r3;size=sizeof(r3);}
    assert(data&&size && size<=8U);
    packet[0]=0x84U;
    packet[1]=(uint8_t)(size*17U);
    for(i=0U;i<size;++i){packet[i+2U]=data[i];crc^=data[i];}
    packet[size+2U]=(uint8_t)(crc^(hw.corrupt?0x11U:0U));
    inject(packet,size+3U);
    hw.stage=3U;
 }
}
static void status(int expected,const char *contains) {
 static const uint8_t req[]="GET /api/v1/ups/status HTTP/1.0\r\n\r\n";
 char body[OU_HTTP_MAX_RESPONSE+1U],prefix[40];
 size_t len=0U;
 assert(ou_native_service_http(req,sizeof(req)-1U,now_ms,
                                body,sizeof(body)-1U,&len)==0);
 body[len]='\0';
 snprintf(prefix,sizeof(prefix),"HTTP/1.1 %d",expected);
 assert(strstr(body,prefix)==body);
 assert(strstr(body,contains)!=NULL);
}
static void boot(unsigned ch) {
 ou_ns7520_serial_rx s;
 memset(&hw,0,sizeof(hw));
 now_ms=1000U;
 ou_native_service_init();
 mon=(ou_ups_monitor){0};
 s=source(ch);
 assert(ou_ups_monitor_bind(&mon,&s,wr8,wr32,0,1U,1U,1U,2400U,
       descriptor,sizeof(descriptor),bindings,&fixture_proof)==0);
 assert(mon.report_count==3U);
 assert(mon.report_id[0]==1U&&mon.report_length[0]==3U);
 assert(mon.report_id[1]==2U&&mon.report_length[1]==7U);
 assert(mon.report_id[2]==3U&&mon.report_length[2]==5U);
}
static void run_until(unsigned target){
 unsigned i;
 for(i=0U;i<750U&&mon.published_sets<target;++i) {
    int r;
    next_rx();
    r=ou_ups_monitor_step(&mon,now_ms);
    assert(r>=0);
    now_ms+=5U;
 }
 assert(mon.published_sets==target);
}
static void inspect_frames(void) {
 size_t i,n=0U;
 while(i=0U, n+13U<=hw.sent) {
    uint8_t checksum=0U;
    assert(hw.outgoing[n]==0x16U);
    assert(hw.outgoing[n+1U]==0x81U);
    assert(hw.outgoing[n+2U]==0x88U);
    assert(hw.outgoing[n+3U]==0xa1U);
    assert(hw.outgoing[n+4U]==0x01U);
    assert(hw.outgoing[n+6U]==0x03U);
    assert(hw.outgoing[n+7U]==0U&&hw.outgoing[n+8U]==0U);
    for(i=3U;i<11U;++i)checksum^=hw.outgoing[n+i];
    assert(checksum==hw.outgoing[n+11U]);
    assert(hw.outgoing[n+12U]==0x06U);
    n+=13U;
 }
 assert(n==hw.sent);
}
static void roundtrip(void) {
 unsigned ch;
 for(ch=1U;ch<=2U;++ch){
   boot(ch);
   status(503,"TELEMETRY_UNAVAILABLE");
   run_until(1U);
   status(200,"\"batteryPercent\":87");
   assert(mon.hid.sequence==1U&&mon.published_sets==1U);
   assert(mon.link.reports_completed==3U);
   inspect_frames();
   /* Scheduler holds after a successful coherent telemetry set. */
   {
     unsigned sent=(unsigned)hw.sent;
     assert(ou_ups_monitor_step(&mon,now_ms)==0);
     assert(hw.sent==sent);
   }
   now_ms+=OU_UPS_MONITOR_PERIOD_MS;
   run_until(2U);
   status(200,"\"batteryPercent\":87");
   assert(mon.hid.sequence==2U&&mon.link.reports_completed==6U);
   inspect_frames();
   assert(!hw.forbidden);
   ou_ups_monitor_revoke(&mon);
   status(503,"TELEMETRY_UNAVAILABLE");
   assert(ou_ups_monitor_step(&mon,now_ms)==-1);
 }
}
static void fault_revokes_immediately(void) {
 boot(1U);run_until(1U);
 status(200,"\"batteryPercent\":87");
 now_ms+=OU_UPS_MONITOR_PERIOD_MS+1U;
 hw.fail_read=1;
 assert(ou_ups_monitor_step(&mon,now_ms)==-1);
 assert(mon.failures==1U&&!mon.initialized);
 status(503,"TELEMETRY_UNAVAILABLE");
 assert(ou_ups_monitor_step(&mon,now_ms)==-1);
}
static void corruption_never_publishes(void) {
 unsigned i;int failed=0;
 boot(2U);hw.corrupt=1;
 for(i=0U;i<400U;++i) {
    next_rx();
    if(ou_ups_monitor_step(&mon,now_ms)<0){failed=1;break;}
    now_ms+=5U;
 }
 assert(failed&&mon.failures==1U);
 assert(!mon.initialized&&!mon.published_sets);
 status(503,"TELEMETRY_UNAVAILABLE");
}
static void refuse_bad_source(void) {
 ou_ns7520_serial_rx s;
 ou_hid_source_evidence invalid=fixture_proof;
 ou_hid_metric_binding changed[OU_HID_TELEMETRY_METRICS];
 size_t i;
 boot(1U);ou_ups_monitor_revoke(&mon);
 s=source(1U);invalid.electrical_link_verified=0U;
 assert(ou_ups_monitor_bind(&mon,&s,wr8,wr32,0,1U,1U,1U,2400U,
       descriptor,sizeof(descriptor),bindings,&invalid)==-1);
 assert(!mon.initialized);
 for(i=0U;i<OU_HID_TELEMETRY_METRICS;++i)changed[i]=bindings[i];
 changed[1]=changed[0];
 assert(ou_ups_monitor_bind(&mon,&s,wr8,wr32,0,1U,1U,1U,2400U,
       descriptor,sizeof(descriptor),changed,&fixture_proof)==-1);
 assert(!mon.initialized);
 assert(ou_ups_monitor_bind(&mon,&s,wr8,wr32,0,0U,1U,1U,2400U,
       descriptor,sizeof(descriptor),bindings,&fixture_proof)==-1);
 assert(!mon.initialized);
}
int main(void) {
 roundtrip();fault_revokes_immediately();
 corruption_never_publishes();refuse_bad_source();
 puts("PASS: descriptor-derived 3-report UART/SHUT/HID -> native HTTP, refresh, unplug and corruption");
 return 0;
}
