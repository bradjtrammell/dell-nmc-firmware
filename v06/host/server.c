#define _POSIX_C_SOURCE 200809L
#include "openh910p.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define HTTP_MAX_REQUEST 8192U
#define HTTP_BACKLOG 16

static volatile sig_atomic_t shutting_down = 0;
static void on_signal(int sig) { (void)sig; shutting_down = 1; }

static const char *reason(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Content Too Large";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        default: return "Error";
    }
}

static int send_full(int fd, const void *data, size_t size) {
    const unsigned char *cursor = (const unsigned char *)data;
    while (size) {
        ssize_t written = send(fd, cursor, size, MSG_NOSIGNAL);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return -1;
        cursor += (size_t)written;
        size -= (size_t)written;
    }
    return 0;
}

static void respond(int fd, int status, const char *mime,
                    const void *body, size_t length, int head) {
    char header[1152];
    int count = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "Referrer-Policy: no-referrer\r\n"
        "X-Frame-Options: DENY\r\n"
        "Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'\r\n"
        "X-OpenH910P-Mode: simulator\r\n"
        "\r\n", status, reason(status), mime, length);
    if (count <= 0 || (size_t)count >= sizeof(header)) return;
    if (send_full(fd, header, (size_t)count) != 0) return;
    if (!head && body && length) (void)send_full(fd, body, length);
}

static void error_reply(int fd, int code, const char *message, int head) {
    char body[256];
    int length = snprintf(body, sizeof(body),
        "{\"schemaVersion\":1,\"source\":\"simulator\",\"error\":\"%s\"}", message);
    if (length < 0 || (size_t)length >= sizeof(body)) return;
    respond(fd, code, "application/json; charset=utf-8", body, (size_t)length, head);
}

static int parse_line(char *request, char *method, size_t method_size,
                      char *path, size_t path_size) {
    char version[16];
    char *end = strstr(request, "\r\n");
    if (!end) return -1;
    *end = '\0';
    if (sscanf(request, "%7s %511s %15s", method, path, version) != 3) return -1;
    if (strlen(method) >= method_size || strlen(path) >= path_size) return -1;
    if (strcmp(version, "HTTP/1.1") && strcmp(version, "HTTP/1.0")) return -1;
    if (path[0] != '/' || strchr(path, '#') || strchr(path, '?') || strchr(path, '%')) return -1;
    return 0;
}

static void serve_client(int fd) {
    char request[HTTP_MAX_REQUEST + 1U];
    char method[8] = "", path[512] = "";
    size_t used = 0;
    int complete = 0;
    struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    while (used < HTTP_MAX_REQUEST) {
        ssize_t count = recv(fd, request + used, HTTP_MAX_REQUEST - used, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        used += (size_t)count;
        request[used] = '\0';
        if (strstr(request, "\r\n\r\n")) { complete = 1; break; }
    }
    if (!complete) {
        error_reply(fd, used >= HTTP_MAX_REQUEST ? 431 : 400,
                    used >= HTTP_MAX_REQUEST ? "HEADER_LIMIT" : "BAD_REQUEST", 0);
        return;
    }
    char *header_end = strstr(request, "\r\n\r\n");
    if (!header_end) { error_reply(fd, 400, "BAD_REQUEST", 0); return; }
    size_t available_body = used - (size_t)((header_end + 4) - request);
    if (memchr(request, 0, used) != NULL) {
        error_reply(fd, 400, "BAD_REQUEST", 0); return;
    }
    if (parse_line(request, method, sizeof(method), path, sizeof(path)) != 0) {
        error_reply(fd, 400, "BAD_REQUEST", 0); return;
    }
    int head = strcmp(method, "HEAD") == 0;
    int get = strcmp(method, "GET") == 0;
    /* Reject ambiguous framing. Drain only a small explicit body so the peer
     * receives the complete 405 response rather than a TCP reset. */
    size_t body_length = 0;
    int found_length = 0;
    for (char *line = request + strlen(request) + 2; line < header_end; ) {
        char *end = strstr(line, "\r\n");
        if (!end || end > header_end) break;
        size_t line_length = (size_t)(end - line);
        if (line_length >= 16 && strncasecmp(line, "Content-Length:", 15) == 0) {
            if (found_length++) { error_reply(fd, 400, "BAD_REQUEST", 0); return; }
            char digits[24];
            size_t k = 15, idx = 0;
            while (k < line_length && line[k] == ' ') k++;
            while (k < line_length && idx < sizeof(digits) - 1 && line[k] >= '0' && line[k] <= '9')
                digits[idx++] = line[k++];
            if (idx == 0 || k != line_length) { error_reply(fd, 400, "BAD_REQUEST", 0); return; }
            digits[idx] = 0;
            body_length = (size_t)strtoul(digits, NULL, 10);
        }
        if (line_length >= 18 && strncasecmp(line, "Transfer-Encoding:", 18) == 0) {
            error_reply(fd, 400, "BAD_REQUEST", 0); return;
        }
        line = end + 2;
    }
    if (body_length > 4096U || available_body > 4096U) {
        error_reply(fd, 413, "CONTENT_LIMIT", 0); return;
    }
    if (body_length && (get || head)) { error_reply(fd, 400, "BAD_REQUEST", head); return; }
    if (body_length && !get && !head) {
        while (available_body < body_length) {
            char discard[1024];
            size_t remaining = body_length - available_body;
            ssize_t count = recv(fd, discard, remaining < sizeof(discard) ? remaining : sizeof(discard), 0);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) { error_reply(fd, 400, "BAD_REQUEST", 0); return; }
            available_body += (size_t)count;
        }
    }
    if (!get && !head) {
        error_reply(fd, 405, "READ_ONLY", 0); return;
    }
    if (strncmp(path, "/api/v1/", 8) == 0) {
        char body[OU_JSON_CAPACITY];
        ou_reply reply;
        ou_snapshot state = ou_simulated_snapshot();
        if (ou_dispatch(method, path, &state, body, sizeof(body), &reply) != 0) {
            error_reply(fd, 500, "SERIALIZATION_ERROR", head); return;
        }
        respond(fd, reply.status, reply.content_type, body, reply.body_length, head);
        return;
    }
    error_reply(fd, 404, "NOT_FOUND", head);
}

int main(int argc, char **argv) {
    unsigned long port = 8760UL;
    if (argc == 3 && strcmp(argv[1], "--port") == 0) {
        char *tail = NULL;
        errno = 0;
        port = strtoul(argv[2], &tail, 10);
        if (errno || !tail || *tail || port == 0UL || port > 65535UL) {
            fprintf(stderr, "Invalid port. Expected integer from 1 to 65535.\n");
            return 2;
        }
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [--port 8760]\n", argv[0]);
        return 2;
    }
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    (void)sigaction(SIGINT, &action, NULL);
    (void)sigaction(SIGTERM, &action, NULL);
    signal(SIGPIPE, SIG_IGN);
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) { perror("socket"); return 1; }
    int reuse = 1;
    (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons((uint16_t)port);
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, HTTP_BACKLOG) != 0) {
        perror("bind/listen"); close(listener); return 1;
    }
    printf("OpenH910P v0.6 native C development simulator\n");
    printf("http://127.0.0.1:%lu/api/v1/health — JSON only, simulated, read-only\n", port);
    fflush(stdout);
    while (!shutting_down) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) { if (errno == EINTR) continue; perror("accept"); break; }
        serve_client(client);
        close(client);
    }
    close(listener);
    return 0;
}
