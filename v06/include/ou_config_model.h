#ifndef OU_CONFIG_MODEL_H
#define OU_CONFIG_MODEL_H
#include "ou_config_journal.h"
#include "ou_schema.h"

/* Independently owned v0.6 configuration; OEM catalog is metadata, not
 * authority to issue hardware writes. Only these explicitly modeled keys
 * can be staged. Others remain in the full 302-field inventory, but cannot
 * be applied accidentally. No passwords, control commands, or credentials. */
#define OU_CONFIG_FIELD_COUNT 9U
#define OU_CONFIG_VALUE_BYTES 64U
#define OU_CONFIG_RECORD_MAX 1024U
#define OU_CONFIG_RECORD_VERSION 1U

enum ou_config_key {
    OU_CONFIG_HOSTNAME=0,
    OU_CONFIG_DOMAIN,
    OU_CONFIG_DHCP,
    OU_CONFIG_IPV4,
    OU_CONFIG_NETMASK,
    OU_CONFIG_GATEWAY,
    OU_CONFIG_CONTACT,
    OU_CONFIG_LOCATION,
    OU_CONFIG_LANGUAGE
};
typedef struct {
    uint8_t present[OU_CONFIG_FIELD_COUNT];
    char values[OU_CONFIG_FIELD_COUNT][OU_CONFIG_VALUE_BYTES];
} ou_config_model;

/* Safe metadata enumeration, no hidden or secret field values. */
const char *ou_config_key_name(unsigned key);
const char *ou_config_key_type(unsigned key);
int ou_config_key_index(const char *name);

/* Strict single-field validation followed by all-or-nothing model update.
 * A draft may be incomplete, but a serialized/journaled model must pass
 * cross-field network validation. */
void ou_config_model_init(ou_config_model *model);
int ou_config_model_set(ou_config_model *model, const char *name,
                        const char *value);
int ou_config_model_clear(ou_config_model *model,const char *name);
int ou_config_model_validate(const ou_config_model *model);

/* Canonical versioned, architecture-independent byte format, sorted keys,
 * no credentials or raw flash addresses. Decoder never changes its target
 * on corruption, unknown keys, duplicates, or semantic errors. */
int ou_config_model_encode(const ou_config_model *model,
                           uint8_t *output,size_t capacity,size_t *written);
int ou_config_model_decode(ou_config_model *model,
                           const uint8_t *input,size_t length);

/* Explicit offline save/restore over a separately qualified two-slot
 * CONFIG partition. Both the external authentication callback and journal
 * safety gates must succeed. This callback is NOT an implementation of
 * HTTPS login or on-card authentication. Never called by HTTP/UI yet. */
typedef int (*ou_config_authorize_fn)(void *context);
int ou_config_model_save(const ou_config_model *model,
                         const ou_journal_io *dedicated_config,
                         ou_config_authorize_fn authorize,void *auth_context,
                         ou_journal_record *meta);
int ou_config_model_load(ou_config_model *model,
                         const ou_journal_io *dedicated_config,
                         ou_journal_record *meta);
#endif
