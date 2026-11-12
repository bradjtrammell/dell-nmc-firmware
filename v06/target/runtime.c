#include "ou_board_runtime.h"
#include "ou_native_service.h"

/* Native firmware reset continuation. The board-specific implementation
 * must fill every physical resource before a MAC/DMA register can be written.
 * Nothing here guesses a VLAN, PHY, RAM range, MAC address or clock. */
volatile unsigned ou_target_probe_state = 0U;
static ou_board_runtime ou_target_runtime;
static ou_board_boot_config ou_target_boot_config;
static char ou_target_http_diagnostic[OU_HTTP_MAX_RESPONSE];
static const uint8_t ou_health_request[] = "GET /api/v1/health HTTP/1.0\r\n\r\n";

/* H910P-specific link-time BSP entry. The weak fail-closed default must be
 * replaced only after its real hardware and recovery qualifications pass. */
__attribute__((weak))
int ou_h910p_port_prepare(ou_board_boot_config *config) {
    (void)config;
    return -1;
}

void ou_target_main(void) {
    size_t written = 0U;
    ou_native_service_init();
    ou_target_probe_state = 1U;
    if (ou_native_service_http(ou_health_request,
                               sizeof(ou_health_request) - 1U, 0U,
                               ou_target_http_diagnostic,
                               sizeof(ou_target_http_diagnostic),
                               &written) != 0 ||
        written < 12U ||
        ou_target_http_diagnostic[0] != 'H' ||
        ou_target_http_diagnostic[9] != '2' ||
        ou_target_http_diagnostic[10] != '0' ||
        ou_target_http_diagnostic[11] != '0') {
        ou_target_probe_state = 3U;
        goto halt;
    }
    ou_target_probe_state = 2U;
    if (ou_h910p_port_prepare(&ou_target_boot_config) != 0) {
        /* The weak adapter intentionally refuses to drive unknown hardware. */
        ou_target_probe_state = 4U;
        goto halt;
    }
    if (ou_board_runtime_start(&ou_target_runtime,&ou_target_boot_config) != 0) {
        ou_target_probe_state = 5U;
        goto halt;
    }
    ou_target_probe_state = 6U;
    while (ou_board_runtime_step(&ou_target_runtime) >= 0) {
        /* Verified single-threaded Ethernet/UPS event-loop slot. */
    }
    ou_target_probe_state = 7U;
halt:
    /* Only a recoverable hardware port may install a reset/watchdog path.
     * Never issue speculative flash writes or UPS power commands. */
    for (;;) { __asm__ volatile("nop"); }
}
