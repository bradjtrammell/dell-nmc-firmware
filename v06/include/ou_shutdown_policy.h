#ifndef OU_SHUTDOWN_POLICY_H
#define OU_SHUTDOWN_POLICY_H
#include <stddef.h>
#include <stdint.h>
#include "openh910p.h"
#include "ou_config_journal.h"

#define OU_SHUTDOWN_SEGMENTS 2U
#define OU_SHUTDOWN_RECORD_BYTES 64U
enum ou_shutdown_advisory_flags {
    OU_SHUTDOWN_UTILITY_LOST=1U,
    OU_SHUTDOWN_LOW_RUNTIME=2U,
    OU_SHUTDOWN_LOW_CAPACITY=4U,
    OU_SHUTDOWN_SEGMENT_2_LIMIT=8U,
    OU_SHUTDOWN_SEGMENT_3_LIMIT=16U,
    OU_SHUTDOWN_RESTART_CAPACITY_READY=32U
};
typedef struct {
    uint16_t duration_seconds;
    uint32_t shutdown_delay_seconds;
    uint32_t startup_delay_seconds;
    uint8_t capacity_limit_percent;
} ou_shutdown_segment;
typedef struct {
    uint16_t shutdown_duration_seconds;
    uint8_t shutdown_timer_selected;
    uint8_t forced_reboot;
    uint32_t shutdown_timer_seconds;
    uint32_t runtime_limit_seconds;
    uint8_t capacity_limit_percent;
    uint8_t restart_level_percent;
    ou_shutdown_segment segments[OU_SHUTDOWN_SEGMENTS];
} ou_shutdown_policy;
typedef struct ou_shutdown_advisory {
    uint32_t flags;
    uint8_t recommend_agent_warning;
    uint8_t power_commands_enabled;
} ou_shutdown_advisory;
typedef int (*ou_shutdown_authorize_fn)(void *context);
void ou_shutdown_policy_init(ou_shutdown_policy *policy);
int ou_shutdown_policy_validate(const ou_shutdown_policy *policy);
/* Advisory ONLY, from accepted native telemetry. No output toggles or
 * automatically initiated OS shutdowns. Returns -1 for untrusted data. */
int ou_shutdown_assess(const ou_shutdown_policy *policy,
                       const ou_snapshot *telemetry,ou_shutdown_advisory *out);
/* Canonical O6SD 64-byte record, all-or-nothing decode; dedicated journal
 * must be independently qualified, never bootloader/config/event sectors. */
int ou_shutdown_encode(const ou_shutdown_policy *policy,uint8_t *out,size_t len);
int ou_shutdown_decode(ou_shutdown_policy *policy,const uint8_t *bytes,size_t len);
int ou_shutdown_checkpoint(const ou_shutdown_policy *policy,
            const ou_journal_io *dedicated_shutdown,
            ou_shutdown_authorize_fn authorize,void *context,
            ou_journal_record *record);
int ou_shutdown_restore(ou_shutdown_policy *policy,
            const ou_journal_io *dedicated_shutdown,
            ou_journal_record *record);
#endif
