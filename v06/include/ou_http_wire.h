#ifndef OU_HTTP_WIRE_H
#define OU_HTTP_WIRE_H
#include <stddef.h>
#include <stdint.h>
#include "openh910p.h"
#define OU_HTTP_MAX_REQUEST 4096U
#define OU_HTTP_PREFIX_RESERVE 512U
#define OU_HTTP_MAX_RESPONSE (OU_HTTP_PREFIX_RESERVE + OU_JSON_CAPACITY)
/* One complete bounded request, one read-only HTTP response.
 * No NIC, sockets, heap, TLS, or POSIX dependencies. */
/* Native service may supply typed event data without modifying the
 * simulator or allowing unauthenticated mutations. */
/* Extended native reader: fixed-memory verified measurement history, even
 * when live UPS telemetry is unavailable. */
/* Extended native notification state and typed history, retaining
 * compatibility wrappers for existing read-only callers. */
int ou_http_respond_managed(const uint8_t *request, size_t size,
                    const ou_snapshot *snapshot, const ou_event_log *events,
                    const struct ou_history_log *history,
                    const struct ou_notify_engine *notifications,
                    const struct ou_agent_status *agents,
                    const struct ou_time_sample *clock,
                    char *output, size_t capacity, size_t *written);

int ou_http_respond_observed(const uint8_t *request, size_t size,
                    const ou_snapshot *snapshot, const ou_event_log *events,
                    const struct ou_history_log *history,
                    char *output, size_t capacity, size_t *written);

int ou_http_respond_events(const uint8_t *request, size_t size,
                    const ou_snapshot *snapshot, const ou_event_log *events,
                    char *output, size_t capacity, size_t *written);

int ou_http_respond(const uint8_t *request, size_t size,
                    const ou_snapshot *snapshot, char *output,
                    size_t capacity, size_t *written);
#endif
