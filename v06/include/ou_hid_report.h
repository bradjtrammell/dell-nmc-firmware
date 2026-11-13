#ifndef OU_HID_REPORT_H
#define OU_HID_REPORT_H
#include <stddef.h>
#include <stdint.h>

/* Bounded, receive-only HID Feature descriptor interpreter for UPS data.
 * No USB stack, no UART, no report writes, and NO automatic telemetry trust.
 * The descriptor must be independently sourced from the actual UPS. */
#define OU_HID_DESCRIPTOR_CAP 2048U
#define OU_HID_FIELDS_CAP 64U
#define OU_HID_REPORT_BITS_CAP 2048U
#define OU_HID_VALUES_CAP 64U

typedef struct {
    uint16_t usage_page;
    uint16_t usage;
    uint16_t bit_offset;
    uint8_t report_id;
    uint8_t bit_size;
    uint8_t signed_value;
    int32_t logical_min;
    int32_t logical_max;
} ou_hid_feature_field;

typedef struct {
    ou_hid_feature_field fields[OU_HID_FIELDS_CAP];
    uint16_t feature_bits[256];
    uint8_t has_report_ids;
    uint8_t field_count;
} ou_hid_feature_map;

typedef struct {
    uint16_t usage_page;
    uint16_t usage;
    int32_t value;
    uint8_t report_id;
    uint8_t telemetry_verified; /* Always zero until physical identity proof. */
} ou_hid_feature_value;

/* 0 success; -1 malformed, unsupported or oversized report descriptor.
 * Only Power Device (0x84) and Battery System (0x85) variable Feature
 * usages are emitted, but ALL Feature fields advance their bit offsets.
 * This is a deliberately strict subset of HID 1.11, not a full HID parser. */
int ou_hid_feature_parse(const uint8_t *descriptor, size_t length,
                         ou_hid_feature_map *map);

/* Decode one complete report including its ID byte if descriptor declares
 * IDs. No hidden units/scaling, power actions, or device authenticity claim.
 * 0 success, -1 malformed/out-of-range or insufficient output capacity. */
int ou_hid_feature_decode(const ou_hid_feature_map *map,
                          const uint8_t *report, size_t length,
                          ou_hid_feature_value *values, size_t capacity,
                          size_t *written);
#endif
