#ifndef OU_NOTIFY_POLICY_H
#define OU_NOTIFY_POLICY_H
#include "ou_notify.h"
#include "ou_config_journal.h"

/* Canonical, versioned, non-secret routing policy only. Destination slots
 * refer to credentials stored separately; no URLs, passwords, PII or
 * transport certificates are copied into this record. */
#define OU_NOTIFY_POLICY_BYTES 104U
#define OU_NOTIFY_POLICY_VERSION 1U
typedef int (*ou_notify_policy_authorize_fn)(void *context);

/* Exactly 104 bytes: 8-byte O6NR header and eight ordered 12-byte rule
 * records. Rule count is explicit, unused records must be zero, and
 * decoder cannot partially change the caller's engine. */
int ou_notify_policy_encode(const ou_notify_engine *engine,
                            uint8_t *out,size_t capacity);
int ou_notify_policy_decode(ou_notify_engine *engine,
                            const uint8_t *data,size_t length);
/* Standalone explicit storage operations; caller must prove authorization
 * and a separately qualified NOTIFICATION journal partition. No automatic
 * flash writes, and no live device partition is configured yet. */
int ou_notify_policy_checkpoint(const ou_notify_engine *engine,
                        const ou_journal_io *dedicated_notifications,
                        ou_notify_policy_authorize_fn authorize,
                        void *context,ou_journal_record *record);
int ou_notify_policy_restore(ou_notify_engine *engine,
                        const ou_journal_io *dedicated_notifications,
                        ou_journal_record *record);
#endif
