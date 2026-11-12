#include "ou_http_wire.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void check(const char *request,int expected,const char *token) {
    char result[OU_HTTP_MAX_RESPONSE + 1U],status[24];
    size_t size=0;
    ou_snapshot sample = ou_simulated_snapshot();
    assert(ou_http_respond((const uint8_t *)request, strlen(request), &sample,
                           result, sizeof(result)-1, &size)==0);
    assert(size<sizeof(result));
    result[size]='\0';
    snprintf(status,sizeof(status),"HTTP/1.1 %d",expected);
    assert(strstr(result,status)==result);
    if (token) assert(strstr(result,token)!=NULL);
}
int main(void) {
    char result[OU_HTTP_MAX_RESPONSE+1U];size_t size=100;
    ou_snapshot sample=ou_simulated_snapshot();
    const char *req="GET /api/v1/health HTTP/1.1\r\nHost: local\r\n\r\n";
    check(req,200,"\"source\":\"simulator\"");
    check("GET /api/v1/ups/status HTTP/1.0\r\n\r\n",200,"\"batteryPercent\":97");
    check("HEAD /api/v1/device HTTP/1.1\r\nHost: a\r\n\r\n",200,"Content-Length: ");
    check("GET /index.htm HTTP/1.1\r\nHost: a\r\n\r\n",404,"NOT_FOUND");
    check("POST /api/v1/ups/outlets HTTP/1.1\r\nHost: a\r\nContent-Length: 0\r\n\r\n",405,"READ_ONLY");
    check("GET /api/v1/health HTTP/1.1\r\n\r\n",400,"BAD_REQUEST");
    check("GET /api/v1/health HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n",400,"BAD_REQUEST");
    check("GET /api/v1/health HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n",400,"BAD_REQUEST");
    check("GET /api/v1/health HTTP/1.1\r\nHost: a\r\nContent-Length: 10\r\n\r\n",400,"BAD_REQUEST");
    check("GET /api/v1/health HTTP/1.1\r\nHost: a\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n",400,"BAD_REQUEST");
    check("GET /api/v1/health?raw HTTP/1.1\r\nHost: a\r\n\r\n",400,"BAD_REQUEST");
    check("GET /api/v1/health HTTP/1.1\r\n Host: a\r\n\r\n",400,"BAD_REQUEST");
    assert(ou_http_respond((const uint8_t*)req,strlen(req),&sample,result,20,&size)==-1);
    sample=ou_unavailable_snapshot();
    assert(ou_http_respond((const uint8_t*)req,strlen(req),&sample,result,sizeof(result)-1,&size)==0);
    result[size]='\0';
    assert(strstr(result,"\"source\":\"unavailable\"")!=NULL);
    puts("PASS: native freestanding HTTP wire framing and read-only enforcement");
    return 0;
}
