#ifndef OU_TIME_SYNC_H
#define OU_TIME_SYNC_H
#include <stddef.h>
#include <stdint.h>

/* Bounded NTPv4 bootstrap. Trusted LOCAL source only until authenticated
 * NTS or equivalent can be qualified. Never writes RTC or system clock. */
#define OU_NTP_WIRE_BYTES 48U
#define OU_NTP_MAX_RTT_MS UINT32_C(5000)
#define OU_NTP_MAX_AGE_MS UINT32_C(3600000)
#define OU_NTP_UNIX_OFFSET UINT64_C(2208988800)
typedef int (*ou_time_nonce_fn)(void *context,uint8_t out[8]);
typedef struct {
    ou_time_nonce_fn secure_nonce;
    void *context;
    uint8_t source_acl_qualified;
    uint8_t isolated_trusted_network;
    uint8_t monotonic_clock_qualified;
    uint8_t explicit_era; /* 0 or 1, anchored by independent device policy */
    uint8_t era_anchored;
} ou_time_source;
typedef struct ou_time_sample {
    uint64_t utc_unix_seconds;
    uint32_t utc_millisecond;
    uint32_t round_trip_ms;
    uint32_t uncertainty_ms;
    uint32_t synchronized_ms;
    uint8_t source_verified;
} ou_time_sample;
typedef struct {
    ou_time_source source;
    ou_time_sample last;
    uint8_t nonce[8];
    uint32_t request_ms;
    uint8_t outstanding;
    uint8_t synchronized;
} ou_time_sync;
/* Source qualification is a BSP assertion, not an untrusted web flag. */
int ou_time_init(ou_time_sync *sync,const ou_time_source *source);
/* Random 64-bit transmit nonce, no default/seeded PRNG or invented clock.
 * Only one request may be outstanding. */
int ou_time_make_request(ou_time_sync *sync,uint32_t now_ms,
                          uint8_t request[OU_NTP_WIRE_BYTES]);
/* Validate server source ACL externally; originate nonce, stratum, leap,
 * mode, RTT and trust bounds internally. accepted=0; refused=-1. */
int ou_time_accept(ou_time_sync *sync,const uint8_t *reply,size_t length,
                   uint32_t now_ms,int source_address_verified);
/* Estimate UTC using qualified monotonic duration only. Expires after 1h.
 * Requires a previously accepted reply; never silently guesses zone/DST. */
int ou_time_now(const ou_time_sync *sync,uint32_t now_ms,
                ou_time_sample *estimated);
#endif
