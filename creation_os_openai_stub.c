/*
 * creation_os_openai_stub — minimal OpenAI-shaped HTTP server (POSIX, loopback-only).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Purpose: let local tools (Continue, Aider, etc.) point at http://127.0.0.1:<port>/v1
 * for **protocol smoke** and wiring tests. This binary does **not** run BitNet weights,
 * does not replace an IDE, and does not implement streaming SSE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "src/server/json_esc.h"

#if defined(_WIN32)
int main(void)
{
    fprintf(stderr, "creation_os_openai_stub: POSIX-only.\n");
    return 2;
}
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

static int g_checks;
static int g_fails;

static void st(const char *name, bool ok)
{
    g_checks++;
    if (!ok) {
        printf("FAIL: %s\n", name);
        g_fails++;
    }
}

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

static int extract_json_string_after_key(const char *body, const char *key, char *out, size_t cap)
{
    char pat[64];
    (void)snprintf(pat, sizeof pat, "\"%s\":\"", key);
    const char *p = strstr(body, pat);
    if (!p)
        return -1;
    p += strlen(pat);
    size_t w = 0;
    for (; *p && *p != '"'; p++) {
        if (*p == '\\' && p[1]) {
            p++;
            if (w + 1 >= cap)
                return -1;
            out[w++] = *p;
            continue;
        }
        if (w + 1 >= cap)
            return -1;
        out[w++] = *p;
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

static int mock_complete(const char *user, char *out, size_t cap)
{
    if (!user || !out || cap < 32u)
        return -1;
    (void)snprintf(out, cap, "CREATION_OS_OPENAI_STUB:%s", user);
    return 0;
}

static int handle_one(int fd)
{
    char hdr[16384];
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

    if (strstr(hdr, "GET /v1/models")) {
        const char *resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n"
                           "{\"object\":\"list\",\"data\":[{\"id\":\"creation-os-stub\",\"object\":\"model\","
                           "\"owned_by\":\"spektre-labs\"}]}\n";
        (void)write(fd, resp, strlen(resp));
        return 0;
    }

    size_t content_len = 0;
    const char *cl = strstr(hdr, "Content-Length:");
    if (cl) {
        cl += strlen("Content-Length:");
        while (*cl == ' ' || *cl == '\t')
            cl++;
        content_len = (size_t)strtoull(cl, NULL, 10);
    }

    char *body = NULL;
    if (content_len > 0 && content_len < sizeof(hdr)) {
        body = (char *)malloc(content_len + 1u);
        if (!body)
            return -1;
        if (read_http_body(fd, body, content_len + 1u, content_len) != 0) {
            free(body);
            return -1;
        }
    } else {
        body = (char *)calloc(1, 1);
        if (!body)
            return -1;
    }

    if (strstr(hdr, "POST /v1/chat/completions")) {
        if (strstr(body, "\"stream\":true")) {
            const char *resp = "HTTP/1.1 501 Not Implemented\r\nContent-Type: text/plain\r\nConnection: "
                               "close\r\n\r\nSSE streaming is not implemented in this stub.\n";
            free(body);
            (void)write(fd, resp, strlen(resp));
            return 0;
        }
        char user[4096];
        if (extract_last_user_content(body, user, sizeof(user)) < 0) {
            free(body);
            const char *resp = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nConnection: "
                               "close\r\n\r\nmissing user message\n";
            (void)write(fd, resp, strlen(resp));
            return 0;
        }
        free(body);
        char ans[8192];
        if (mock_complete(user, ans, sizeof(ans)) != 0) {
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
                          "{\"id\":\"cos-stub-1\",\"object\":\"chat.completion\","
                          "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"%s\"},"
                          "\"finish_reason\":\"stop\"}]}",
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

    if (strstr(hdr, "POST /v1/completions")) {
        char prompt[4096];
        char suffix[4096];
        int pr = extract_json_string_after_key(body, "prompt", prompt, sizeof prompt);
        int su = extract_json_string_after_key(body, "suffix", suffix, sizeof suffix);
        free(body);
        if (pr < 0) {
            const char *resp = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nConnection: "
                               "close\r\n\r\nmissing prompt\n";
            (void)write(fd, resp, strlen(resp));
            return 0;
        }
        if (su < 0)
            suffix[0] = '\0';
        char text[8192];
        (void)snprintf(text, sizeof text, "CREATION_OS_OPENAI_STUB_FIM:%s|%s", prompt, suffix);
        char esc[8192];
        if (cos_json_escape_cstr(text, esc, sizeof(esc)) < 0) {
            const char *resp = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: application/json\r\nConnection: "
                               "close\r\n\r\n{\"error\":\"escape_failed\"}\n";
            (void)write(fd, resp, strlen(resp));
            return 0;
        }
        char payload[16384];
        int pl = snprintf(payload, sizeof payload,
                          "{\"id\":\"cos-stub-fim-1\",\"object\":\"text_completion\","
                          "\"choices\":[{\"index\":0,\"text\":\"%s\",\"finish_reason\":\"stop\"}]}",
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

    free(body);
    const char *resp = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nnot found\n";
    (void)write(fd, resp, strlen(resp));
    return 0;
}

static int self_test(void)
{
    g_checks = 0;
    g_fails = 0;
    const char *b1 = "{\"messages\":[{\"role\":\"user\",\"content\":\"hello\"}]}";
    char u[256];
    st("extract_user", extract_last_user_content(b1, u, sizeof u) > 0 && strcmp(u, "hello") == 0);

    const char *b2 = "{\"prompt\":\"abc\",\"suffix\":\"def\"}";
    char p[128], s[128];
    st("extract_prompt", extract_json_string_after_key(b2, "prompt", p, sizeof p) > 0 && strcmp(p, "abc") == 0);
    st("extract_suffix", extract_json_string_after_key(b2, "suffix", s, sizeof s) > 0 && strcmp(s, "def") == 0);

    char out[512];
    st("mock_complete", mock_complete("ping", out, sizeof out) == 0 && strstr(out, "ping") != NULL);

    char esc[256];
    st("json_esc", cos_json_escape_cstr("a\"b", esc, sizeof esc) > 0 && strstr(esc, "\\\"") != NULL);

    printf("%d/%d PASS\n", g_checks - g_fails, g_checks);
    return g_fails ? 1 : 0;
}

static int serve(unsigned short port)
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
    fprintf(stderr, "openai_stub: http://127.0.0.1:%u (GET /v1/models, POST /v1/chat/completions, POST /v1/completions, GET /health)\n",
            (unsigned)port);
    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        (void)handle_one(c);
        close(c);
    }
    close(s);
    return 0;
}

int main(int argc, char **argv)
{
    unsigned short port = 8080;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--self-test"))
            return self_test();
        if (!strcmp(argv[i], "--port") && i + 1 < argc)
            port = (unsigned short)atoi(argv[++i]);
    }
    return serve(port) == 0 ? 0 : 2;
}
#endif
