#include "ou_native_service.h"
#include "ou_telemetry_guard.h"

static ou_telemetry_guard source;

void ou_native_service_init(void) {
    ou_telemetry_guard_reset(&source);
}

int ou_native_service_driver_report(const ou_driver_report *report, uint32_t now_ms) {
    return ou_telemetry_guard_accept(&source, report, now_ms);
}

int ou_native_service_http(const uint8_t *req, size_t req_len, uint32_t now_ms,
                           char *response, size_t capacity, size_t *response_len) {
    ou_snapshot sample = ou_unavailable_snapshot();
    (void)ou_telemetry_guard_read(&source, now_ms, &sample);
    return ou_http_respond(req, req_len, &sample, response, capacity, response_len);
}
