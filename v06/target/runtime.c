#include "ou_native_service.h"

/* Native single-threaded ARM7 application. This now exercises the same
 * freestanding HTTP/JSON service exposed to the future MAC/TCP adapter.
 * There is no verified H910P NIC, physical UPS transport or hardware timer.
 * The startup probe runs entirely in RAM. DO NOT FLASH THIS RESEARCH IMAGE. */
volatile unsigned ou_target_probe_state = 0U;
static char ou_target_http_diagnostic[OU_HTTP_MAX_RESPONSE];
static const uint8_t ou_health_request[] = "GET /api/v1/health HTTP/1.0\r\n\r\n";

void ou_target_main(void) {
    size_t written = 0U;
    ou_native_service_init();
    ou_target_probe_state = 1U;
    if (ou_native_service_http(ou_health_request,
                               sizeof(ou_health_request) - 1U, 0U,
                               ou_target_http_diagnostic,
                               sizeof(ou_target_http_diagnostic),
                               &written) == 0 &&
        written > 12U &&
        ou_target_http_diagnostic[0] == 'H' &&
        ou_target_http_diagnostic[9] == '2' &&
        ou_target_http_diagnostic[10] == '0' &&
        ou_target_http_diagnostic[11] == '0')
        ou_target_probe_state = 2U;
    else
        ou_target_probe_state = 3U;
    /* Hardware clocks/watchdog/MAC/UPS drivers are still not initialized.
     * No unsafe register writes or power commands. */
    for (;;) { __asm__ volatile("nop"); }
}
