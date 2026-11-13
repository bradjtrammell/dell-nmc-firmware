#include "ou_shut_session.h"

static int valid_time(ou_shut_session *s, uint32_t now) {
    if (s->started && (uint32_t)(now-s->last_ms)>UINT32_C(0x7fffffff))
        return 0;
    s->last_ms=now;
    s->started=1U;
    return 1;
}
int ou_shut_session_abort(ou_shut_session *s, uint8_t error) {
    uint8_t *dst;
    size_t i;
    if(!s || error==OU_SHUT_SESSION_ERR_NONE || error>OU_SHUT_SESSION_ERR_IO)
        return -1;
    s->state=OU_SHUT_SESSION_FAILED;
    s->out_length=0U;
    s->after_ack=0U;
    s->error=error;
    dst=(uint8_t *)&s->result;
    for(i=0U;i<sizeof(s->result);++i)dst[i]=0U;
    for(i=0U;i<sizeof(s->out);++i)s->out[i]=0U;
    ou_shut_rx_init(&s->rx);
    if(s->failures!=UINT32_MAX)++s->failures;
    return -1;
}
static int fail(ou_shut_session *s, uint8_t error) {
    return ou_shut_session_abort(s,error);
}
static int queue_sync(ou_shut_session *s) {
    s->out[0]=0x16U;
    s->out_length=1U;
    s->queued_ms=s->last_ms;
    s->state=OU_SHUT_SESSION_SYNC_READY;
    return 1;
}
static int queue_request(ou_shut_session *s) {
    size_t n=0U;
    if(ou_shut_build_get_report_sized(s->report_id,s->requested_bytes,
          s->out,sizeof(s->out),&n)!=0 || n!=OU_SHUT_GET_REPORT_FRAME_SIZE)
        return fail(s,OU_SHUT_SESSION_ERR_BAD_PACKET);
    s->out_length=(uint8_t)n;
    s->queued_ms=s->last_ms;
    s->state=OU_SHUT_SESSION_REQUEST_READY;
    return 1;
}
void ou_shut_session_init(ou_shut_session *s) {
    if(s)*s=(ou_shut_session){0};
}
int ou_shut_session_begin(ou_shut_session *s,uint8_t report_id,
                          uint16_t requested_bytes,uint32_t now_ms,
                          uint8_t physical_link_verified,
                          uint8_t protocol_verified) {
    if(!s || (s->state!=OU_SHUT_SESSION_IDLE &&
              s->state!=OU_SHUT_SESSION_COMPLETE &&
              s->state!=OU_SHUT_SESSION_FAILED) ||
       physical_link_verified!=1U || protocol_verified!=1U ||
       requested_bytes>OU_SHUT_GET_REPORT_MAX_LENGTH)return -1;
    ou_shut_session_init(s);
    ou_shut_rx_init(&s->rx);
    ou_link_init(&s->observer);
    s->report_id=report_id;
    s->requested_bytes=requested_bytes;
    s->last_ms=now_ms;
    s->started=1U;
    return queue_sync(s);
}
int ou_shut_session_take_tx(ou_shut_session *s,uint32_t now_ms,
                            uint8_t *frame,size_t capacity,size_t *written) {
    size_t i;
    uint8_t next;
    if(!written)return -1;
    *written=0U;
    if(!s || !frame)return -1;
    if(!valid_time(s,now_ms))return fail(s,OU_SHUT_SESSION_ERR_CLOCK);
    if(s->out_length==0U)return 0;
    if(capacity<s->out_length)return -1;
    for(i=0U;i<s->out_length;++i)frame[i]=s->out[i];
    *written=s->out_length;
    next=s->state;
    s->out_length=0U;
    s->sent_ms=now_ms;
    if(next==OU_SHUT_SESSION_SYNC_READY) {
        ++s->sync_attempts;
        s->state=OU_SHUT_SESSION_SYNC_WAIT;
    } else if(next==OU_SHUT_SESSION_REQUEST_READY) {
        ++s->request_attempts;
        s->state=OU_SHUT_SESSION_REQUEST_WAIT;
    } else if(next==OU_SHUT_SESSION_ACK_READY) {
        s->state=s->after_ack;
        s->after_ack=0U;
        if(s->state==OU_SHUT_SESSION_COMPLETE &&
           s->successful_reads!=UINT32_MAX)++s->successful_reads;
    } else return fail(s,OU_SHUT_SESSION_ERR_BAD_PACKET);
    return 1;
}
int ou_shut_session_feed(ou_shut_session *s,uint8_t byte,uint32_t now_ms) {
    ou_shut_rx_message message;
    uint32_t packets;
    int r;
    if(!s)return -1;
    if(!valid_time(s,now_ms))return fail(s,OU_SHUT_SESSION_ERR_CLOCK);
    if(s->state==OU_SHUT_SESSION_SYNC_WAIT) {
        if(byte==0x16U)return queue_request(s);
        return 0;
    }
    if(s->state==OU_SHUT_SESSION_REQUEST_WAIT) {
        if(byte==0x06U) {
            s->state=OU_SHUT_SESSION_RESPONSE_WAIT;
            s->sent_ms=now_ms;
            return 0;
        }
        if(byte==0x15U) {
            if(s->request_attempts>=OU_SHUT_SESSION_REQUEST_ATTEMPTS)
                return fail(s,OU_SHUT_SESSION_ERR_NAK);
            return queue_request(s);
        }
        return 0;
    }
    if(s->state!=OU_SHUT_SESSION_RESPONSE_WAIT)return -1;
    packets=s->rx.accepted_packets;
    r=ou_shut_rx_feed(&s->rx,byte,now_ms,&s->observer,&message);
    if(r<0)return fail(s,OU_SHUT_SESSION_ERR_BAD_PACKET);
    if(s->rx.accepted_packets!=packets) {
        /* Per MGE section 2.4, ACK each checksum-verified received packet.
         * Do not ACK malformed data. Incomplete fragments require ACK too. */
        if(r==1 && message.packet_type==OU_SHUT_RX_RESPONSE) {
            if(message.length<1U || message.payload[0]!=s->report_id)
                return fail(s,OU_SHUT_SESSION_ERR_REPORT_MISMATCH);
            s->result=message;
            s->after_ack=OU_SHUT_SESSION_COMPLETE;
        } else s->after_ack=OU_SHUT_SESSION_RESPONSE_WAIT;
        s->out[0]=0x06U;
        s->out_length=1U;
        s->queued_ms=now_ms;
        s->state=OU_SHUT_SESSION_ACK_READY;
        s->sent_ms=now_ms;
        return r==1?1:0;
    }
    return 0;
}
int ou_shut_session_tick(ou_shut_session *s,uint32_t now_ms) {
    uint32_t elapsed;
    if(!s)return -1;
    if(!valid_time(s,now_ms))return fail(s,OU_SHUT_SESSION_ERR_CLOCK);
    elapsed=(uint32_t)(now_ms-s->sent_ms);
    switch(s->state) {
    case OU_SHUT_SESSION_SYNC_WAIT:
        if(elapsed>=OU_SHUT_SESSION_SYNC_INTERVAL_MS) {
            if(s->sync_attempts>=OU_SHUT_SESSION_ATTEMPTS)
                return fail(s,OU_SHUT_SESSION_ERR_TIMEOUT);
            return queue_sync(s);
        }
        return 0;
    case OU_SHUT_SESSION_REQUEST_WAIT:
        if(elapsed>=OU_SHUT_SESSION_ACK_TIMEOUT_MS) {
            if(s->request_attempts>=OU_SHUT_SESSION_REQUEST_ATTEMPTS)
                return fail(s,OU_SHUT_SESSION_ERR_TIMEOUT);
            return queue_request(s);
        }
        return 0;
    case OU_SHUT_SESSION_RESPONSE_WAIT:
        if(ou_shut_rx_expire(&s->rx,now_ms)>0)
            return fail(s,OU_SHUT_SESSION_ERR_TIMEOUT);
        if(elapsed>=OU_SHUT_SESSION_RESPONSE_TIMEOUT_MS)
            return fail(s,OU_SHUT_SESSION_ERR_TIMEOUT);
        return 0;
    case OU_SHUT_SESSION_SYNC_READY:
    case OU_SHUT_SESSION_REQUEST_READY:
    case OU_SHUT_SESSION_ACK_READY:
        /* A board driver that cannot drain a pending outbound frame must
         * abort instead of silently dropping protocol acknowledgements. */
        if((uint32_t)(now_ms-s->queued_ms)>OU_SHUT_SESSION_ACK_TIMEOUT_MS)
            return fail(s,OU_SHUT_SESSION_ERR_TIMEOUT);
        return 0;
    case OU_SHUT_SESSION_IDLE:
    case OU_SHUT_SESSION_COMPLETE:
    case OU_SHUT_SESSION_FAILED: return 0;
    default: return fail(s,OU_SHUT_SESSION_ERR_BAD_PACKET);
    }
}
int ou_shut_session_take_report(ou_shut_session *s,ou_shut_rx_message *out) {
    if(!s || !out)return -1;
    if(s->state!=OU_SHUT_SESSION_COMPLETE)return 0;
    *out=s->result;
    s->result=(ou_shut_rx_message){0};
    s->state=OU_SHUT_SESSION_IDLE;
    return 1;
}
