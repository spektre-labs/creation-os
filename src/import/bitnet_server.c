/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 *
 *  src/import/bitnet_server.c — see bitnet_server.h.
 *
 *  Implementation notes:
 *    - Pure POSIX: fork, execvp, socket, AF_INET, send, recv,
 *      kill, waitpid, atexit.  No libcurl, no threads.
 *    - JSON parsing is deliberately minimal — we only need five
 *      fields (content, completion_probabilities, tokens_predicted,
 *      stopped_eos, stopped_limit) and the per-token `prob` floats.
 *      The parser scans linearly, tracks brace/bracket depth, and
 *      pulls scalars by substring lookup within the current object.
 *    - The response buffer is a single large static allocation
 *      (8 MiB by default) so a 1000-token reply with 5 probs each
 *      fits comfortably without heap churn.
 */

#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#ifndef _DARWIN_C_SOURCE
#  define _DARWIN_C_SOURCE
#endif

#include "bitnet_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* --- sizing knobs ------------------------------------------------ */

#define BNS_DEFAULT_HOST       "127.0.0.1"
#define BNS_DEFAULT_PORT       8088
#define BNS_DEFAULT_CTX        2048
#define BNS_DEFAULT_NPROBS     5
#define BNS_DEFAULT_NPRED      64
#define BNS_DEFAULT_EXE        "./third_party/bitnet/build/bin/llama-server"
#define BNS_DEFAULT_MODEL      "./models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf"

#define BNS_READY_TIMEOUT_S    90   /* cold-start on M-series: ~3 s */
#define BNS_REQ_TIMEOUT_S      120
#define BNS_RESP_CAP           (8u * 1024u * 1024u)
#define BNS_REQ_CAP            (1u * 1024u * 1024u)
#define BNS_TEXT_CAP           (512u * 1024u)

#define COS_BITNET_SERVER_MAX_TOKENS 4096

/* --- module state ------------------------------------------------ */

typedef struct {
    int   initialized;
    int   external;    /* 1 if COS_BITNET_SERVER_EXTERNAL=1 */
    pid_t child;       /* 0 until spawned */
    int   port;
    char  host[64];
    char  exe[512];
    char  model[512];
    int   ctx;
    int   n_probs_default;
    int   n_predict_default;

    /* Response-shaped scratch space.  Static so a cos chat session
     * that emits a 10k-token reply stays allocation-free. */
    char *resp_buf;        /* BNS_RESP_CAP */
    char *req_buf;         /* BNS_REQ_CAP  */
    char *text_buf;        /* BNS_TEXT_CAP */
} bns_state_t;

static bns_state_t g_bns = {0};

/* --- utilities --------------------------------------------------- */

static const char *env_or(const char *name, const char *fallback) {
    const char *v = getenv(name);
    return (v != NULL && v[0] != '\0') ? v : fallback;
}

static int env_int_or(const char *name, int fallback) {
    const char *v = getenv(name);
    if (v == NULL || v[0] == '\0') return fallback;
    int n = atoi(v);
    return (n > 0) ? n : fallback;
}

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0
         + (double)ts.tv_nsec / 1.0e6;
}

static void bns_alloc_scratch(void) {
    if (g_bns.resp_buf == NULL)
        g_bns.resp_buf = (char *)malloc(BNS_RESP_CAP);
    if (g_bns.req_buf == NULL)
        g_bns.req_buf  = (char *)malloc(BNS_REQ_CAP);
    if (g_bns.text_buf == NULL)
        g_bns.text_buf = (char *)malloc(BNS_TEXT_CAP);
}

static void bns_load_config(void) {
    if (g_bns.initialized) return;
    const char *host = env_or("COS_BITNET_SERVER_HOST", BNS_DEFAULT_HOST);
    strncpy(g_bns.host, host, sizeof(g_bns.host) - 1);
    g_bns.host[sizeof(g_bns.host) - 1] = '\0';

    strncpy(g_bns.exe,   env_or("COS_BITNET_SERVER_EXE",   BNS_DEFAULT_EXE),
            sizeof(g_bns.exe) - 1);
    g_bns.exe[sizeof(g_bns.exe) - 1] = '\0';
    strncpy(g_bns.model, env_or("COS_BITNET_SERVER_MODEL", BNS_DEFAULT_MODEL),
            sizeof(g_bns.model) - 1);
    g_bns.model[sizeof(g_bns.model) - 1] = '\0';

    g_bns.port              = env_int_or("COS_BITNET_SERVER_PORT",   BNS_DEFAULT_PORT);
    g_bns.ctx               = env_int_or("COS_BITNET_SERVER_CTX",    BNS_DEFAULT_CTX);
    g_bns.n_probs_default   = env_int_or("COS_BITNET_SERVER_NPROBS", BNS_DEFAULT_NPROBS);
    g_bns.n_predict_default = env_int_or("COS_BITNET_SERVER_NPRED",  BNS_DEFAULT_NPRED);

    const char *ext = getenv("COS_BITNET_SERVER_EXTERNAL");
    g_bns.external = (ext != NULL && ext[0] == '1') ? 1 : 0;

    bns_alloc_scratch();
    g_bns.initialized = 1;
}

/* --- socket helpers --------------------------------------------- */

static int bns_connect(int timeout_s) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((uint16_t)g_bns.port);
    if (inet_pton(AF_INET, g_bns.host, &sa.sin_addr) != 1) {
        close(fd);
        return -1;
    }

    struct timeval tv;
    tv.tv_sec  = timeout_s;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int bns_send_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t w = send(fd, buf, n, 0);
        if (w <= 0) {
            if (w < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            return -1;
        }
        buf += (size_t)w;
        n   -= (size_t)w;
    }
    return 0;
}

/* Read entire HTTP/1.1 response (Content-Length framed) into
 * g_bns.resp_buf.  Returns body length on success, -1 on error.
 * Writes a pointer to the start of the body into *out_body. */
static ssize_t bns_recv_response(int fd, const char **out_body) {
    size_t total = 0;
    for (;;) {
        if (total + 1 >= BNS_RESP_CAP) return -1;
        ssize_t r = recv(fd, g_bns.resp_buf + total,
                         BNS_RESP_CAP - 1 - total, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) break;
        total += (size_t)r;

        g_bns.resp_buf[total] = '\0';
        /* Headers complete? Look for \r\n\r\n. */
        char *hdr_end = strstr(g_bns.resp_buf, "\r\n\r\n");
        if (hdr_end != NULL) {
            size_t body_off = (size_t)(hdr_end - g_bns.resp_buf) + 4;
            const char *cl = strcasestr(g_bns.resp_buf, "Content-Length:");
            if (cl != NULL && cl < hdr_end) {
                long len = strtol(cl + strlen("Content-Length:"),
                                  NULL, 10);
                if (len >= 0 && body_off + (size_t)len <= total) {
                    *out_body = g_bns.resp_buf + body_off;
                    g_bns.resp_buf[body_off + (size_t)len] = '\0';
                    return (ssize_t)len;
                }
            } else {
                /* Chunked or connection-close: keep reading until
                 * recv returns 0. */
            }
        }
    }
    /* Connection closed: treat everything after headers as body. */
    char *hdr_end = strstr(g_bns.resp_buf, "\r\n\r\n");
    if (hdr_end == NULL) return -1;
    size_t body_off = (size_t)(hdr_end - g_bns.resp_buf) + 4;
    *out_body = g_bns.resp_buf + body_off;
    return (ssize_t)(total - body_off);
}

static int bns_http_get(const char *path, char *body_out, size_t body_cap) {
    int fd = bns_connect(3);
    if (fd < 0) return -1;
    int n = snprintf(g_bns.req_buf, BNS_REQ_CAP,
                     "GET %s HTTP/1.1\r\n"
                     "Host: %s:%d\r\n"
                     "Connection: close\r\n\r\n",
                     path, g_bns.host, g_bns.port);
    if (bns_send_all(fd, g_bns.req_buf, (size_t)n) != 0) {
        close(fd); return -1;
    }
    const char *body = NULL;
    ssize_t len = bns_recv_response(fd, &body);
    close(fd);
    if (len < 0 || body == NULL) return -1;
    if (body_out != NULL && body_cap > 0) {
        size_t c = (size_t)len < body_cap - 1 ? (size_t)len : body_cap - 1;
        memcpy(body_out, body, c);
        body_out[c] = '\0';
    }
    /* HTTP status: first line should start with "HTTP/1.1 200". */
    return (strncmp(g_bns.resp_buf, "HTTP/1.1 200", 12) == 0) ? 0 : -1;
}

static int bns_http_post_json(const char *path,
                              const char *json_body, size_t json_len,
                              const char **out_body, size_t *out_len) {
    int fd = bns_connect(BNS_REQ_TIMEOUT_S);
    if (fd < 0) return -1;
    int hn = snprintf(g_bns.req_buf, BNS_REQ_CAP,
                      "POST %s HTTP/1.1\r\n"
                      "Host: %s:%d\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: %zu\r\n"
                      "Connection: close\r\n\r\n",
                      path, g_bns.host, g_bns.port, json_len);
    if (hn < 0 || (size_t)hn + json_len >= BNS_REQ_CAP) {
        close(fd); return -1;
    }
    memcpy(g_bns.req_buf + hn, json_body, json_len);
    size_t total = (size_t)hn + json_len;
    if (bns_send_all(fd, g_bns.req_buf, total) != 0) {
        close(fd); return -1;
    }
    const char *body = NULL;
    ssize_t len = bns_recv_response(fd, &body);
    close(fd);
    if (len < 0 || body == NULL) return -1;
    if (strncmp(g_bns.resp_buf, "HTTP/1.1 200", 12) != 0) return -1;
    *out_body = body;
    *out_len  = (size_t)len;
    return 0;
}

/* --- minimal JSON helpers ---------------------------------------- */
/*
 * We do not need a general JSON parser.  The shape from llama-server
 * is well-known; we just need:
 *    - a string value for the top-level "content" field
 *    - an integer for "tokens_predicted", "stopped_eos", "stopped_limit"
 *    - a scan over "completion_probabilities":[ ... ] where each
 *      element has "probs":[ { "prob": <num>, ... }, ... ]
 *
 * The scanner respects string quoting + backslash escapes and
 * balanced braces/brackets; it is robust against nested objects in
 * the common path.
 */

static int json_skip_ws(const char **p, const char *end) {
    const char *q = *p;
    while (q < end && (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r'))
        q++;
    *p = q;
    return (q < end) ? 1 : 0;
}

static int json_parse_string(const char **p, const char *end,
                             char *out, size_t out_cap, size_t *out_len) {
    const char *q = *p;
    if (q >= end || *q != '"') return -1;
    q++;
    size_t w = 0;
    while (q < end && *q != '"') {
        if (*q == '\\' && q + 1 < end) {
            q++;
            char esc = *q++;
            char c;
            switch (esc) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '"': c = '"';  break;
                case '\\': c = '\\';break;
                case '/': c = '/';  break;
                case 'u': {
                    if (q + 4 > end) return -1;
                    unsigned int cp = 0;
                    for (int i = 0; i < 4; i++) {
                        char h = q[i];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                        else return -1;
                    }
                    q += 4;
                    /* UTF-8 encode */
                    if (cp < 0x80u) {
                        if (w + 1 >= out_cap) return -1;
                        out[w++] = (char)cp;
                    } else if (cp < 0x800u) {
                        if (w + 2 >= out_cap) return -1;
                        out[w++] = (char)(0xC0u | (cp >> 6));
                        out[w++] = (char)(0x80u | (cp & 0x3Fu));
                    } else {
                        if (w + 3 >= out_cap) return -1;
                        out[w++] = (char)(0xE0u | (cp >> 12));
                        out[w++] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
                        out[w++] = (char)(0x80u | (cp & 0x3Fu));
                    }
                    continue;
                }
                default: c = esc; break;
            }
            if (w + 1 >= out_cap) return -1;
            out[w++] = c;
        } else {
            if (w + 1 >= out_cap) return -1;
            out[w++] = *q++;
        }
    }
    if (q >= end) return -1;
    q++;  /* closing quote */
    out[w] = '\0';
    if (out_len) *out_len = w;
    *p = q;
    return 0;
}

static const char *json_find_key(const char *s, const char *end,
                                 const char *key) {
    /* Linear scan for "<key>" — not scope-aware but good enough for
     * the top-level llama-server shape.  Returns pointer to the char
     * after the closing quote (ready to consume ':'), or NULL. */
    size_t klen = strlen(key);
    const char *p = s;
    while (p + klen + 2 < end) {
        const char *q = (const char *)memchr(p, '"', (size_t)(end - p));
        if (q == NULL) return NULL;
        if (q + klen + 1 < end
            && memcmp(q + 1, key, klen) == 0
            && q[1 + klen] == '"') {
            const char *r = q + 2 + klen;
            while (r < end && (*r == ' ' || *r == '\t')) r++;
            if (r < end && *r == ':') {
                r++;
                while (r < end && (*r == ' ' || *r == '\t'
                                   || *r == '\n' || *r == '\r')) r++;
                return r;
            }
            p = q + 1;
        } else {
            p = q + 1;
        }
    }
    return NULL;
}

/* Skip a JSON value at *p, handling strings/objects/arrays/scalars.
 * Returns 0 on success, -1 on error.  *p advances past the value.
 * Kept for future parser paths (e.g. a full OpenAI-shape response
 * walker); current `/completion` reducer scans with json_find_key. */
static int json_skip_value(const char **p, const char *end)
    __attribute__((unused));
static int json_skip_value(const char **p, const char *end) {
    if (!json_skip_ws(p, end)) return -1;
    char c = **p;
    if (c == '"') {
        char dummy[1]; size_t dl;
        (void)dummy; (void)dl;
        const char *q = *p + 1;
        while (q < end && *q != '"') {
            if (*q == '\\' && q + 1 < end) q += 2;
            else q++;
        }
        if (q >= end) return -1;
        *p = q + 1;
        return 0;
    }
    if (c == '{' || c == '[') {
        char open = c, close = (c == '{') ? '}' : ']';
        int depth = 0;
        const char *q = *p;
        while (q < end) {
            if (*q == '"') {
                q++;
                while (q < end && *q != '"') {
                    if (*q == '\\' && q + 1 < end) q += 2;
                    else q++;
                }
                if (q >= end) return -1;
                q++;
                continue;
            }
            if (*q == open)  depth++;
            if (*q == close) { depth--; q++; if (depth == 0) { *p = q; return 0; } continue; }
            q++;
        }
        return -1;
    }
    /* scalar: number/true/false/null */
    const char *q = *p;
    while (q < end && *q != ',' && *q != '}' && *q != ']'
           && *q != ' ' && *q != '\t' && *q != '\n' && *q != '\r')
        q++;
    *p = q;
    return 0;
}

/* --- response parsing ------------------------------------------- */

static int parse_completion_response(const char *body, size_t body_len,
                                     cos_bitnet_server_result_t *out) {
    const char *end = body + body_len;
    const char *p;

    /* content: /v1/chat/completions nests it under
     * choices[0].message.content; /completion puts it at the top.
     * Look for "message" first (chat shape) then fall back. */
    const char *content_anchor = json_find_key(body, end, "message");
    const char *scan_from      = (content_anchor != NULL) ? content_anchor : body;
    p = json_find_key(scan_from, end, "content");
    if (p != NULL && *p == '"') {
        size_t n;
        if (json_parse_string(&p, end, g_bns.text_buf, BNS_TEXT_CAP, &n) == 0) {
            out->text = g_bns.text_buf;
        } else {
            g_bns.text_buf[0] = '\0';
            out->text = g_bns.text_buf;
        }
    } else {
        g_bns.text_buf[0] = '\0';
        out->text = g_bns.text_buf;
    }

    /* tokens_predicted (llama.cpp extension; also present on chat path) */
    out->token_count = 0;
    p = json_find_key(body, end, "tokens_predicted");
    if (p != NULL) out->token_count = atoi(p);
    /* OpenAI shape: usage.completion_tokens */
    if (out->token_count == 0) {
        p = json_find_key(body, end, "completion_tokens");
        if (p != NULL) out->token_count = atoi(p);
    }

    /* stop reason: /completion → stopped_eos/stopped_limit,
     * /v1/chat/completions → finish_reason = "stop" / "length". */
    out->stopped_eos = 0;
    out->stopped_limit = 0;
    p = json_find_key(body, end, "stopped_eos");
    if (p != NULL && strncmp(p, "true", 4) == 0) out->stopped_eos = 1;
    p = json_find_key(body, end, "stopped_limit");
    if (p != NULL && strncmp(p, "true", 4) == 0) out->stopped_limit = 1;
    p = json_find_key(body, end, "finish_reason");
    if (p != NULL && *p == '"') {
        if (strncmp(p, "\"stop\"", 6) == 0)        out->stopped_eos   = 1;
        else if (strncmp(p, "\"length\"", 8) == 0) out->stopped_limit = 1;
    }

    /* completion_probabilities: array of { content, probs:[{prob},...] }.
     * Walk the array; for each inner probs[0] pick the prob. */
    float max_sigma = 0.0f;
    double sum_sigma = 0.0;
    int n_tokens = 0;

    p = json_find_key(body, end, "completion_probabilities");
    if (p != NULL && *p == '[') {
        const char *q = p + 1;
        /* iterate elements */
        while (q < end) {
            if (!json_skip_ws(&q, end)) break;
            if (*q == ']') break;
            if (*q != '{') break;
            /* one token object */
            const char *obj_start = q;
            int depth = 0;
            const char *obj_end = q;
            while (obj_end < end) {
                if (*obj_end == '"') {
                    obj_end++;
                    while (obj_end < end && *obj_end != '"') {
                        if (*obj_end == '\\' && obj_end + 1 < end) obj_end += 2;
                        else obj_end++;
                    }
                    if (obj_end >= end) break;
                    obj_end++;
                    continue;
                }
                if (*obj_end == '{') depth++;
                if (*obj_end == '}') { depth--; obj_end++; if (depth == 0) break; continue; }
                obj_end++;
            }
            if (obj_end > end) obj_end = end;

            /* Find probs array inside [obj_start .. obj_end). */
            const char *pp = json_find_key(obj_start, obj_end, "probs");
            float chosen_prob = 1.0f;
            if (pp != NULL && *pp == '[') {
                const char *pq = pp + 1;
                json_skip_ws(&pq, obj_end);
                if (pq < obj_end && *pq == '{') {
                    /* first probs entry — the sampled token */
                    const char *fo_start = pq;
                    int d = 0;
                    const char *fo_end = pq;
                    while (fo_end < obj_end) {
                        if (*fo_end == '"') {
                            fo_end++;
                            while (fo_end < obj_end && *fo_end != '"') {
                                if (*fo_end == '\\' && fo_end + 1 < obj_end) fo_end += 2;
                                else fo_end++;
                            }
                            if (fo_end >= obj_end) break;
                            fo_end++;
                            continue;
                        }
                        if (*fo_end == '{') d++;
                        if (*fo_end == '}') { d--; fo_end++; if (d == 0) break; continue; }
                        fo_end++;
                    }
                    const char *pr = json_find_key(fo_start, fo_end, "prob");
                    if (pr != NULL) {
                        chosen_prob = (float)strtod(pr, NULL);
                    } else {
                        /* post_sampling_probs=false case: logprob */
                        const char *lp = json_find_key(fo_start, fo_end, "logprob");
                        if (lp != NULL) {
                            double lg = strtod(lp, NULL);
                            /* logprob is in natural log */
                            chosen_prob = (float)exp(lg);
                            /* fallback if still invalid */
                            if (chosen_prob < 0.0f) chosen_prob = 0.0f;
                            if (chosen_prob > 1.0f) chosen_prob = 1.0f;
                        }
                    }
                }
            }
            if (chosen_prob < 0.0f) chosen_prob = 0.0f;
            if (chosen_prob > 1.0f) chosen_prob = 1.0f;
            float sigma_tok = 1.0f - chosen_prob;
            if (sigma_tok > max_sigma) max_sigma = sigma_tok;
            sum_sigma += (double)sigma_tok;
            n_tokens++;

            q = obj_end;
            json_skip_ws(&q, end);
            if (q < end && *q == ',') q++;
        }
    }

    if (n_tokens == 0) {
        out->sigma      = 1.0f;
        out->mean_sigma = 1.0f;
    } else {
        out->sigma      = max_sigma;
        out->mean_sigma = (float)(sum_sigma / (double)n_tokens);
        if (out->token_count == 0) out->token_count = n_tokens;
    }
    return 0;
}

/* --- JSON encoding for the request body -------------------------- */

static size_t json_encode_string(char *dst, size_t cap, const char *s) {
    size_t w = 0;
    if (w + 1 >= cap) return 0;
    dst[w++] = '"';
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            if (w + 2 >= cap) return 0;
            dst[w++] = '\\';
            dst[w++] = (char)c;
        } else if (c == '\n') {
            if (w + 2 >= cap) return 0;
            dst[w++] = '\\'; dst[w++] = 'n';
        } else if (c == '\r') {
            if (w + 2 >= cap) return 0;
            dst[w++] = '\\'; dst[w++] = 'r';
        } else if (c == '\t') {
            if (w + 2 >= cap) return 0;
            dst[w++] = '\\'; dst[w++] = 't';
        } else if (c < 0x20u) {
            if (w + 6 >= cap) return 0;
            w += (size_t)snprintf(dst + w, cap - w, "\\u%04x", c);
        } else {
            if (w + 1 >= cap) return 0;
            dst[w++] = (char)c;
        }
    }
    if (w + 1 >= cap) return 0;
    dst[w++] = '"';
    return w;
}

/* --- spawn / health --------------------------------------------- */

int cos_bitnet_server_is_healthy(void) {
    bns_load_config();
    char body[64];
    if (bns_http_get("/health", body, sizeof(body)) != 0) return 0;
    return (strstr(body, "\"status\":\"ok\"") != NULL) ? 1 : 0;
}

static int bns_wait_ready(int timeout_s) {
    for (int i = 0; i < timeout_s * 10; i++) {
        if (cos_bitnet_server_is_healthy()) return 0;
        struct timespec sl = { 0, 100 * 1000 * 1000 };
        nanosleep(&sl, NULL);
    }
    return -1;
}

static void bns_atexit(void) {
    cos_bitnet_server_shutdown();
}

int cos_bitnet_server_ensure(void) {
    bns_load_config();

    /* Already healthy?  (external mode or pre-existing spawn.) */
    if (cos_bitnet_server_is_healthy()) return 0;

    if (g_bns.external) {
        /* Caller promised an external server; propagate failure. */
        return -1;
    }

    /* Fork + exec llama-server. */
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        /* child: redirect stdout/stderr to /tmp/cos-bitnet-server.log */
        int fd = open("/tmp/cos-bitnet-server.log",
                      O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0) { dup2(fd, 1); dup2(fd, 2); close(fd); }

        /* detach from our process group so Ctrl-C in the parent
         * terminal doesn't also kill the server (we'll SIGTERM it
         * from atexit anyway). */
        setpgid(0, 0);

        char port_s[16]; snprintf(port_s, sizeof port_s, "%d", g_bns.port);
        char ctx_s[16];  snprintf(ctx_s,  sizeof ctx_s,  "%d", g_bns.ctx);

        char *argv[32];
        int a = 0;
        argv[a++] = (char *)g_bns.exe;
        argv[a++] = (char *)"--model";
        argv[a++] = (char *)g_bns.model;
        argv[a++] = (char *)"--host";
        argv[a++] = (char *)g_bns.host;
        argv[a++] = (char *)"--port";
        argv[a++] = port_s;
        argv[a++] = (char *)"--ctx-size";
        argv[a++] = ctx_s;
        argv[a++] = (char *)"--parallel";
        argv[a++] = (char *)"1";
        argv[a++] = (char *)"-ngl";
        argv[a++] = (char *)"0";
        argv[a++] = NULL;
        execvp(g_bns.exe, argv);
        /* execvp failed. */
        fprintf(stderr, "cos_bitnet_server: execvp(%s) failed: %s\n",
                g_bns.exe, strerror(errno));
        _exit(127);
    }
    /* parent */
    g_bns.child = pid;
    atexit(bns_atexit);

    if (bns_wait_ready(BNS_READY_TIMEOUT_S) != 0) {
        cos_bitnet_server_shutdown();
        return -1;
    }
    return 0;
}

void cos_bitnet_server_shutdown(void) {
    if (g_bns.child > 0) {
        kill(g_bns.child, SIGTERM);
        int status = 0;
        for (int i = 0; i < 30; i++) {
            pid_t r = waitpid(g_bns.child, &status, WNOHANG);
            if (r == g_bns.child || r < 0) break;
            struct timespec sl = { 0, 100 * 1000 * 1000 };
            nanosleep(&sl, NULL);
        }
        /* last resort */
        kill(g_bns.child, SIGKILL);
        waitpid(g_bns.child, &status, 0);
        g_bns.child = 0;
    }
}

void cos_bitnet_server_diag(const char **out_host, int *out_port,
                            int *out_pid) {
    bns_load_config();
    if (out_host) *out_host = g_bns.host;
    if (out_port) *out_port = g_bns.port;
    if (out_pid)  *out_pid  = (int)g_bns.child;
}

/* --- test hook (declared in tests/import/test_bitnet_server_parse.c)
 *
 * Exposes the static reducer to the CI unit test without widening
 * the public header.  The ordinary build path does not reference
 * this symbol. */
int cos_bitnet_server__test_parse(const char *body, size_t len,
                                  cos_bitnet_server_result_t *out);
int cos_bitnet_server__test_parse(const char *body, size_t len,
                                  cos_bitnet_server_result_t *out) {
    memset(out, 0, sizeof(*out));
    bns_alloc_scratch();
    return parse_completion_response(body, len, out);
}

/* --- main entry -------------------------------------------------- */

int cos_bitnet_server_complete(const char                       *prompt,
                               const cos_bitnet_server_params_t *params,
                               cos_bitnet_server_result_t       *out) {
    if (prompt == NULL || out == NULL) return -1;
    memset(out, 0, sizeof(*out));
    bns_load_config();
    if (cos_bitnet_server_ensure() != 0) return -2;

    int   n_predict   = params && params->n_predict   > 0
                            ? params->n_predict   : g_bns.n_predict_default;
    int   n_probs     = params && params->n_probs     > 0
                            ? params->n_probs     : g_bns.n_probs_default;
    int   seed        = params ? params->seed : -1;
    float temperature = params ? params->temperature  : 0.0f;
    const char *stop  = params ? params->stop_word    : NULL;
    const char *syspr = params ? params->system_prompt: NULL;

    /* Build JSON body for /v1/chat/completions.
     *
     * We use the OpenAI-shape `messages` array instead of raw
     * /completion because the BitNet-b1.58 llama.cpp fork has a
     * degenerate sampler state on /completion that returns a
     * literal "@@@@" decode + `"prob": null` for every token.
     * The chat endpoint wraps the prompt in the model's chat
     * template and exercises the full sampler path correctly;
     * logprobs=true + top_logprobs=N yields the same per-token
     * `completion_probabilities` array, with real prob floats.
     *
     * Shape:
     *   { "model": "bitnet",
     *     "messages": [ {"role":"system","content":"<codex>"},
     *                   {"role":"user",  "content":"<prompt>"} ],
     *     "max_tokens": N,
     *     "logprobs": true,
     *     "top_logprobs": n_probs }
     */
    char *b = g_bns.req_buf;
    const size_t JSON_CAP = BNS_REQ_CAP - 4096;
    char *jb = b + 4096;
    size_t w = 0;
    int n;
    n = snprintf(jb + w, JSON_CAP - w,
                 "{\"model\":\"bitnet\",\"messages\":[");
    if (n < 0 || (size_t)n >= JSON_CAP - w) return -3;
    w += (size_t)n;

    if (syspr != NULL && syspr[0] != '\0') {
        n = snprintf(jb + w, JSON_CAP - w, "{\"role\":\"system\",\"content\":");
        if (n < 0) return -3; w += (size_t)n;
        size_t sw = json_encode_string(jb + w, JSON_CAP - w, syspr);
        if (sw == 0) return -3; w += sw;
        n = snprintf(jb + w, JSON_CAP - w, "},");
        if (n < 0) return -3; w += (size_t)n;
    }
    n = snprintf(jb + w, JSON_CAP - w, "{\"role\":\"user\",\"content\":");
    if (n < 0) return -3; w += (size_t)n;
    {
        size_t sw = json_encode_string(jb + w, JSON_CAP - w, prompt);
        if (sw == 0) return -3; w += sw;
    }
    n = snprintf(jb + w, JSON_CAP - w,
                 "}],\"max_tokens\":%d,\"logprobs\":true,"
                 "\"top_logprobs\":%d,\"cache_prompt\":true",
                 n_predict, n_probs);
    if (n < 0) return -3; w += (size_t)n;

    if (temperature > 0.0f) {
        n = snprintf(jb + w, JSON_CAP - w, ",\"temperature\":%.3f",
                     (double)temperature);
        if (n < 0) return -3; w += (size_t)n;
    }
    if (seed >= 0) {
        n = snprintf(jb + w, JSON_CAP - w, ",\"seed\":%d", seed);
        if (n < 0) return -3; w += (size_t)n;
    }
    if (stop != NULL && stop[0] != '\0') {
        n = snprintf(jb + w, JSON_CAP - w, ",\"stop\":[");
        if (n < 0) return -3; w += (size_t)n;
        size_t sw = json_encode_string(jb + w, JSON_CAP - w, stop);
        if (sw == 0) return -3; w += sw;
        n = snprintf(jb + w, JSON_CAP - w, "]");
        if (n < 0) return -3; w += (size_t)n;
    }
    n = snprintf(jb + w, JSON_CAP - w, "}");
    if (n < 0) return -3; w += (size_t)n;

    double t0 = now_ms();
    const char *body = NULL;
    size_t      blen = 0;
    /* Copy JSON back to its own buffer, because post_json reuses
     * g_bns.req_buf for the HTTP header. */
    static char json_tmp[BNS_REQ_CAP];
    if (w >= sizeof(json_tmp)) return -3;
    memcpy(json_tmp, jb, w);
    int rc = bns_http_post_json("/v1/chat/completions",
                                json_tmp, w, &body, &blen);
    if (rc != 0 || body == NULL) return -4;

    const char *debug = getenv("COS_BITNET_SERVER_DEBUG");
    if (debug != NULL && debug[0] == '1') {
        fprintf(stderr, "--- bitnet_server: REQ ---\n%.*s\n"
                        "--- bitnet_server: RESP (%zu bytes) ---\n%.*s\n",
                (int)w, json_tmp, blen, (int)blen, body);
    }
    rc = parse_completion_response(body, blen, out);
    out->elapsed_ms = now_ms() - t0;
    out->cost_eur   = 0.0001;  /* BitNet local electricity estimate */

    /* Degenerate-sampler salvage pass.
     *
     * The BitNet-b1.58 llama.cpp fork occasionally collapses into a
     * repeating single-character decode with `"prob": null` on
     * every `probs[0]`.  Symptom: token_count > 0 AND σ_max ≥ 0.999
     * AND σ_mean ≥ 0.999 (all tokens at maximum uncertainty).  When
     * we see this AND the caller did not already pin a seed, do
     * ONE internal retry at an elevated temperature + distinct
     * seed.  This keeps the σ signal honest without letting a
     * single sampler glitch short-circuit the whole pipeline into
     * ABSTAIN before the caller gets a chance to RETHINK. */
    int already_retried = (params != NULL && params->seed >= 0);
    if (!already_retried && out->token_count > 0
        && out->sigma >= 0.999f && out->mean_sigma >= 0.999f) {

        if (debug != NULL && debug[0] == '1') {
            fprintf(stderr, "--- bitnet_server: SALVAGE (σ=1.0 → retry seed=1013 T=0.9)\n");
        }
        cos_bitnet_server_params_t p2;
        if (params) p2 = *params;
        else        memset(&p2, 0, sizeof(p2));
        p2.seed        = 1013;   /* arbitrary prime, avoids 0 */
        p2.temperature = 0.9f;
        if (p2.n_predict <= 0) p2.n_predict = g_bns.n_predict_default;
        if (p2.n_probs   <= 0) p2.n_probs   = g_bns.n_probs_default;

        cos_bitnet_server_result_t r2;
        memset(&r2, 0, sizeof(r2));
        /* Guard against infinite recursion: we've already set a
         * seed, so the retry branch above is suppressed. */
        int rc2 = cos_bitnet_server_complete(prompt, &p2, &r2);
        if (debug != NULL && debug[0] == '1') {
            fprintf(stderr, "--- bitnet_server: SALVAGE rc=%d σ=%.3f (vs %.3f)\n",
                    rc2, (double)r2.sigma, (double)out->sigma);
        }
        if (rc2 == 0 && r2.sigma < out->sigma) {
            *out = r2;  /* adopt the better result */
        }
    }
    return rc;
}
