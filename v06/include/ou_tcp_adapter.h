#ifndef OU_TCP_ADAPTER_H
#define OU_TCP_ADAPTER_H
#include "ou_tcp_engine.h"
/* Adapter from VLAN TCP session engine to native read-only HTTP service.
 * Must never be enabled on physical hardware without secure authorization. */
int ou_tcp_native_http(void *context,const uint8_t *request,size_t size,
                       uint32_t now_ms,char *out,size_t capacity,
                       size_t *written);
#endif
