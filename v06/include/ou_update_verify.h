#ifndef OU_UPDATE_VERIFY_H
#define OU_UPDATE_VERIFY_H
#include <stddef.h>
#include <stdint.h>

/* The independent project repository is the ONLY update authority.
 * No OEM update channel, no Dell BIN fallbacks. */
#define OU_UPDATE_REPOSITORY "bradjtrammell/dell-nmc-firmware"
#define OU_UPDATE_HARDWARE_ID UINT16_C(0x9109)
#define OU_UPDATE_SIGNED_FORMAT_VERSION 1U
#define OU_UPDATE_SIGNATURE_BYTES 64U
#define OU_UPDATE_KEY_ID_BYTES 8U
#define OU_UPDATE_SHA256_BYTES 32U
#define OU_UPDATE_MESSAGE_MAX 128U
#define OU_UPDATE_MIN_IMAGE 4096U

typedef struct {
    uint32_t h[8];
    uint8_t block[64];
    uint32_t total_bytes;
    uint8_t used;
} ou_sha256_ctx;
void ou_sha256_init(ou_sha256_ctx *ctx);
int ou_sha256_update(ou_sha256_ctx *ctx,const uint8_t *bytes,size_t size);
int ou_sha256_final(ou_sha256_ctx *ctx,uint8_t digest[32]);

typedef struct {
    uint16_t hardware_id; /* must match independently verified H910P */
    uint16_t major,minor,patch;
    uint32_t image_bytes;
    uint8_t sha256[32];
    uint8_t key_id[8];
    uint8_t signature[64];
    uint8_t format_version;
    uint8_t prerelease;
    char repository[sizeof(OU_UPDATE_REPOSITORY)];
} ou_update_manifest;
typedef int (*ou_update_signature_verify_fn)(
    void *ctx,const uint8_t *message,size_t message_len,
    const uint8_t signature[OU_UPDATE_SIGNATURE_BYTES],
    const uint8_t key_id[OU_UPDATE_KEY_ID_BYTES]);
typedef struct {
    void *ctx;
    ou_update_signature_verify_fn verify_signature;
    uint16_t installed_major,installed_minor,installed_patch;
    uint32_t verified_application_capacity;
    uint8_t board_verified;
    uint8_t flash_layout_verified;
    uint8_t boot_recovery_verified;
    uint8_t trusted_github_tls_verified;
    uint8_t signing_keys_verified;
} ou_update_trust;
typedef struct {
    ou_sha256_ctx sha;
    ou_update_manifest manifest;
    uint32_t received;
    uint8_t verified_manifest;
    uint8_t image_complete;
} ou_update_verifier;

/* Canonical signed message includes pinned repository identity, exact H910P
 * target, version, release channel, image length and SHA256 digest.
 * Noncanonical/oversized release data is rejected BEFORE signature check. */
int ou_update_canonical(const ou_update_manifest *m,
                        uint8_t *output,size_t size,size_t *written);
/* No flash writes in these functions. A true result means the supplied
 * qualified external Ed25519 implementation authenticated the manifest. */
int ou_update_begin(ou_update_verifier *out,const ou_update_manifest *m,
                    const ou_update_trust *trust);
/* Bounded streaming SHA256 of image bytes. Appends never access flash. */
int ou_update_feed(ou_update_verifier *v,const uint8_t *image,size_t count);
/* Returns 0 only if verified manifest plus exact image digest/length pass.
 * Does NOT authorize activation/flash/reboot or signal a safe release. */
int ou_update_finish(ou_update_verifier *v);
#endif
