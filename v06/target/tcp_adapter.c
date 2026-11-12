#include "ou_native_service.h"
#include "ou_tcp_adapter.h"

/* The existing native JSON service remains read-only; it will not accept
 * control/firmware write methods through the TCP transport. An outer
 * authenticated and encrypted management plane is still required. */
int ou_tcp_native_http(void *context,const uint8_t *request,size_t size,
                       uint32_t now_ms,char *out,size_t capacity,
                       size_t *written) {
    (void)context;
    return ou_native_service_http(request,size,now_ms,out,capacity,written);
}
