/* Ollama localhost autodetect for cos-chat (no Python). */

#include "ollama_detect.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int cos_tcp_probe_ipv4(const char *host, uint16_t port, int timeout_ms)
{
    if (host == NULL || host[0] == '\0' || timeout_ms < 1)
        return -1;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;
    int fl = fcntl(s, F_GETFL, 0);
    if (fl < 0 || fcntl(s, F_SETFL, fl | O_NONBLOCK) < 0) {
        close(s);
        return -1;
    }
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port   = htons(port);
    if (inet_pton(AF_INET, host, &a.sin_addr) != 1) {
        close(s);
        return -1;
    }
    if (connect(s, (struct sockaddr *)&a, sizeof a) < 0 && errno != EINPROGRESS
        && errno != EINTR) {
        close(s);
        return -1;
    }
    struct pollfd pfd = { s, POLLOUT, 0 };
    if (poll(&pfd, 1, timeout_ms) <= 0) {
        close(s);
        return -1;
    }
    int       soe = 0;
    socklen_t sl  = sizeof soe;
    if (getsockopt(s, SOL_SOCKET, SO_ERROR, &soe, &sl) < 0 || soe != 0) {
        close(s);
        return -1;
    }
    (void)fcntl(s, F_SETFL, fl);
    close(s);
    return 0;
}

int cos_ollama_http_get_tags(const char *host, uint16_t port, char *buf, size_t cap)
{
    if (buf == NULL || cap < 256u)
        return -1;
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;
    struct timeval tv;
    tv.tv_sec  = 2;
    tv.tv_usec = 0;
    (void)setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    (void)setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port   = htons(port);
    if (inet_pton(AF_INET, host, &a.sin_addr) != 1) {
        close(s);
        return -1;
    }
    if (connect(s, (struct sockaddr *)&a, sizeof a) != 0) {
        close(s);
        return -1;
    }
    char req[256];
    int  nr = snprintf(req, sizeof req,
                       "GET /api/tags HTTP/1.1\r\nHost: %s\r\nConnection: "
                       "close\r\n\r\n",
                       host);
    if (nr <= 0 || (size_t)nr >= sizeof req) {
        close(s);
        return -1;
    }
    if (send(s, req, (size_t)nr, 0) < 0) {
        close(s);
        return -1;
    }
    size_t n = 0;
    while (n + 1u < cap) {
        ssize_t r = recv(s, buf + n, cap - n - 1u, 0);
        if (r <= 0)
            break;
        n += (size_t)r;
    }
    buf[n] = '\0';
    close(s);
    return 0;
}

void cos_ollama_pick_model_from_tags(const char *http, char *model, size_t mcap)
{
    if (model == NULL || mcap == 0)
        return;
    model[0] = '\0';
    const char *body = strstr(http, "\r\n\r\n");
    if (!body)
        body = http;
    else
        body += 4;
    if (strstr(body, "\"name\":\"gemma3:4b\"")) {
        snprintf(model, mcap, "gemma3:4b");
        return;
    }
    const char *p = body;
    while ((p = strstr(p, "\"name\":\"gemma3")) != NULL) {
        p += 9u;
        size_t w = 0;
        while (*p && *p != '"' && w + 1u < mcap)
            model[w++] = *p++;
        model[w] = '\0';
        if (model[0])
            return;
    }
    p = strstr(body, "\"name\":\"");
    if (p) {
        p += 8u;
        size_t w = 0;
        while (*p && *p != '"' && w + 1u < mcap)
            model[w++] = *p++;
        model[w] = '\0';
    }
}

void cos_ollama_autodetect_apply_env(void)
{
    const char *ext = getenv("COS_BITNET_SERVER_EXTERNAL");
    if (ext != NULL)
        return;

    const char *host = "127.0.0.1";
    uint16_t    port = 11434;
    if (cos_tcp_probe_ipv4(host, port, 800) != 0)
        return;

    (void)setenv("COS_BITNET_SERVER_EXTERNAL", "1", 1);
    (void)setenv("COS_BITNET_SERVER_PORT", "11434", 1);
    (void)setenv("COS_INFERENCE_BACKEND", "ollama", 1);
    if (getenv("COS_OLLAMA_HOST") == NULL || getenv("COS_OLLAMA_HOST")[0] == '\0')
        (void)setenv("COS_OLLAMA_HOST", host, 1);
    if (getenv("COS_OLLAMA_PORT") == NULL || getenv("COS_OLLAMA_PORT")[0] == '\0')
        (void)setenv("COS_OLLAMA_PORT", "11434", 1);

    int have_chat = getenv("COS_BITNET_CHAT_MODEL") != NULL
                    && getenv("COS_BITNET_CHAT_MODEL")[0] != '\0';
    int have_ollama_m = getenv("COS_OLLAMA_MODEL") != NULL
                      && getenv("COS_OLLAMA_MODEL")[0] != '\0';

    if (have_chat && !have_ollama_m) {
        (void)setenv("COS_OLLAMA_MODEL", getenv("COS_BITNET_CHAT_MODEL"), 1);
        return;
    }
    if (have_chat)
        return;

    char tags[65536];
    char m[128];
    m[0] = '\0';
    if (cos_ollama_http_get_tags(host, port, tags, sizeof tags) == 0)
        cos_ollama_pick_model_from_tags(tags, m, sizeof m);
    if (!m[0])
        snprintf(m, sizeof m, "gemma3:4b");
    (void)setenv("COS_OLLAMA_MODEL", m, 1);
    (void)setenv("COS_BITNET_CHAT_MODEL", m, 1);
}
