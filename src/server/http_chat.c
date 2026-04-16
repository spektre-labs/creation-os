/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "http_chat.h"
#include "json_esc.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
int cos_http_chat_serve(unsigned short port,
                        int (*complete)(const char *prompt, char *out, size_t cap, void *ctx), void *ctx)
{
    (void)port;
    (void)complete;
    (void)ctx;
    fprintf(stderr, "http_chat: POSIX-only in this tree (Windows stub).\n");
    return -1;
}
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

static int extract_last_user_content(const char *body, char *out, size_t cap)
{
    const char *last = NULL;
    for (const char *p = strstr(body, "\"role\":\"user\""); p != NULL; p = strstr(p + 1, "\"role\":\"user\""))
        last = p;
    if (!last)
        return -1;
    const char *c = strstr(last, "\"content\":\"");
    if (!c)
        return -1;
    c += strlen("\"content\":\"");
    size_t w = 0;
    for (; *c && *c != '"'; c++) {
        if (*c == '\\' && c[1]) {
            c++;
            if (w + 1 >= cap)
                return -1;
            out[w++] = *c;
            continue;
        }
        if (w + 1 >= cap)
            return -1;
        out[w++] = *c;
    }
    out[w] = '\0';
    return (int)w;
}

static int read_http_body(int fd, char *buf, size_t cap, size_t content_len)
{
    if (content_len + 1u > cap)
        return -1;
    size_t got = 0;
    while (got < content_len) {
        ssize_t n = read(fd, buf + got, content_len - got);
        if (n <= 0)
            return -1;
        got += (size_t)n;
    }
    buf[got] = '\0';
    return 0;
}

static int handle_one(int fd, int (*complete)(const char *prompt, char *out, size_t cap, void *ctx), void *ctx)
{
    char hdr[8192];
    size_t hp = 0;
    while (hp + 4 < sizeof(hdr)) {
        ssize_t n = read(fd, hdr + hp, 1);
        if (n != 1)
            return -1;
        hp++;
        if (hp >= 4 && memcmp(hdr + hp - 4, "\r\n\r\n", 4) == 0)
            break;
    }
    hdr[hp] = '\0';

    if (strncmp(hdr, "GET /health", 11) == 0) {
        const char *resp = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nok\n";
        (void)write(fd, resp, strlen(resp));
        return 0;
    }

    if (!strstr(hdr, "POST /v1/chat/completions"))
        return -1;

    size_t content_len = 0;
    const char *cl = strstr(hdr, "Content-Length:");
    if (cl) {
        cl += strlen("Content-Length:");
        while (*cl == ' ' || *cl == '\t')
            cl++;
        content_len = (size_t)strtoull(cl, NULL, 10);
    }
    if (content_len == 0 || content_len > (sizeof(hdr) - 1))
        return -1;

    char *body = (char *)malloc(content_len + 1u);
    if (!body)
        return -1;
    if (read_http_body(fd, body, content_len + 1u, content_len) != 0) {
        free(body);
        return -1;
    }

    char user[2048];
    if (extract_last_user_content(body, user, sizeof(user)) < 0) {
        free(body);
        return -1;
    }
    free(body);

    char ans[4096];
    if (complete(user, ans, sizeof(ans), ctx) != 0) {
        const char *resp = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nConnection: "
                           "close\r\n\r\ncompletion failed\n";
        (void)write(fd, resp, strlen(resp));
        return 0;
    }

    char esc[8192];
    if (cos_json_escape_cstr(ans, esc, sizeof(esc)) < 0) {
        const char *resp = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: application/json\r\nConnection: "
                           "close\r\n\r\n{\"error\":\"escape_failed\"}\n";
        (void)write(fd, resp, strlen(resp));
        return 0;
    }

    char payload[16384];
    int pl = snprintf(payload, sizeof payload,
                      "{"
                      "\"object\":\"chat.completion\","
                      "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"%s\"}}]"
                      "}",
                      esc);
    if (pl < 0 || (size_t)pl >= sizeof(payload)) {
        const char *resp = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: application/json\r\nConnection: "
                           "close\r\n\r\n{\"error\":\"payload_too_large\"}\n";
        (void)write(fd, resp, strlen(resp));
        return 0;
    }

    char resp[18432];
    int rl = snprintf(resp, sizeof resp,
                      "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: "
                      "%d\r\n\r\n%s",
                      pl, payload);
    if (rl < 0 || (size_t)rl >= sizeof(resp))
        return -1;
    (void)write(fd, resp, (size_t)rl);
    return 0;
}

int cos_http_chat_serve(unsigned short port,
                        int (*complete)(const char *prompt, char *out, size_t cap, void *ctx), void *ctx)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;
    int one = 1;
    (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) != 0) {
        close(s);
        return -1;
    }
    if (listen(s, 8) != 0) {
        close(s);
        return -1;
    }
    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        (void)handle_one(c, complete, ctx);
        close(c);
    }
    close(s);
    return 0;
}
#endif
