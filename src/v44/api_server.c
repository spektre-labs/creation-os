/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "api_server.h"

#include "sigma_proxy.h"
#include "sigma_stream.h"

#include "../server/json_esc.h"
#include "../sigma/syndrome_decoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static v44_sigma_config_t g_sigma_cfg;
static int g_cfg_inited;

static void v44_api_cfg_init(void)
{
    if (g_cfg_inited) {
        return;
    }
    v44_sigma_config_default(&g_sigma_cfg);
    g_cfg_inited = 1;
}

static const char *v44_action_name(sigma_action_t a)
{
    switch (a) {
    case ACTION_EMIT:
        return "emit";
    case ACTION_ABSTAIN:
        return "abstain";
    case ACTION_FALLBACK:
        return "fallback";
    case ACTION_RESAMPLE:
        return "resample";
    case ACTION_DECOMPOSE:
        return "decompose";
    case ACTION_CITE:
        return "cite";
    default:
        return "unknown";
    }
}

static int read_http_body(int fd, char *buf, size_t cap, size_t content_len)
{
    if (content_len + 1u > cap) {
        return -1;
    }
    size_t got = 0;
    while (got < content_len) {
        ssize_t n = read(fd, buf + got, content_len - got);
        if (n <= 0) {
            return -1;
        }
        got += (size_t)n;
    }
    buf[got] = '\0';
    return 0;
}

static int extract_last_user_content(const char *body, char *out, size_t cap)
{
    const char *last = NULL;
    for (const char *p = strstr(body, "\"role\":\"user\""); p != NULL; p = strstr(p + 1, "\"role\":\"user\"")) {
        last = p;
    }
    if (!last) {
        return -1;
    }
    const char *c = strstr(last, "\"content\":\"");
    if (!c) {
        return -1;
    }
    c += strlen("\"content\":\"");
    size_t w = 0;
    for (; *c && *c != '"'; c++) {
        if (*c == '\\' && c[1]) {
            c++;
            if (w + 1 >= cap) {
                return -1;
            }
            out[w++] = *c;
            continue;
        }
        if (w + 1 >= cap) {
            return -1;
        }
        out[w++] = *c;
    }
    out[w] = '\0';
    return (int)w;
}

static int body_wants_stream(const char *body)
{
    return body && strstr(body, "\"stream\":true") != NULL;
}

static int v44_write_all(int fd, const char *p, size_t n)
{
    size_t o = 0;
    while (o < n) {
        ssize_t w = write(fd, p + o, n - o);
        if (w <= 0) {
            return -1;
        }
        o += (size_t)w;
    }
    return 0;
}

static int handle_chat_stream(int fd, const char *user_msg, const v44_proxy_response_t *pr)
{
    const char *hdr =
        "HTTP/1.1 200 OK\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Content-Type: text/event-stream\r\n"
        "Connection: close\r\n"
        "\r\n";
    if (v44_write_all(fd, hdr, strlen(hdr)) != 0) {
        return -1;
    }
    sigma_decomposed_t per;
    memset(&per, 0, sizeof per);
    if (pr->sigmas && pr->n_tokens > 0) {
        per = pr->sigmas[0];
    }
    char line[512];
    const char *words[] = {"sigma", "proxy", "stream", "ok"};
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        int n = v44_stream_format_token_line(words[i], &per, line, sizeof line);
        if (n < 0 || v44_write_all(fd, line, (size_t)n) != 0) {
            return -1;
        }
    }
    int n2 = v44_stream_format_sigma_alert("demo_only", "none", line, sizeof line);
    if (n2 < 0 || v44_write_all(fd, line, (size_t)n2) != 0) {
        return -1;
    }
    (void)user_msg;
    const char *done = "data: [DONE]\n\n";
    return v44_write_all(fd, done, strlen(done));
}

static int handle_chat_json(int fd, const char *user_msg)
{
    (void)user_msg;
    v44_api_cfg_init();
    v44_engine_config_t eng = {"stub", "loopback", 1};
    v44_proxy_response_t pr;
    memset(&pr, 0, sizeof pr);
    if (v44_sigma_proxy_generate(&eng, user_msg, &g_sigma_cfg, 0, &pr) != 0) {
        const char *resp = "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin: *\r\n"
                           "Content-Type: text/plain\r\nConnection: close\r\n\r\nproxy_generate_failed\n";
        return v44_write_all(fd, resp, strlen(resp));
    }
    char esc[8192];
    if (!pr.text || cos_json_escape_cstr(pr.text, esc, sizeof esc) < 0) {
        v44_proxy_response_free(&pr);
        const char *resp = "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin: *\r\n"
                           "Content-Type: application/json\r\nConnection: close\r\n\r\n{\"error\":\"escape_failed\"}\n";
        return v44_write_all(fd, resp, strlen(resp));
    }
    sigma_decomposed_t agg;
    v44_aggregate_sigmas(pr.sigmas, pr.n_tokens, &agg);
    float svec[CH_COUNT];
    v44_agg_to_syndrome(&agg, svec);
    sigma_action_t act = pr.action;
    v44_proxy_response_free(&pr);
    char payload[16384];
    int pl = snprintf(payload, sizeof payload,
                      "{\"id\":\"creation-os-proxy-1\",\"object\":\"chat.completion\","
                      "\"choices\":[{\"index\":0,\"message\":{\"role\":\"assistant\",\"content\":\"%s\"},"
                      "\"finish_reason\":\"stop\","
                      "\"sigma\":{\"total\":%.5f,\"epistemic\":%.5f,\"aleatoric\":%.5f,\"action\":\"%s\","
                      "\"channels\":{\"logit_entropy\":%.5f,\"top_margin\":%.5f,\"prediction_stability\":%.5f,"
                      "\"critical_token_entropy\":%.5f}}}]}",
                      esc, (double)agg.total, (double)agg.epistemic, (double)agg.aleatoric, v44_action_name(act),
                      (double)svec[CH_LOGIT_ENTROPY], (double)svec[CH_TOP_MARGIN], (double)svec[CH_REPETITION],
                      (double)svec[CH_EPISTEMIC]);
    if (pl < 0 || (size_t)pl >= sizeof(payload)) {
        const char *resp = "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin: *\r\n"
                           "Content-Type: application/json\r\nConnection: close\r\n\r\n{\"error\":\"payload_too_large\"}\n";
        return v44_write_all(fd, resp, strlen(resp));
    }
    char resp[18432];
    int rl = snprintf(resp, sizeof resp,
                      "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers: Content-Type\r\n"
                      "Content-Type: application/json\r\nConnection: close\r\nContent-Length: %d\r\n\r\n%s",
                      pl, payload);
    if (rl < 0 || (size_t)rl >= sizeof(resp)) {
        return -1;
    }
    return v44_write_all(fd, resp, (size_t)rl);
}

int v44_api_handle_one(int client_fd)
{
    v44_api_cfg_init();
    char hdr[16384];
    size_t hp = 0;
    while (hp + 4u < sizeof(hdr)) {
        ssize_t n = read(client_fd, hdr + hp, 1);
        if (n != 1) {
            return -1;
        }
        hp++;
        if (hp >= 4u && memcmp(hdr + hp - 4, "\r\n\r\n", 4) == 0) {
            break;
        }
    }
    hdr[hp] = '\0';

    if (strncmp(hdr, "OPTIONS", 7) == 0) {
        const char *resp = "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\n"
                           "Access-Control-Allow-Headers: Content-Type\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                           "Connection: close\r\n\r\n";
        (void)v44_write_all(client_fd, resp, strlen(resp));
        return 0;
    }

    if (strncmp(hdr, "GET /health", 11) == 0) {
        const char *resp = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/plain\r\n"
                           "Connection: close\r\n\r\nok\n";
        (void)v44_write_all(client_fd, resp, strlen(resp));
        return 0;
    }

    size_t content_len = 0;
    const char *cl = strstr(hdr, "Content-Length:");
    if (cl) {
        cl += strlen("Content-Length:");
        while (*cl == ' ' || *cl == '\t') {
            cl++;
        }
        content_len = (size_t)strtoull(cl, NULL, 10);
    }

    char *body = NULL;
    if (content_len > 0 && content_len < sizeof(hdr)) {
        body = (char *)malloc(content_len + 1u);
        if (!body) {
            return -1;
        }
        if (read_http_body(client_fd, body, content_len + 1u, content_len) != 0) {
            free(body);
            return -1;
        }
    } else {
        body = (char *)calloc(1, 1);
        if (!body) {
            return -1;
        }
    }

    if (strstr(hdr, "POST /v1/chat/completions")) {
        char user[4096];
        if (extract_last_user_content(body, user, sizeof user) < 0) {
            free(body);
            const char *resp = "HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\n"
                               "Content-Type: text/plain\r\nConnection: close\r\n\r\nmissing user message\n";
            (void)v44_write_all(client_fd, resp, strlen(resp));
            return 0;
        }
        int stream = body_wants_stream(body);
        free(body);
        if (stream) {
            v44_engine_config_t eng = {"stub", "loopback", 1};
            v44_proxy_response_t pr;
            memset(&pr, 0, sizeof pr);
            if (v44_sigma_proxy_generate(&eng, user, &g_sigma_cfg, 0, &pr) != 0) {
                const char *resp = "HTTP/1.1 500 Internal Server Error\r\nAccess-Control-Allow-Origin: *\r\n"
                                   "Content-Type: text/plain\r\nConnection: close\r\n\r\nproxy_generate_failed\n";
                (void)v44_write_all(client_fd, resp, strlen(resp));
                return 0;
            }
            int rc = handle_chat_stream(client_fd, user, &pr);
            v44_proxy_response_free(&pr);
            return rc;
        }
        return handle_chat_json(client_fd, user);
    }

    free(body);
    const char *resp = "HTTP/1.1 404 Not Found\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: text/plain\r\n"
                       "Connection: close\r\n\r\nnot found\n";
    (void)v44_write_all(client_fd, resp, strlen(resp));
    return 0;
}
