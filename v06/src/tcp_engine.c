#include "ou_tcp_engine.h"

#define SYN 0x02U
#define FIN 0x01U
#define RST 0x04U
#define PSH 0x08U
#define ACK 0x10U
#define ST_SYN 1U
#define ST_RECEIVE 2U
#define ST_RESPONSE 3U
#define ST_FIN 4U

static uint16_t r16(const uint8_t *p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static void w16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void w32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void cp(uint8_t *d,const uint8_t*s,size_t n){size_t i;for(i=0;i<n;++i)d[i]=s[i];}
static void clear(void *p,size_t n){volatile uint8_t *q=p;while(n--)*q++=0U;}
static int eq(const uint8_t*a,const uint8_t*b,size_t n){size_t i;for(i=0;i<n;++i)if(a[i]!=b[i])return 0;return 1;}
static uint32_t add(uint32_t v,const uint8_t*p,size_t n){size_t i;for(i=0;i+1U<n;i+=2U)v+=r16(p+i);if(i<n)v+=(uint32_t)p[i]<<8;while(v>>16)v=(v&65535U)+(v>>16);return v;}
static uint16_t check(uint32_t v){while(v>>16)v=(v&65535U)+(v>>16);return (uint16_t)~v;}
static void discard(ou_tcp_slot *s){clear(s,sizeof(*s));}
int ou_tcp_engine_init(ou_tcp_engine *server,uint32_t boot_seed){
 if(!server||!boot_seed)return -1;
 clear(server,sizeof(*server));server->next_isn=boot_seed;server->next_ip_id=(uint16_t)boot_seed;return 0;
}
static int match(const ou_tcp_slot*s,const ou_tcp_ingress*in){
 return s->state&&s->vid==in->vlan.vlan_id&&s->tagged==in->vlan.tagged&&
 s->remote_port==in->source_port&&eq(s->remote_ip,in->source_ipv4,4U)&&
 eq(s->local_ip,in->destination_ipv4,4U)&&eq(s->remote_mac,in->source_mac,6U);
}
static ou_tcp_slot *locate(ou_tcp_engine *e,const ou_tcp_ingress*in){size_t i;for(i=0;i<OU_TCP_SLOTS;i++)if(match(&e->slots[i],in))return &e->slots[i];return 0;}
static ou_tcp_slot *available(ou_tcp_engine *e){size_t i;for(i=0;i<OU_TCP_SLOTS;i++)if(!e->slots[i].state)return &e->slots[i];return 0;}
static int encode(ou_tcp_engine*e,const ou_vlan_table*vlans,ou_tcp_slot*s,
                  uint8_t flags,const uint8_t*payload,size_t length,
                  uint32_t now_ms,uint8_t*out,size_t cap,size_t*written){
 size_t offset=(s->tagged?18U:14U),n=offset+40U+length;
 size_t frame=n<(s->tagged?64U:60U)?(s->tagged?64U:60U):n;
 uint8_t *ip,*tcp;uint16_t c;uint32_t pseudo;size_t i;int idx=-1;
 *written=0U;
 if(length>OU_TCP_SEGMENT_MSS||n>OU_NET_MAX_FRAME||frame>cap||
    (length&&!payload))return -1;
 for(i=0;i<vlans->count;i++)if(vlans->interfaces[i].vid==s->vid){idx=(int)i;break;}
 if(idx<0)return -1;
 for(i=0;i<frame;i++)out[i]=0U;
 cp(out,s->remote_mac,6U);cp(out+6U,vlans->mac,6U);
 if(s->tagged){
  uint16_t vid=s->priority_tag?0U:s->vid;
  uint16_t tci=(uint16_t)(((uint16_t)vlans->interfaces[idx].tx_pcp<<13)|vid);
  w16(out+12U,OU_VLAN_TPID);w16(out+14U,tci);w16(out+16U,UINT16_C(0x0800));
 }else w16(out+12U,UINT16_C(0x0800));
 ip=out+offset;tcp=ip+20U;ip[0]=0x45U;w16(ip+2U,(uint16_t)(40U+length));
 w16(ip+4U,e->next_ip_id++);ip[8U]=64U;ip[9U]=6U;
 cp(ip+12U,s->local_ip,4U);cp(ip+16U,s->remote_ip,4U);
 c=check(add(0U,ip,20U));w16(ip+10U,c);
 w16(tcp,OU_TCP_HTTP_PORT);w16(tcp+2U,s->remote_port);
 w32(tcp+4U,s->send_next);w32(tcp+8U,s->receive_next);
 tcp[12U]=0x50U;tcp[13U]=flags;
 w16(tcp+14U,(uint16_t)(OU_TCP_REQUEST_CAP-s->request_len));
 if(length)cp(tcp+20U,payload,length);
 pseudo=add(0U,ip+12U,8U)+6U+(uint32_t)(20U+length);
 c=check(add(pseudo,tcp,20U+length));w16(tcp+16U,c);
 *written=frame;
 if((flags&(SYN|FIN))||length){
  if(s->outstanding)return -1;
  s->send_unacked=s->send_next;
  s->send_next+=(uint32_t)length+((flags&SYN)?1U:0U)+((flags&FIN)?1U:0U);
  s->outstanding=1U;s->retries=0U;s->sent_at=now_ms;s->last_len=(uint16_t)frame;
  cp(s->last_packet,out,frame);
 }
 return 1;
}
static int send_chunk(ou_tcp_engine*e,const ou_vlan_table*t,ou_tcp_slot*s,
                      uint32_t now_ms,uint8_t*out,size_t cap,size_t*written){
 size_t len=s->response_len-s->response_offset;
 int ret;
 if(len){
  if(s->peer_window==0U)return 0;
  if(len>OU_TCP_SEGMENT_MSS)len=OU_TCP_SEGMENT_MSS;
  if(len>s->peer_window)len=s->peer_window;
  ret=encode(e,t,s,ACK|PSH,(const uint8_t*)s->response+s->response_offset,
             len,now_ms,out,cap,written);
  if(ret==1)s->response_offset+=len;
 }else{
  s->state=ST_FIN;
  ret=encode(e,t,s,ACK|FIN,0,0U,now_ms,out,cap,written);
 }
 return ret;
}
static int header_end(const uint8_t*data,size_t len){
 size_t i;if(len<4U)return 0;
 for(i=3U;i<len;i++)if(data[i-3U]=='\r'&&data[i-2U]=='\n'&&
   data[i-1U]=='\r'&&data[i]=='\n')return 1;
 return 0;
}
int ou_tcp_engine_receive(ou_tcp_engine*e,ou_vlan_table*t,const uint8_t*frame,
                          size_t len,uint32_t now_ms,ou_tcp_app app,void*ctx,
                          uint8_t*out,size_t cap,size_t*written){
 ou_tcp_ingress in;ou_tcp_slot*s;int v;size_t n=0U;
 if(!written)return -1;
 *written=0U;
 if(!e||!t||!frame||!out||!app||cap<OU_NET_MAX_FRAME)return -1;
 v=ou_tcp_ingress_check(t,frame,len,&in);if(v!=1)return v<0?-1:0;
 s=locate(e,&in);
 if(!s){
  if((in.flags&(SYN|ACK|FIN|RST))!=SYN||in.payload_length)return 0;
  s=available(e);if(!s)return 0;
  discard(s);s->state=ST_SYN;s->vid=in.vlan.vlan_id;
  s->tagged=in.vlan.tagged;s->priority_tag=(uint8_t)(in.vlan.tagged&&
     (r16(frame+14U)&UINT16_C(0x0fff))==0U);
  s->remote_port=in.source_port;s->peer_window=in.advertised_window;
  cp(s->remote_mac,in.source_mac,6U);cp(s->remote_ip,in.source_ipv4,4U);
  cp(s->local_ip,in.destination_ipv4,4U);
  s->receive_next=in.sequence+1U;
  e->next_isn+=UINT32_C(0x9e3779b9);
  s->send_next=e->next_isn^(in.sequence<<11)^(in.sequence>>21);
  s->last_activity=now_ms;
  return encode(e,t,s,SYN|ACK,0,0U,now_ms,out,cap,written);
 }
 if(in.flags&RST){if(in.sequence==s->receive_next)discard(s);return 0;}
 if(in.sequence!=s->receive_next){
  return encode(e,t,s,ACK,0,0U,now_ms,out,cap,written);
 }
 if(in.flags&SYN)return 0;
 if((in.flags&ACK)==0U)return 0;
 s->last_activity=now_ms;s->peer_window=in.advertised_window;
 if(s->outstanding){
  if(in.acknowledgment==s->send_next){s->outstanding=0U;s->last_len=0U;}
  else if(in.acknowledgment!=s->send_unacked)return 0;
 }else if(in.acknowledgment!=s->send_next)return 0;
 if(s->state==ST_SYN){
  if(s->outstanding||in.sequence!=s->receive_next)return 0;
  s->state=ST_RECEIVE;
 }
 if(s->state==ST_FIN){
  if(!s->outstanding){
   if(in.flags&FIN){
    int sent;
    s->receive_next++;
    sent=encode(e,t,s,ACK,0,0U,now_ms,out,cap,written);
    discard(s);
    return sent;
   }
   discard(s);return 0;
  }
  return 0;
 }
 if(s->state==ST_RESPONSE){
  if(!s->outstanding)return send_chunk(e,t,s,now_ms,out,cap,written);
  return 0;
 }
 if(in.payload_length){
  if(s->request_len+in.payload_length>OU_TCP_REQUEST_CAP){discard(s);return 0;}
  cp(s->request+s->request_len,in.payload,in.payload_length);
  s->request_len+=in.payload_length;s->receive_next+=(uint32_t)in.payload_length;
 }
 if(in.flags&FIN){
  s->receive_next++;
  if(!s->request_len){discard(s);return 0;}
 }
 if(!in.payload_length && !(in.flags&FIN) && s->request_len==0U)return 0;
 if(!header_end(s->request,s->request_len)){
  return encode(e,t,s,ACK,0,0U,now_ms,out,cap,written);
 }
 if(app(ctx,s->request,s->request_len,now_ms,s->response,
        OU_TCP_RESPONSE_CAP,&n)!=0||!n||n>OU_TCP_RESPONSE_CAP){
  discard(s);return 0;
 }
 s->response_len=n;s->response_offset=0U;s->state=ST_RESPONSE;
 return send_chunk(e,t,s,now_ms,out,cap,written);
}
int ou_tcp_engine_tick(ou_tcp_engine*e,uint32_t now_ms,
                       uint8_t*out,size_t cap,size_t*written){
 size_t i;
 if(!written)return -1;
 *written=0U;
 if(!e||!out||cap<OU_NET_MAX_FRAME)return -1;
 for(i=0U;i<OU_TCP_SLOTS;i++){
  ou_tcp_slot*s=&e->slots[i];
  if(!s->state)continue;
  if((uint32_t)(now_ms-s->last_activity)>OU_TCP_IDLE_MS){discard(s);continue;}
  if(!s->outstanding||((uint32_t)(now_ms-s->sent_at)<OU_TCP_RETRY_MS))continue;
  if(s->retries>=OU_TCP_MAX_RETRIES){discard(s);continue;}
  if(s->last_len>cap)return -1;
  cp(out,s->last_packet,s->last_len);*written=s->last_len;
  s->sent_at=now_ms;s->retries++;
  return 1;
 }
 return 0;
}
