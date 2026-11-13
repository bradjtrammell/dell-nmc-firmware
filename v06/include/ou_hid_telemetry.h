#ifndef OU_HID_TELEMETRY_H
#define OU_HID_TELEMETRY_H
#include "ou_hid_report.h"
#include "ou_shut_rx.h"
#include "ou_provider.h"

/* Separate physical attestations MUST come from a qualified on-card driver,
 * never from successful parsing, XML, or these caller-provided flags alone.
 * No physical H910P adapter currently supplies this evidence. */
typedef struct {
    uint8_t installed_ups_identity_verified;
    uint8_t electrical_link_verified;
    uint8_t transaction_origin_verified;
    uint8_t descriptor_origin_verified;
} ou_hid_source_evidence;

#define OU_HID_TELEMETRY_METRICS 9U
#define OU_HID_TELEMETRY_FULL_MASK UINT16_C(0x01ff)
#define OU_HID_TELEMETRY_SKEW_MS UINT32_C(4000)

enum ou_hid_metric {
    OU_HID_BATTERY_PERCENT = 0, OU_HID_LOAD_PERCENT = 1,
    OU_HID_RUNTIME_SECONDS = 2, OU_HID_INPUT_DECIVOLTS = 3,
    OU_HID_OUTPUT_DECIVOLTS = 4, OU_HID_AC_PRESENT = 5,
    OU_HID_BATTERY_PRESENT = 6, OU_HID_SEGMENT_1_ON = 7,
    OU_HID_SEGMENT_2_ON = 8
};

typedef struct {
    uint16_t usage_page;
    uint16_t usage;
    uint8_t report_id;
    uint8_t multiply; /* Explicit 1..100 calibration. Never guessed. */
    uint8_t divide;   /* Exact division only, no hidden rounding. */
} ou_hid_metric_binding;

typedef struct {
    ou_hid_feature_map map;
    ou_hid_metric_binding binding[OU_HID_TELEMETRY_METRICS];
    uint32_t metrics[OU_HID_TELEMETRY_METRICS];
    uint32_t sampled_ms[OU_HID_TELEMETRY_METRICS];
    uint32_t last_report_ms;
    uint32_t sequence;
    uint16_t seen;
    uint8_t initialized;
    uint8_t received;
} ou_hid_telemetry;

/* Bind nine distinct, descriptor-verified Feature usages. Invalid evidence,
 * duplicate/missing fields, and unknown scaling refuse initialization. */
int ou_hid_telemetry_init(ou_hid_telemetry *state,
                          const uint8_t *descriptor, size_t descriptor_size,
                          const ou_hid_metric_binding bindings[OU_HID_TELEMETRY_METRICS],
                          const ou_hid_source_evidence *evidence);
/* Ingest one completed SHUT RESPONSE from an independently verified physical
 * session. All fields in that frame are committed atomically. Failure clears
 * staged values but retains the monotonic replay watermark. */
int ou_hid_telemetry_ingest(ou_hid_telemetry *state,
                            const ou_shut_rx_message *message, uint32_t now_ms);
/* Only a complete, fresh and coherent nine-field bundle can be published.
 * On success, clear all samples to prevent replay; the caller must still
 * submit to the existing stateful telemetry guard. */
int ou_hid_telemetry_take(ou_hid_telemetry *state, uint32_t now_ms,
                           ou_driver_report *output);
/* Revoke both data and source attestations on any physical link failure. */
void ou_hid_telemetry_revoke(ou_hid_telemetry *state);
#endif
