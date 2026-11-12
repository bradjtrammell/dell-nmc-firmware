#include "ou_shut_rx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static ou_shut_rx decoder;
static ou_link_observer observer;
static ou_shut_rx_message report;
static unsigned event_count;
static uint32_t tick;
static int next(uint8_t byte) {
    int status=ou_shut_rx_feed(&decoder, byte, tick++, &observer, &report);
    if (status==1) ++event_count;
    return status;
}
static int frame(uint8_t kind,const uint8_t*payload,unsigned size) {
    unsigned i;uint8_t sum=0U;int status;
    assert(size>=1 && size<=8);
    assert(next(kind)==0);
    assert(next((uint8_t)((size<<4U)|size))==0);
    for(i=0U;i<size;++i){sum^=payload[i];assert(next(payload[i])==0);}
    status=next(sum);
    return status;
}
static void reset(void) {
    ou_shut_rx_init(&decoder);ou_link_init(&observer);
    tick=42U;event_count=0U;
}
static void single_and_observation(void) {
    const uint8_t payload[]={0x0f,0x78,0x00,0x00};ou_link_record meta;
    reset();assert(frame(0x84,payload,4U)==1);
    assert(report.length==4U&&report.packet_type==4U);
    assert(memcmp(report.payload,payload,4U)==0&&!report.telemetry_verified);
    assert(decoder.completed_messages==1U && decoder.accepted_packets==1U);
    assert(observer.reports_seen==1U);
    assert(ou_link_recent(&observer,0,&meta)==0);
    assert(meta.size==4U && meta.channel==OU_LINK_CHANNEL_OTHER);
    assert(!decoder.active&&!decoder.assembled);
}
static void multi_and_notify(void) {
    const uint8_t part1[]={1,2,3,4,5,6,7,8};
    const uint8_t part2[]={9,10,11};
    reset();assert(frame(0x04,part1,8U)==0);
    assert(decoder.active && decoder.assembled==8U);
    assert(frame(0x84,part2,3U)==1);
    assert(report.length==11U && report.packet_type==OU_SHUT_RX_RESPONSE);
    assert(memcmp(report.payload,part1,8U)==0);
    assert(memcmp(report.payload+8,part2,3U)==0);
    assert(frame(0x85,part2,3U)==1);
    assert(report.packet_type==OU_SHUT_RX_NOTIFY);
    assert(decoder.completed_messages==2U && event_count==2U);
}
static void invalid(void) {
    const uint8_t payload[]={0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18};
    unsigned i;
    reset();assert(next(0x81)==-1); /* Never accept host->UPS commands. */
    assert(next(0x84)==0);assert(next(0x23)==-1);
    assert(next(0x84)==0);assert(next(0x11)==0);
    assert(next(0xaa)==0);assert(next(0xff)==-1);
    assert(!decoder.active && !decoder.assembled && !observer.reports_seen);
    assert(frame(0x04,payload,8U)==0);
    assert(next(0x85)==-1); /* No mid-message type mixing. */
    assert(frame(0x04,payload,8U)==0);
    assert(next(0x15)==0); /* NAK cancels partial message. */
    assert(!decoder.active&&!decoder.assembled);
    assert(frame(0x04,payload,8U)==0);
    assert(next(0x16)==0); /* SYNC cancels partial message. */
    assert(!decoder.active&&!decoder.assembled);
    for(i=0U;i<32U;i++)assert(frame(0x04,payload,8U)==0);
    assert(frame(0x84,payload,8U)==-1); /* 257th byte overflows. */
    assert(!decoder.active&&!decoder.assembled);
    assert(observer.reports_seen==0U);
}
static void clock_and_timeout(void) {
    const uint8_t payload[]={9,8,7};
    reset();assert(next(0x84)==0);
    tick+=OU_SHUT_RX_TIMEOUT_MS+1U;
    assert(ou_shut_rx_expire(&decoder,tick)==1);
    assert(decoder.timeouts==1U && !decoder.active);
    assert(frame(0x85,payload,3U)==1);
    tick=0U;assert(next(0x06)==-1); /* Clock regression rejected. */
    reset();tick=UINT32_MAX-3U;
    assert(frame(0x85,payload,3U)==1); /* Valid 32-bit clock wrap. */
}
static void fuzz(void) {
    uint32_t x=UINT32_C(0x12345678);unsigned i;
    reset();
    for(i=0U;i<20000U;i++){
        x=x*UINT32_C(1664525)+UINT32_C(1013904223);
        assert(ou_shut_rx_feed(&decoder,(uint8_t)(x>>24),tick++,0,&report)>=-1);
        assert(decoder.assembled<=OU_SHUT_RX_MESSAGE_CAP);
        assert(decoder.received<=OU_SHUT_RX_PACKET_DATA_MAX);
    }
}
int main(void){
    single_and_observation();multi_and_notify();invalid();clock_and_timeout();fuzz();
    puts("PASS: receive-only SHUT packets, XOR, fragments, notifications, observer, timeouts and fuzz");
    return 0;
}
