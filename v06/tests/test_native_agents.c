#include "ou_native_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct {unsigned sent;ou_agent_notice notice;} outbound;
static int send(void *context,const ou_agent_notice *notice){
    (void)context;
    ++outbound.sent;
    outbound.notice=*notice;
    return 1;
}
static ou_agent_sender adapter(void){
    ou_agent_sender a={0};
    a.send_notice=send;
    a.tls_peer_verified=1U;
    a.destination_identity_qualified=1U;
    a.network_qualified=1U;
    return a;
}
static void fetch(const char *path,uint32_t now_ms,int status,
                  const char *expect){
    char request[160],response[OU_HTTP_MAX_RESPONSE+1U];
    char prefix[20U];
    size_t length=0U;
    int n=snprintf(request,sizeof(request),"GET %s HTTP/1.0\r\n\r\n",path);
    assert(n>0&&(size_t)n<sizeof(request));
    assert(ou_native_service_http((const uint8_t *)request,(size_t)n,now_ms,
                                  response,sizeof(response)-1U,&length)==0);
    response[length]=0;
    (void)snprintf(prefix,sizeof(prefix),"HTTP/1.1 %d",status);
    assert(strstr(response,prefix)==response);
    assert(strstr(response,expect)!=0);
}
int main(void){
    ou_agent_sender sender=adapter();
    memset(&outbound,0,sizeof(outbound));
    ou_native_service_init();
    fetch("/api/v1/agents/capabilities",0U,200,"\"maximumClients\":12");
    fetch("/api/v1/agents/status",0U,200,"\"enrolled\":0");
    assert(ou_native_service_agent_enroll(5U,"Linux archive",60000U,0)==-1);
    assert(ou_native_service_agent_enroll(5U,"Linux archive",60000U,1)==0);
    fetch("/api/v1/agents/status",1000U,200,"\"online\":0");
    assert(ou_native_service_agent_heartbeat(5U,1U,1000U,0)==-1);
    assert(ou_native_service_agent_heartbeat(5U,1U,1000U,1)==0);
    fetch("/api/v1/agents/status",2000U,200,"\"online\":1");
    assert(ou_native_service_agent_warning(3000U,50000U,0,1)==-1);
    assert(ou_native_service_agent_warning(3000U,50000U,1,0)==-1);
    assert(ou_native_service_agent_warning(3000U,50000U,1,1)==0);
    fetch("/api/v1/agents/status",3001U,200,"\"warningActive\":true");
    sender.tls_peer_verified=0U;
    assert(ou_native_service_agent_notify(&sender,3100U)==-1);
    assert(outbound.sent==0U);
    sender.tls_peer_verified=1U;
    assert(ou_native_service_agent_notify(&sender,3100U)==1);
    assert(outbound.sent==1U);
    assert(outbound.notice.agent_id==5U && outbound.notice.transaction==1U);
    assert(ou_native_service_agent_ack(5U,1U,3200U,0)==-1);
    assert(ou_native_service_agent_ack(5U,1U,3200U,1)==0);
    assert(ou_native_service_agent_step(3300U)==1);
    fetch("/api/v1/agents/status",3400U,200,"\"acknowledged\":1");
    fetch("/api/v1/agents/status",3400U,200,"\"warningActive\":false");
    ou_native_service_init();
    fetch("/api/v1/agents/status",0U,200,"\"enrolled\":0");
    puts("PASS: native shutdown-agent status API, credential boundaries, warning/ACK cycle and reset");
    return 0;
}
