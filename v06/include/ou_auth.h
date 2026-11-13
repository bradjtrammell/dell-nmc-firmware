#ifndef OU_AUTH_H
#define OU_AUTH_H
#include <stddef.h>
#include <stdint.h>

/* Freestanding, fail-closed authentication/session core. No users,
 * passwords, entropy, TLS or time source is hardcoded or generated here.
 * A qualified H910P BSP MUST supply a real CSPRNG, password-KDF verifier,
 * monotonic time and authenticated TLS before this can serve operators. */
#define OU_AUTH_TOKEN_BYTES 32U
#define OU_AUTH_SESSIONS 4U
#define OU_AUTH_MAX_USERNAME 32U
#define OU_AUTH_MAX_PASSWORD 128U
#define OU_AUTH_IDLE_MS UINT32_C(900000)
#define OU_AUTH_ABSOLUTE_MS UINT32_C(28800000)
#define OU_AUTH_LOCKOUT_MS UINT32_C(60000)
#define OU_AUTH_MAX_FAILURES 5U

enum ou_auth_role {
    OU_AUTH_NONE=0,
    OU_AUTH_VIEWER=1,
    OU_AUTH_OPERATOR=2,
    OU_AUTH_ADMIN=3
};
enum ou_auth_permission {
    OU_AUTH_READ=1,
    OU_AUTH_CONFIG_WRITE=2,
    OU_AUTH_POWER_CONTROL=3,
    OU_AUTH_FIRMWARE_UPDATE=4,
    OU_AUTH_USER_ADMIN=5
};
typedef int (*ou_auth_random_fn)(void *context,uint8_t *out,size_t length);
typedef int (*ou_auth_password_verify_fn)(void *context,
                    const char *username,
                    const uint8_t *password,size_t password_len,
                    uint8_t *role_out);
typedef struct {
    ou_auth_random_fn csprng;
    ou_auth_password_verify_fn verify_password_kdf;
    void *context;
    /* BSP assertions, not HTTP inputs. All three MUST be positively
     * established before initializing. No default/test credentials. */
    uint8_t entropy_qualified;
    uint8_t credential_store_qualified;
    uint8_t tls_qualified;
} ou_auth_provider;
typedef struct {
    uint8_t token[OU_AUTH_TOKEN_BYTES];
    uint8_t csrf[OU_AUTH_TOKEN_BYTES];
    uint32_t issued_ms;
    uint32_t last_seen_ms;
    uint32_t request_counter;
    uint8_t role;
    uint8_t occupied;
} ou_auth_session;
typedef struct {
    ou_auth_session sessions[OU_AUTH_SESSIONS];
    ou_auth_provider provider;
    uint32_t failure_start_ms;
    uint32_t failures;
    uint8_t lockout_active;
    uint8_t initialized;
} ou_auth_state;
typedef struct {
    uint8_t token[OU_AUTH_TOKEN_BYTES];
    uint8_t csrf[OU_AUTH_TOKEN_BYTES];
    uint8_t role;
} ou_auth_grant;

void ou_auth_init(ou_auth_state *state,const ou_auth_provider *provider);
void ou_auth_revoke_all(ou_auth_state *state);

/* Encrypted transport MUST be asserted by the qualified TLS layer on
 * EVERY request. Returns 0 on success, -1 on wrong credentials, lockout,
 * no entropy, no free slots or unqualified provider. Caller must use
 * Secure/HttpOnly/SameSite session cookies, not URL query parameters. */
int ou_auth_login(ou_auth_state *state,const char *username,
                  const uint8_t *password,size_t password_len,
                  uint32_t now_ms,uint8_t encrypted_transport,
                  ou_auth_grant *grant);
/* Read needs role VIEWER+, config writes OPERATOR+, power ADMIN,
 * firmware update ADMIN, user administration ADMIN. All mutations need
 * CSRF and a STRICTLY increasing per-session request ID to bar replay.
 * Returns 1 only on successful authorization. */
int ou_auth_check(ou_auth_state *state,
                  const uint8_t token[OU_AUTH_TOKEN_BYTES],
                  const uint8_t csrf[OU_AUTH_TOKEN_BYTES],
                  uint32_t request_id,
                  enum ou_auth_permission permission,
                  uint32_t now_ms,uint8_t encrypted_transport);
int ou_auth_logout(ou_auth_state *state,
                   const uint8_t token[OU_AUTH_TOKEN_BYTES],
                   const uint8_t csrf[OU_AUTH_TOKEN_BYTES],
                   uint32_t request_id,uint32_t now_ms,
                   uint8_t encrypted_transport);
/* Use as ou_config_authorize_fn in the existing two-phase configuration
 * controller. Each call consumes one distinct, externally supplied request
 * counter. The authenticated TLS dispatcher must construct/update this
 * gate for EACH separate request; HTTP currently never creates one. */
typedef struct {
    ou_auth_state *state;
    const uint8_t *token;
    const uint8_t *csrf;
    uint32_t request_id;
    uint32_t now_ms;
    uint8_t encrypted_transport;
} ou_auth_config_gate;

int ou_auth_config_authorize(void *context);

int ou_auth_revoke_sessions(ou_auth_state *state,
                   const uint8_t admin_token[OU_AUTH_TOKEN_BYTES],
                   const uint8_t admin_csrf[OU_AUTH_TOKEN_BYTES],
                   uint32_t request_id,uint32_t now_ms,
                   uint8_t encrypted_transport);
#endif
