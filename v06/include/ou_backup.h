#ifndef OU_BACKUP_H
#define OU_BACKUP_H

#include "ou_config_model.h"
#include "ou_shutdown_policy.h"
#include "ou_weekly_schedule.h"
#include "ou_update_verify.h"

/* Portable NON-SECRET configuration bundle (not installable firmware).
 * Contains only modeled network/system settings, shutdown policy and
 * weekly schedule. It never contains credentials, private keys, account
 * records, agent identities, or arbitrary OEM configuration fields. */
#define OU_BACKUP_HEADER_BYTES 48U
#define OU_BACKUP_MAX_BYTES 1536U
#define OU_BACKUP_VERSION 1U

typedef struct {
    ou_config_model configuration;
    ou_shutdown_policy shutdown;
    ou_weekly_schedule weekly;
} ou_backup_snapshot;

typedef int (*ou_backup_permission_fn)(void *context);

/* Export requires an authenticated caller's positive permission. This
 * callback is NOT an authentication implementation. SHA-256 is accidental
 * corruption detection, NOT a signature or authorizing credential. */
int ou_backup_export(const ou_backup_snapshot *snapshot,
                     ou_backup_permission_fn authorize,void *context,
                     uint8_t *output,size_t capacity,size_t *written);

/* Parse, hash-check and validate ALL sections before replacing the caller's
 * snapshot. NO flash writes, network reconfiguration or UPS power actions.
 * Restore requires a future multi-partition transaction coordinator. */
int ou_backup_preview(ou_backup_snapshot *destination,
                      const uint8_t *input,size_t length);

#endif
