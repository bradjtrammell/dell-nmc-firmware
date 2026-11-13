#ifndef OU_NATIVE_SERVICE_H
#define OU_NATIVE_SERVICE_H
#include <stddef.h>
#include <stdint.h>
#include "ou_provider.h"
#include "ou_http_wire.h"
#include "ou_event_archive.h"

/* Single-threaded, zero-heap native application service. The future verified
 * MAC/TCP adapter submits ONE complete bounded request at a time.
 * This interface alone is not a NIC, TCP/IP stack or network listener. */
void ou_native_service_init(void);

/* Explicit, wear-controlled event archive. Not automatically persisted on
 * each event. Requires an independently qualified dedicated EVENT partition,
 * and does NOT share the settings or bootloader sectors. Return -1 on
 * invalid/corrupt storage without changing the live event history. */
int ou_native_service_events_restore(const ou_journal_io *dedicated_events);
int ou_native_service_events_checkpoint(const ou_journal_io *dedicated_events,
                                         ou_journal_record *record);

/* Only a driver that has verified actual physical source identity, framing,
 * checksum and report authenticity may set report->verified. No UPS TX. */
int ou_native_service_driver_report(const ou_driver_report *report, uint32_t now_ms);

/* Called on a qualified UPS transport loss; preserve the replay watermark
 * while immediately withdrawing previously published telemetry. */
void ou_native_service_source_lost(void);
/* Precise monotonic event timestamp when transport driver has a clock. */
void ou_native_service_source_lost_at(uint32_t now_ms);

/* Fail-closed native data; never substitutes simulated UPS measurements. */
int ou_native_service_http(const uint8_t *req, size_t req_len, uint32_t now_ms,
                           char *response, size_t capacity, size_t *response_len);
#endif
