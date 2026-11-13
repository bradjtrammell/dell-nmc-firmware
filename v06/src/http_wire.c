#include "ou_http_wire.h"

typedef struct { char *buf; size_t used, cap; int error; } writer;
static void putc_(writer *w, char c) {
    if (w->error) return;
    if (w->used >= w->cap) { w->error = 1; return; }
    w->buf[w->used++] = c;
}
static void puts_(writer *w, const char *s) {
    while (*s) putc_(w, *s++);
}
static void putdec(writer *w, size_t n) {
    char tmp[24]; size_t i = 0;
    do { tmp[i++] = (char)('0'+n%10U); n /= 10U; } while (n && i < sizeof(tmp));
    while (i) putc_(w, tmp[--i]);
}
static int equals(const uint8_t *bytes, size_t length, const char *literal) {
    size_t i = 0;
    for (; i < length && literal[i]; ++i) if (bytes[i] != (uint8_t)literal[i]) return 0;
    return i == length && literal[i] == '\0';
}
static int eq_ci(const uint8_t *bytes, size_t length, const char *literal) {
    size_t i = 0;
    for (; i < length && literal[i]; ++i) {
        uint8_t c = bytes[i]; uint8_t d = (uint8_t)literal[i];
        if (c >= 'A' && c <= 'Z') c = (uint8_t)(c + 32U);
        if (d >= 'A' && d <= 'Z') d = (uint8_t)(d + 32U);
        if (c != d) return 0;
    }
    return i == length && literal[i] == '\0';
}
static const char *reason(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 503: return "Service Unavailable";
        default: return "Error";
    }
}
static const char *errjson(int status) {
    switch (status) {
        case 400: return "{\"schemaVersion\":1,\"error\":\"BAD_REQUEST\"}";
        case 404: return "{\"schemaVersion\":1,\"error\":\"NOT_FOUND\"}";
        case 405: return "{\"schemaVersion\":1,\"error\":\"READ_ONLY\"}";
        case 431: return "{\"schemaVersion\":1,\"error\":\"HEADER_LIMIT\"}";
        default: return "{\"schemaVersion\":1,\"error\":\"INTERNAL_ERROR\"}";
    }
}
static size_t cstr_size(const char *text) {
    size_t i = 0; while (text[i]) ++i; return i;
}
static int parse_request(const uint8_t *r, size_t size,
                         char method[9], char path[193], int *head) {
    size_t at = 0, end = 0, a, b, p, header_count = 0;
    int http11, host_seen = 0, content_length_seen = 0;
    size_t length = 0;
    if (!r || size == 0 || size > OU_HTTP_MAX_REQUEST) return 431;
    for (a = 0; a < size; ++a)
        if (r[a] == 0 || r[a] == 0x7f || (r[a] < 0x20 && r[a] != '\r' && r[a] != '\n' && r[a] != '\t')) return 400;
    while (end + 1U < size && !(r[end] == '\r' && r[end+1] == '\n')) ++end;
    if (end + 1U >= size) return 400;
    a = 0;
    while (a < end && r[a] != ' ') ++a;
    if (a == 0 || a > 8 || a >= end) return 400;
    for (b = 0; b < a; ++b) {
        if (r[b] < 'A' || r[b] > 'Z') return 400;
        method[b] = (char)r[b];
    }
    method[a] = '\0';
    *head = equals(r, a, "HEAD");
    p = a + 1U; a = p;
    while (p < end && r[p] != ' ') ++p;
    if (p == a || p-a > 192 || p == end || r[a] != '/') return 400;
    for (b = 0; b < p-a; ++b) {
        uint8_t c = r[a+b];
        if (c < 0x21 || c > 0x7e || c == '?' || c == '#' || c == '%') return 400;
        path[b] = (char)c;
    }
    path[p-a] = '\0';
    a = p + 1U;
    if (equals(r+a, end-a, "HTTP/1.1")) http11 = 1;
    else if (equals(r+a, end-a, "HTTP/1.0")) http11 = 0;
    else return 400;
    at = end + 2U;
    while (at + 1U < size) {
        if (r[at] == '\r' && r[at+1] == '\n') {
            if (at + 2U != size) return 400;
            if (http11 && !host_seen) return 400;
            if (content_length_seen && length != 0 &&
                (equals((const uint8_t *)method,cstr_size(method),"GET") || *head)) return 400;
            return 0;
        }
        if (++header_count > 32U) return 431;
        end = at;
        while (end + 1U < size && !(r[end] == '\r' && r[end+1] == '\n')) ++end;
        if (end + 1U >= size || end == at || r[at] == ' ' || r[at] == '\t') return 400;
        a = at;
        while (a < end && r[a] != ':') {
            if (!((r[a]>='A' && r[a]<='Z') || (r[a]>='a' && r[a]<='z') ||
                  (r[a]>='0' && r[a]<='9') || r[a]=='-')) return 400;
            ++a;
        }
        if (a == at || a == end) return 400;
        for (b = a+1U; b < end; ++b)
            if (r[b] == '\r' || r[b] == '\n' || r[b] == 0 || (r[b] < 0x20 && r[b] != '\t')) return 400;
        b = a+1U;
        while (b < end && (r[b] == ' ' || r[b] == '\t')) ++b;
        if (eq_ci(r+at,a-at,"host")) {
            if (host_seen++ || b == end) return 400;
        } else if (eq_ci(r+at,a-at,"transfer-encoding")) return 400;
        else if (eq_ci(r+at,a-at,"content-length")) {
            if (content_length_seen++ || b == end) return 400;
            length = 0;
            for (; b < end; ++b) {
                if (r[b] < '0' || r[b] > '9') return 400;
                length = length * 10U + (size_t)(r[b] - '0');
                if (length > OU_HTTP_MAX_REQUEST) return 400;
            }
        }
        at = end + 2U;
    }
    return 400;
}
int ou_http_respond_managed(const uint8_t *request, size_t size,
                    const ou_snapshot *snapshot, const ou_event_log *events,
                    const struct ou_history_log *history,
                    const struct ou_notify_engine *notifications,
                    const struct ou_agent_status *agents,
                    const struct ou_time_sample *clock,
                    char *output, size_t capacity, size_t *written) {
    char method[9] = {0}, path[193] = {0};
    writer w;
    int status, head = 0, read_only, code;
    size_t bodylen = 0, i, hdrlen;
    const char *error = 0;
    ou_reply reply;
    if (!output || !written || !snapshot || !request ||
        capacity < OU_HTTP_MAX_RESPONSE) return -1;
    *written = 0;
    code = parse_request(request,size,method,path,&head);
    status = code;
    if (code) error = errjson(code);
    else {
        read_only = !(equals((const uint8_t *)method,cstr_size(method),"GET") || head);
        if (read_only) {
            status = 405;
            error = errjson(405);
        } else if (path[0] != '/' || path[1] != 'a' || path[2] != 'p' ||
                   path[3] != 'i' || path[4] != '/' || path[5] != 'v' ||
                   path[6] != '1' || path[7] != '/') {
            status = 404;
            error = errjson(404);
        } else if (ou_dispatch_managed(method,path,snapshot,events,history,notifications,agents,clock,output+OU_HTTP_PREFIX_RESERVE,
                               capacity-OU_HTTP_PREFIX_RESERVE,&reply) != 0) {
            status = 500;
            error = errjson(500);
        } else {
            status = reply.status;
            bodylen = reply.body_length;
        }
    }
    if (error) {
        bodylen = cstr_size(error);
        if (bodylen > capacity-OU_HTTP_PREFIX_RESERVE) return -1;
        for (i=0;i<bodylen;++i) output[OU_HTTP_PREFIX_RESERVE+i]=error[i];
    }
    w=(writer){output,0,OU_HTTP_PREFIX_RESERVE,0};
    puts_(&w,"HTTP/1.1 ");putdec(&w,(size_t)status);putc_(&w,' ');
    puts_(&w,reason(status));puts_(&w,"\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: ");
    putdec(&w,bodylen);
    puts_(&w,"\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nX-Frame-Options: DENY\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'none'; frame-ancestors 'none'\r\nX-OpenH910P-Mode: ");
    puts_(&w,snapshot->origin == OU_ORIGIN_SIMULATOR?"simulator":"native-research");
    puts_(&w,"\r\n\r\n");
    if (w.error) return -1;
    hdrlen = w.used;
    if (head) { *written=hdrlen; return 0; }
    if (hdrlen+bodylen > capacity) return -1;
    for (i=0;i<bodylen;++i) output[hdrlen+i]=output[OU_HTTP_PREFIX_RESERVE+i];
    *written=hdrlen+bodylen;
    return 0;
}

int ou_http_respond_observed(const uint8_t *request, size_t size,
                    const ou_snapshot *snapshot, const ou_event_log *events,
                    const struct ou_history_log *history,
                    char *output, size_t capacity, size_t *written) {
    return ou_http_respond_managed(request,size,snapshot,events,history,0,0,0,
                                   output,capacity,written);
}

int ou_http_respond_events(const uint8_t *request, size_t size,
                    const ou_snapshot *snapshot, const ou_event_log *events,
                    char *output, size_t capacity, size_t *written) {
    return ou_http_respond_observed(request,size,snapshot,events,0,
                                    output,capacity,written);
}

int ou_http_respond(const uint8_t *request, size_t size,
                    const ou_snapshot *snapshot, char *output,
                    size_t capacity, size_t *written) {
    return ou_http_respond_events(request,size,snapshot,0,output,capacity,written);
}
