#include "ou_shut_session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static ou_shut_session session;
static uint8_t tx[16];
static size_t used;
static uint32_t now;
static void reset(void) {
 ou_shut_session_init(&session);now=1000U;used=0U;
}
static int take(void) {
 return ou_shut_session_take_tx(&session,now,tx,sizeof(tx),&used);
}
static void sync_sent_request(uint8_t id,uint16_t requested){
 reset();
 assert(ou_shut_session_begin(&session,id,requested,now,1U,1U)==1);
 assert(session.state==OU_SHUT_SESSION_SYNC_READY);
 assert(take()==1 && used==1U && tx[0]==0x16U);
 assert(session.state==OU_SHUT_SESSION_SYNC_WAIT);
 assert(ou_shut_session_feed(&session,0x16U,++now)==1);
 assert(session.state==OU_SHUT_SESSION_REQUEST_READY);
 assert(take()==1&&used==11U&&tx[0]==0x81U&&tx[1]==0x88U);
 assert(tx[4]==id&&tx[8]==(uint8_t)(requested&255U));
 assert(tx[9]==(uint8_t)(requested>>8));
}
static void sync_request(uint8_t id,uint16_t requested){
 sync_sent_request(id,requested);
 assert(ou_shut_session_feed(&session,0x06U,++now)==0);
 assert(session.state==OU_SHUT_SESSION_RESPONSE_WAIT);
}
static int packet(uint8_t type,const uint8_t*p,size_t len){
 size_t i;uint8_t checksum=0U;int r;
 assert(len>=1U&&len<=8U);
 assert(ou_shut_session_feed(&session,type,++now)==0);
 assert(ou_shut_session_feed(&session,(uint8_t)(len*17U),++now)==0);
 for(i=0;i<len;i++){
  checksum^=p[i];
  assert(ou_shut_session_feed(&session,p[i],++now)==0);
 }
 r=ou_shut_session_feed(&session,checksum,++now);
 return r;
}
static void nominal(void){
 static const uint8_t data[]={0x16,0x64,0x08,0x07};
 ou_shut_rx_message got;
 sync_request(0x16U,8U);
 assert(packet(0x84U,data,4U)==1);
 assert(session.state==OU_SHUT_SESSION_ACK_READY);
 assert(session.result.telemetry_verified==0U);
 assert(ou_shut_session_take_report(&session,&got)==0);
 assert(take()==1&&used==1U&&tx[0]==0x06U);
 assert(session.state==OU_SHUT_SESSION_COMPLETE);
 assert(ou_shut_session_take_report(&session,&got)==1);
 assert(got.length==4U&&memcmp(got.payload,data,4U)==0);
 assert(got.telemetry_verified==0U);
 assert(session.state==OU_SHUT_SESSION_IDLE);
 assert(ou_shut_session_take_report(&session,&got)==0);
}
static void fragments_and_notify(void){
 static const uint8_t notify[]={0x02U,0x24U};
 static const uint8_t part1[]={0x31,1,2,3,4,5,6,7};
 static const uint8_t part2[]={8,9,10};
 ou_shut_rx_message got;
 sync_request(0x31U,0U);
 assert(packet(0x85U,notify,2U)==1);
 assert(take()==1&&tx[0]==0x06U&&session.state==OU_SHUT_SESSION_RESPONSE_WAIT);
 assert(packet(0x04U,part1,8U)==0);
 assert(take()==1&&tx[0]==0x06U&&session.state==OU_SHUT_SESSION_RESPONSE_WAIT);
 assert(packet(0x84U,part2,3U)==1);
 assert(take()==1&&tx[0]==0x06U&&session.state==OU_SHUT_SESSION_COMPLETE);
 assert(ou_shut_session_take_report(&session,&got)==1);
 assert(got.length==11U&&got.payload[0]==0x31U&&got.payload[10]==10U);
}
static void sync_timeout(void){
 unsigned i;
 reset();assert(ou_shut_session_begin(&session,9U,0U,now,1U,1U)==1);
 for(i=0U;i<OU_SHUT_SESSION_ATTEMPTS;i++){
  assert(take()==1&&tx[0]==0x16U);
  now+=OU_SHUT_SESSION_SYNC_INTERVAL_MS;
  if(i+1U<OU_SHUT_SESSION_ATTEMPTS){
   assert(ou_shut_session_tick(&session,now)==1);
   assert(session.state==OU_SHUT_SESSION_SYNC_READY);
  }else{
   assert(ou_shut_session_tick(&session,now)==-1);
   assert(session.state==OU_SHUT_SESSION_FAILED);
  }
 }
}
static void command_nak_retry(void){
 unsigned i;
 reset();assert(ou_shut_session_begin(&session,0x31U,8U,now,0U,1U)==-1);
 assert(session.state==OU_SHUT_SESSION_IDLE);
 sync_sent_request(0x31U,8U);
 for(i=1U;i<OU_SHUT_SESSION_REQUEST_ATTEMPTS;i++){
  assert(ou_shut_session_feed(&session,0x15U,++now)==1);
  assert(take()==1&&used==11U&&tx[4]==0x31U);
 }
 assert(ou_shut_session_feed(&session,0x15U,++now)==-1);
 assert(session.state==OU_SHUT_SESSION_FAILED);
}
static void response_faults(void){
 static const uint8_t wrong[]={0x22U,0x01U};
 static const uint8_t valid[]={0x31U,0x01U};
 unsigned i;
 sync_request(0x31U,0U);
 assert(packet(0x84U,wrong,2U)==-1);
 assert(session.error==OU_SHUT_SESSION_ERR_REPORT_MISMATCH);
 sync_request(0x31U,0U);
 assert(ou_shut_session_feed(&session,0x84U,++now)==0);
 assert(ou_shut_session_feed(&session,0x23U,++now)==-1);
 assert(session.error==OU_SHUT_SESSION_ERR_BAD_PACKET);
 sync_request(0x31U,0U);
 assert(ou_shut_session_feed(&session,0x84U,++now)==0);
 assert(ou_shut_session_feed(&session,0x11U,++now)==0);
 assert(ou_shut_session_feed(&session,0x31U,++now)==0);
 assert(ou_shut_session_feed(&session,0x00U,++now)==-1);
 sync_request(0x31U,0U);
 for(i=0U;i<2U;++i)assert(ou_shut_session_tick(&session,now+1000U)==0);
 now+=1600U;
 assert(ou_shut_session_tick(&session,now)==-1);
 sync_request(0x31U,0U);
 assert(packet(0x84U,valid,2U)==1);
 assert(take()==1&&session.state==OU_SHUT_SESSION_COMPLETE);
}
static void framing_clock_and_limits(void) {
 ou_shut_rx_message out;
 reset();assert(ou_shut_session_begin(&session,0x11U,257U,now,1U,1U)==-1);
 assert(ou_shut_session_begin(&session,0x11U,0U,now,1U,1U)==1);
 assert(ou_shut_session_take_tx(&session,now,tx,0U,&used)==-1&&!used);
 assert(take()==1);
 now=900U;
 assert(ou_shut_session_tick(&session,now)==-1);
 assert(session.error==OU_SHUT_SESSION_ERR_CLOCK);
 assert(ou_shut_session_take_report(&session,&out)==0);
 reset();assert(ou_shut_session_begin(&session,0x11U,8U,now,1U,1U)==1);
 now+=OU_SHUT_SESSION_ACK_TIMEOUT_MS+1U;
 assert(ou_shut_session_tick(&session,now)==-1);
 assert(session.error==OU_SHUT_SESSION_ERR_TIMEOUT);
}
int main(void){
 nominal();fragments_and_notify();sync_timeout();command_nak_retry();
 response_faults();framing_clock_and_limits();
 puts("PASS: SHUT read-only SYNC/GET REPORT/ACK state machine, NAKs, timeout, checksum and replay gates");
 return 0;
}
