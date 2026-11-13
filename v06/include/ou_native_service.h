#ifndef OU_NATIVE_SERVICE_H
#define OU_NATIVE_SERVICE_H
#include <stddef.h>
#include <stdint.h>
#include "ou_provider.h"
#include "ou_http_wire.h"
#include "ou_event_archive.h"
#include "ou_notify.h"
#include "ou_snmp.h"
#include "ou_agents.h"
#include "ou_time_sync.h"
#include "ou_shutdown_policy.h"

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

/* Bind up to 8 notification rules through a separately authenticated
 * device-side caller. NEVER exposed as a write-capable HTTP endpoint. */
int ou_native_service_notify_configure(const ou_notify_rule *rules,
                      size_t count,int (*authorize)(void *),void *context);
/* Progress one pending outbound event using an explicitly qualified
 * delivery adapter. No sending occurs without all sender qualifications. */
int ou_native_service_notify_step(const ou_notify_sender *sender,
                                   uint32_t now_ms);

/* Qualified source ACL/community must be enforced by the future network
 * adapter; malformed/unauthorized packets never receive a response. */
int ou_native_service_snmp(const ou_snmp_access *access,
               const uint8_t *request,size_t request_len,uint32_t now_ms,
               uint8_t *response,size_t capacity,size_t *written);
/* Securely enrolled and mutually authenticated shutdown agents. The caller
 * must prove authorization and TLS identity separately from HTTP.
 * These functions NEVER issue a local UPS power shutdown. */
int ou_native_service_agent_enroll(uint32_t id,const char *label,
                                    uint32_t lease_ms,int admin_authorized);
int ou_native_service_agent_heartbeat(uint32_t id,uint32_t sequence,
                          uint32_t now_ms,int peer_identity_verified);
int ou_native_service_agent_warning(uint32_t now_ms,uint32_t warning_ms,
                          int operator_authorized,int verified_intent);
int ou_native_service_agent_notify(const ou_agent_sender *sender,uint32_t now_ms);
int ou_native_service_agent_ack(uint32_t id,uint32_t transaction,
                                uint32_t now_ms,int peer_identity_verified);
int ou_native_service_agent_step(uint32_t now_ms);
/* Trusted isolated-LAN NTP wire adapter. Real source/monotonic qualifications
 * must be established by BSP; this does NOT set a physical RTC or clock. */
int ou_native_service_time_bind(const ou_time_source *source);
int ou_native_service_time_request(uint32_t now_ms,uint8_t wire[OU_NTP_WIRE_BYTES]);
int ou_native_service_time_response(const uint8_t *wire,size_t length,
                            uint32_t now_ms,int source_address_verified);
/* Bind independently provisioned validated shutdown policy; admin and
 * separate physical config partition attestations are prerequisites.
 * Advisory only. NEVER a UPS power command or OS shutdown. */
int ou_native_service_shutdown_bind(const ou_shutdown_policy *policy,
                             int admin_authorized,int qualified_storage);
/* Fail-closed native data; never substitutes simulated UPS measurements. */
int ou_native_service_http(const uint8_t *req, size_t req_len, uint32_t now_ms,
                           char *response, size_t capacity, size_t *response_len);
#endif
