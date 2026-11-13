#ifndef OU_UPS_MONITOR_H
#define OU_UPS_MONITOR_H
#include "ou_shut_uart_link.h"
#include "ou_hid_telemetry.h"
#include "ou_native_service.h"

/* Cooperative native UPS monitoring: qualified NS7520 UART -> SHUT
 * feature reads -> descriptor-defined HID -> native JSON status.
 * Read-only transactions; physical H910P interface not yet qualified. */
#define OU_UPS_MONITOR_MAX_REPORTS OU_HID_TELEMETRY_METRICS
#define OU_UPS_MONITOR_PERIOD_MS UINT32_C(1000)

typedef struct {
    ou_shut_uart_link link;
    ou_hid_telemetry hid;
    uint8_t report_id[OU_UPS_MONITOR_MAX_REPORTS];
    uint16_t report_length[OU_UPS_MONITOR_MAX_REPORTS];
    uint8_t report_count;
    uint8_t report_index;
    uint8_t cycle_active;
    uint8_t initialized;
    uint8_t clock_started;
    uint8_t source_lifetime_valid;
    uint32_t last_ms;
    uint32_t last_publish_ms;
    /* Non-repeating lifetime sequence across revoked/requalified link. */
    uint32_t sequence_watermark;
    uint32_t published_sets;
    uint32_t failures;
} ou_ups_monitor;

/* The descriptor/bindings and electrical/source qualifications must be
 * supplied by a real independently verified board-specific port. A test
 * fixture may assert them, but the running H910P does not yet do so.
 * This function does not probe hardware or turn on a UART. */
int ou_ups_monitor_bind(ou_ups_monitor *monitor,
                        const ou_ns7520_serial_rx *physical_rx,
                        ou_shut_uart_write8 tx_byte,
                        ou_shut_uart_write32 tx_status,
                        void *tx_context,
                        uint8_t tx_pin_verified,
                        uint8_t baud_verified,
                        uint8_t shut_protocol_verified,
                        uint32_t measured_baud,
                        const uint8_t *hid_descriptor,
                        size_t hid_descriptor_length,
                        const ou_hid_metric_binding bindings[OU_HID_TELEMETRY_METRICS],
                        const ou_hid_source_evidence *source_evidence);

/* One bounded event-loop step, starting a three-or-more report cycle
 * automatically. Lengths are derived from parsed descriptor report bits,
 * never guessed. Return 1 on transport progress or publication,
 * 0 when idle/waiting, -1 on revoked hardware or bad telemetry. */
int ou_ups_monitor_step(ou_ups_monitor *monitor, uint32_t now_ms);

/* Link loss immediately withdraws native telemetry without resetting its
 * replay history. Source identity and descriptor must be requalified. */
void ou_ups_monitor_revoke(ou_ups_monitor *monitor);
#endif
