#ifndef OU_CONFIG_ACTIVATION_H
#define OU_CONFIG_ACTIVATION_H
#include "ou_config_model.h"

/* Two-phase network update, independent of the actual H910P BSP.
 * No power, flash, Ethernet, or HTTPS operations are implemented here.
 * A qualified physical adapter supplies apply/restore/probe callbacks. */
#define OU_CONFIG_CONFIRM_MIN_MS UINT32_C(10000)
#define OU_CONFIG_CONFIRM_MAX_MS UINT32_C(180000)
enum ou_config_activation_state {
    OU_CONFIG_ACT_IDLE=0,
    OU_CONFIG_ACT_PENDING=1,
    OU_CONFIG_ACT_CONFIRMED=2,
    OU_CONFIG_ACT_ROLLED_BACK=3,
    OU_CONFIG_ACT_FAILED_ROLLBACK=4
};
enum ou_config_activation_error {
    OU_CONFIG_ACT_ERR_NONE=0,
    OU_CONFIG_ACT_ERR_INVALID=1,
    OU_CONFIG_ACT_ERR_UNAUTHORIZED=2,
    OU_CONFIG_ACT_ERR_STALE=3,
    OU_CONFIG_ACT_ERR_APPLY=4,
    OU_CONFIG_ACT_ERR_TIMEOUT=5,
    OU_CONFIG_ACT_ERR_CONNECTIVITY=6,
    OU_CONFIG_ACT_ERR_STORAGE=7,
    OU_CONFIG_ACT_ERR_RESTORE=8
};
typedef int (*ou_config_apply_fn)(void *context,const ou_config_model *settings);
typedef int (*ou_config_probe_fn)(void *context);
/* Every callback must be backed by a separately qualified device-side
 * implementation. A fake returning 1 is not physical qualification. */
typedef struct {
    ou_config_apply_fn apply;
    ou_config_apply_fn restore;
    ou_config_probe_fn probe;
    ou_config_authorize_fn authorize;
    void *context;
    uint8_t physical_network_verified;
    uint8_t rollback_channel_verified;
    uint8_t authenticated_session_verified;
} ou_config_activation_adapter;
typedef struct {
    ou_config_model active;
    ou_config_model candidate;
    ou_journal_record active_record;
    const ou_journal_io *config_partition;
    ou_config_activation_adapter adapter;
    uint32_t started_ms;
    uint32_t timeout_ms;
    uint32_t confirmed_changes;
    uint32_t rolled_back_changes;
    uint8_t state;
    uint8_t last_error;
} ou_config_activation;

/* Load last fully committed settings, refusing an invalid/no baseline.
 * This does not activate the saved network profile on hardware. */
int ou_config_activation_init(ou_config_activation *owner,
                              const ou_journal_io *dedicated_config,
                              const ou_config_activation_adapter *adapter);
/* Before any physical change: verify operator permission, baseline journal
 * generation (anti-stale), prior reachability and fully valid target.
 * Requires qualified rollback path and protected storage. */
int ou_config_activation_begin(ou_config_activation *owner,
                               const ou_config_model *candidate,
                               uint32_t now_ms,uint32_t timeout_ms);
/* Bounded cooperative progress; 1=timed rollback, 0=no change, -1=fatal.
 * A transient probe=0 waits for remote operator reconnect/confirmation.
 * probe<0 is hard failure and triggers immediate rollback. */
int ou_config_activation_step(ou_config_activation *owner,uint32_t now_ms);
/* Only an authenticated operator with proven new-link reachability can
 * confirm. Persist to the inactive journal slot AFTER reachability proof;
 * on commit failure restore old network settings. */
int ou_config_activation_confirm(ou_config_activation *owner,uint32_t now_ms);
/* Explicitly abandon a pending activation, restoring the old profile.
 * No write to flash or changes to the saved baseline. */
int ou_config_activation_cancel(ou_config_activation *owner);
#endif
