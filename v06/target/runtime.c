#include "openh910p.h"

/* A host-testable JSON core does not make a hardware firmware. This entry
 * intentionally attempts no network initialization, UPS bus I/O, flash
 * access, memory-controller writes, watchdog changes, or power commands.
 * It exists to prove independent ARM link/startup shape offline only. */
volatile unsigned ou_target_probe_state = 0U;
static char ou_target_diagnostic_json[OU_JSON_CAPACITY];

void ou_target_main(void) {
    ou_snapshot snapshot = ou_unavailable_snapshot();
    ou_reply reply;
    ou_target_probe_state = 1U;
    if (ou_dispatch("GET", "/api/v1/health", &snapshot,
                    ou_target_diagnostic_json,
                    sizeof(ou_target_diagnostic_json), &reply) == 0 &&
        reply.status == 200)
        ou_target_probe_state = 2U;
    else
        ou_target_probe_state = 3U;
    /* No verified MMIO UART, clock tree or hardware watchdog yet. */
    for (;;) {
        __asm__ volatile("nop");
    }
}
