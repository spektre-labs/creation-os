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
 *      kill, waitpid, atexit.  No libcurl.  Optional pthread overlap
 *      for semantic-σ dual-temperature probes lives in semantic_sigma.c
 *      (COS_SEMANTIC_SIGMA_USE_PTHREAD=1; default remains fork isolation).
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
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* --- sizing knobs ------------------------------------------------ */

#define BNS_DEFAULT_HOST       "127.0.0.1"
#define BNS_DEFAULT_PORT       8088
/* Default context: prefer COS_BITNET_CHAT_CTX (chat-side hint), then
 * COS_BITNET_SERVER_CTX.  Low-RAM hosts should use 2048 or 1024. */
#define BNS_DEFAULT_CTX        2048
#define BNS_DEFAULT_NPROBS     5
#define BNS_DEFAULT_NPRED      64
#define BNS_DEFAULT_EXE        "./third_party/bitnet/build/bin/llama-server"
/* Prefer Qwen3.5-9B when present; resolver falls back to Qwen3-8B GGUF. */
#define BNS_DEFAULT_MODEL      "./models/qwen3.5-9b-Q4_K_M.gguf"
#define BNS_FALLBACK_MODEL     "./models/qwen3-8b-Q4_K_M.gguf"
#define BNS_SMALL_MODEL        "./models/qwen3.5-2b-Q4_K_M.gguf"

#define BNS_READY_TIMEOUT_S    90   /* cold-start on M-series: ~3 s */
#define BNS_CONNECT_TIMEOUT_S    5
#define BNS_IO_TIMEOUT_S         60   /* recv/send; inference wall clock */
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
    int   http_port;         /* TCP port for API (llama or Ollama)     */
    int   backend_ollama;    /* 1 if COS_INFERENCE_BACKEND=ollama     */

    /* Response-shaped scratch space.  Static so a cos chat session
     * that emits a 10k-token reply stays allocation-free. */
    char *resp_buf;        /* BNS_RESP_CAP */
    char *req_buf;         /* BNS_REQ_CAP  */
    char *text_buf;        /* BNS_TEXT_CAP */
    char *reasoning_buf;   /* BNS_TEXT_CAP — parallel parse for σ split */
} bns_state_t;

static bns_state_t g_bns = {0};

static float g_bns_last_tok_sigma[COS_BITNET_SERVER_MAX_TOKENS];
static int   g_bns_last_tok_n;
static float g_bns_lp_scratch[COS_BITNET_SERVER_MAX_TOKENS];

static char bns_spawn_batch_s[16];
static char bns_spawn_ubatch_s[16];
static char bns_spawn_threads_s[16];

static double g_bns_last_ttft_ms = -1.0;

/* Ollama HTTP production hardening: circuit breaker + cooperative cancel. */
static int               g_bns_consecutive_fail   = 0;
static time_t            g_bns_circuit_open_until = 0;
static volatile sig_atomic_t g_bns_io_cancel        = 0;

#define BNS_CIRCUIT_FAIL_MAX   5
#define BNS_CIRCUIT_PAUSE_SEC  60
/* Internal recv sentinel; mapped to public -17 from bns_http_post_json_once. */
#define BNS_HTTP_RECV_INTR_STOP ((ssize_t)-77)

/* --- utilities --------------------------------------------------- */

double cos_bitnet_server_last_ttft_ms(void) {
    return g_bns_last_ttft_ms;
}

void cos_bitnet_server_clear_ttft(void) {
    g_bns_last_ttft_ms = -1.0;
}

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

static float bns_env_float_default(const char *name, float def) {
    const char *v = getenv(name);
    if (v == NULL || v[0] == '\0') return def;
    return strtof(v, NULL);
}

static int bns_env_int_clamp(const char *name, int def, int lo, int hi) {
    const char *v = getenv(name);
    if (v == NULL || v[0] == '\0') return def;
    int x = atoi(v);
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
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
    if (g_bns.reasoning_buf == NULL)
        g_bns.reasoning_buf = (char *)malloc(BNS_TEXT_CAP);
}

static int bns_model_path_exists(const char *p) {
    struct stat st;
    return (p != NULL && p[0] != '\0' && stat(p, &st) == 0) ? 1 : 0;
}

/** Pick GGUF path: COS_BITNET_SERVER_MODEL overrides everything.
 *  Else COS_MODEL_SIZE=small → COS_MODEL_SMALL_PATH (2B),
 *  else COS_MODEL_PATH or default 9B.  Missing Qwen3.5 files fall
 *  back to Qwen3-8B when present. */
static void bns_resolve_model_path(char *dst, size_t cap) {
    const char *forced = getenv("COS_BITNET_SERVER_MODEL");
    if (forced != NULL && forced[0] != '\0') {
        strncpy(dst, forced, cap - 1);
        dst[cap - 1] = '\0';
        return;
    }

    const char *size_env = getenv("COS_MODEL_SIZE");
    int          small    = (size_env != NULL && strcmp(size_env, "small") == 0);

    const char *primary =
        small ? env_or("COS_MODEL_SMALL_PATH", BNS_SMALL_MODEL)
              : env_or("COS_MODEL_PATH", BNS_DEFAULT_MODEL);

    strncpy(dst, primary, cap - 1);
    dst[cap - 1] = '\0';

    if (bns_model_path_exists(dst))
        return;

    if (bns_model_path_exists(BNS_FALLBACK_MODEL)) {
        strncpy(dst, BNS_FALLBACK_MODEL, cap - 1);
        dst[cap - 1] = '\0';
    }
}

static void bns_load_config(void) {
    if (g_bns.initialized) return;
    const char *host = env_or("COS_BITNET_SERVER_HOST", BNS_DEFAULT_HOST);
    strncpy(g_bns.host, host, sizeof(g_bns.host) - 1);
    g_bns.host[sizeof(g_bns.host) - 1] = '\0';

    strncpy(g_bns.exe,   env_or("COS_BITNET_SERVER_EXE",   BNS_DEFAULT_EXE),
            sizeof(g_bns.exe) - 1);
    g_bns.exe[sizeof(g_bns.exe) - 1] = '\0';
    bns_resolve_model_path(g_bns.model, sizeof(g_bns.model));

    g_bns.port              = env_int_or("COS_BITNET_SERVER_PORT",   BNS_DEFAULT_PORT);
    {
        const char *cc = getenv("COS_BITNET_CHAT_CTX");
        const char *cs = getenv("COS_BITNET_SERVER_CTX");
        const char *cl = getenv("COS_LLAMA_CTX");
        int         chat = (cc != NULL && cc[0] != '\0') ? atoi(cc) : 0;
        int         srv  = (cs != NULL && cs[0] != '\0') ? atoi(cs) : 0;
        int         lc   = (cl != NULL && cl[0] != '\0') ? atoi(cl) : 0;
        if (chat > 0)
            g_bns.ctx = chat;
        else if (srv > 0)
            g_bns.ctx = srv;
        else if (lc > 0)
            g_bns.ctx = lc;
        else
            g_bns.ctx = BNS_DEFAULT_CTX;
    }
    g_bns.n_probs_default   = env_int_or("COS_BITNET_SERVER_NPROBS", BNS_DEFAULT_NPROBS);
    g_bns.n_predict_default = env_int_or("COS_BITNET_SERVER_NPRED",  BNS_DEFAULT_NPRED);

    const char *ext = getenv("COS_BITNET_SERVER_EXTERNAL");
    g_bns.external = (ext != NULL && ext[0] == '1') ? 1 : 0;

    {
        const char *be = getenv("COS_INFERENCE_BACKEND");
        g_bns.backend_ollama =
            (be != NULL && strcmp(be, "ollama") == 0) ? 1 : 0;
    }
    if (g_bns.backend_ollama) {
        const char *oh = getenv("COS_OLLAMA_HOST");
        if (oh != NULL && oh[0] != '\0') {
            strncpy(g_bns.host, oh, sizeof(g_bns.host) - 1);
            g_bns.host[sizeof(g_bns.host) - 1] = '\0';
        }
        /* Prefer COS_OLLAMA_PORT; else COS_BITNET_SERVER_PORT when not the
         * llama default (8088) so `COS_BITNET_SERVER_PORT=11434` alone works. */
        const char *op = getenv("COS_OLLAMA_PORT");
        if (op != NULL && op[0] != '\0')
            g_bns.http_port = atoi(op);
        else if (g_bns.port != BNS_DEFAULT_PORT)
            g_bns.http_port = g_bns.port;
        else
            g_bns.http_port = 11434;
    } else {
        g_bns.http_port = g_bns.port;
    }

    bns_alloc_scratch();
    g_bns.initialized = 1;
}

/** Default JSON `model` for /v1/chat/completions when COS_BITNET_CHAT_MODEL
 *  is unset: Gemma on Ollama-shaped ports (11434), BitNet id otherwise. */
static const char *bns_default_openai_compat_chat_model(void) {
    bns_load_config();
    if (g_bns.backend_ollama
        || (g_bns.external && g_bns.http_port == 11434))
        return "gemma3:4b";
    return "bitnet";
}

/** Per-socket recv/send wall clock (default BNS_IO_TIMEOUT_S). Override with
 * COS_BITNET_IO_TIMEOUT_S (seconds, clamped 30..600) for slow hosts or
 * Ollama + logprobs cold starts. */
static int bns_io_timeout_sec(void) {
    const char *e = getenv("COS_BITNET_IO_TIMEOUT_S");
    if (e == NULL || e[0] == '\0')
        return BNS_IO_TIMEOUT_S;
    int v = atoi(e);
    if (v < 30)  v = 30;
    if (v > 600) v = 600;
    return v;
}

/* --- socket helpers --------------------------------------------- */

/** Blocking TCP to llama-server: connect ≤5 s, recv/send each ≤60 s
 * (or COS_BITNET_IO_TIMEOUT_S). */
static int bns_tcp_connect(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((uint16_t)g_bns.http_port);
    if (inet_pton(AF_INET, g_bns.host, &sa.sin_addr) != 1) {
        close(fd);
        return -1;
    }

    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0 || fcntl(fd, F_SETFL, fl | O_NONBLOCK) != 0) {
        close(fd);
        return -1;
    }

    int cr = connect(fd, (struct sockaddr *)&sa, sizeof(sa));
    if (cr != 0 && errno != EINPROGRESS && errno != EINTR) {
        close(fd);
        return -1;
    }

    struct pollfd pfd;
    memset(&pfd, 0, sizeof pfd);
    pfd.fd     = fd;
    pfd.events = POLLOUT;
    int pr     = poll(&pfd, 1, BNS_CONNECT_TIMEOUT_S * 1000);
    if (pr <= 0) {
        close(fd);
        return -1;
    }

    int         soerr = 0;
    socklen_t   sl    = sizeof(soerr);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &sl) != 0 || soerr != 0) {
        close(fd);
        return -1;
    }

    if (fcntl(fd, F_SETFL, fl) != 0) {
        close(fd);
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = bns_io_timeout_sec();
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
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
            if (errno == EINTR) {
                if (g_bns_io_cancel)
                    return BNS_HTTP_RECV_INTR_STOP;
                continue;
            }
#ifdef EAGAIN
            if (errno == EAGAIN)
                return -2;
#endif
#ifdef EWOULDBLOCK
            if (errno == EWOULDBLOCK)
                return -2;
#endif
#ifdef ETIMEDOUT
            if (errno == ETIMEDOUT)
                return -2;
#endif
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
    int fd = bns_tcp_connect();
    if (fd < 0) return -1;
    int n = snprintf(g_bns.req_buf, BNS_REQ_CAP,
                     "GET %s HTTP/1.1\r\n"
                     "Host: %s:%d\r\n"
                     "Connection: close\r\n\r\n",
                     path, g_bns.host, g_bns.http_port);
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

/* POLISH-3: attempt a single HTTP/1.1 POST; returns 0 on success with
 * a populated body pointer, -1 on transient transport failures that
 * the caller may want to retry. */
static int bns_http_post_json_once(const char *path,
                                   const char *json_body, size_t json_len,
                                   const char **out_body, size_t *out_len) {
    int fd = bns_tcp_connect();
    if (fd < 0)
        return -1;
    int hn = snprintf(g_bns.req_buf, BNS_REQ_CAP,
                      "POST %s HTTP/1.1\r\n"
                      "Host: %s:%d\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: %zu\r\n"
                      "Connection: close\r\n\r\n",
                      path, g_bns.host, g_bns.http_port, json_len);
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
    if (len == BNS_HTTP_RECV_INTR_STOP)
        return -17;
    if (len == -2)
        return -5;
    if (len < 0 || body == NULL)
        return -1;
    if (strncmp(g_bns.resp_buf, "HTTP/1.1 200", 12) != 0)
        return -1;
    *out_body = body;
    *out_len  = (size_t)len;
    return 0;
}

/* POLISH-3: wrapper that retries up to 3 times.  Between retries we
 * reconfirm /health; if the server has died we attempt to restart it
 * once before giving up.  Prints a single status line to stderr so
 * the user sees what happened. */
static int bns_http_post_json(const char *path,
                              const char *json_body, size_t json_len,
                              const char **out_body, size_t *out_len) {
    bns_load_config();
    const int MAX_ATTEMPTS = 3;
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt) {
        int once = bns_http_post_json_once(path, json_body, json_len,
                                            out_body, out_len);
        if (once == 0)
            return 0;
        if (once == -5)
            return -5;
        if (once == -17)
            return -17;
        if (attempt == MAX_ATTEMPTS)
            break;
        unsigned ms = 250u;
        if (g_bns.external || g_bns.backend_ollama) {
            ms = 500u * (1u << (unsigned)(attempt - 1));
            if (ms > 8000u)
                ms = 8000u;
            fprintf(stderr,
                    "[retry] HTTP POST %s attempt %d/%d failed "
                    "(backoff %ums)\n",
                    path, attempt, MAX_ATTEMPTS, ms);
        }
        struct timespec sl;
        sl.tv_sec  = (time_t)(ms / 1000u);
        sl.tv_nsec = (long)((ms % 1000u) * 1000000u);
        nanosleep(&sl, NULL);
        /* External / Ollama: never fork-restart a peer we do not own. */
        if (!g_bns.external && !g_bns.backend_ollama
            && !cos_bitnet_server_is_healthy()) {
            fprintf(stderr,
                    "cos: llama-server unresponsive (attempt %d/%d), restarting…\n",
                    attempt, MAX_ATTEMPTS);
            cos_bitnet_server_shutdown();
            if (cos_bitnet_server_ensure() != 0) {
                return -1;
            }
        }
    }
    return -1;
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

static const char *bns_find_bytes(const char *s, const char *end,
                                  const char *needle, size_t nlen) {
    if (nlen == 0 || s == NULL || end == NULL || s >= end) return NULL;
    for (; s + nlen <= end; ++s) {
        if (memcmp(s, needle, nlen) == 0) return s;
    }
    return NULL;
}

static float bns_clamp_lp_for_exp(float lp) {
    if (lp < -80.0f) return -80.0f;
    if (lp > 0.0f) return 0.0f;
    return lp;
}

/** σ from natural-log token scores (Ollama / OpenAI logprob channel):
 *  0.4·(1−mean p) + 0.3·(1−exp min_lp) + 0.3·(low_conf_ratio), where
 *  low_conf counts tokens with logprob < −3.  n==0 → neutral 0.5. */
static float bns_sigma_from_logprobs_agg(const float *lp, int n) {
    if (n <= 0) return 0.5f;
    float sum_prob = 0.0f;
    float min_lp     = lp[0];
    int   low_conf   = 0;
    for (int i = 0; i < n; i++) {
        float lpv = bns_clamp_lp_for_exp(lp[i]);
        if (lp[i] < min_lp) min_lp = lp[i];
        if (lp[i] < -3.0f) low_conf++;
        float p = expf(lpv);
        if (p > 1.0f) p = 1.0f;
        if (p < 0.0f) p = 0.0f;
        sum_prob += p;
    }
    float mean_prob       = sum_prob / (float)n;
    float sigma_mean      = 1.0f - mean_prob;
    float sigma_min       = 1.0f - expf(bns_clamp_lp_for_exp(min_lp));
    float sigma_lowconf   = (float)low_conf / (float)n;
    float sigma           = 0.4f * sigma_mean + 0.3f * sigma_min
                  + 0.3f * sigma_lowconf;
    if (sigma < 0.0f) sigma = 0.0f;
    if (sigma > 1.0f) sigma = 1.0f;
    return sigma;
}

static float bns_mean_sigma_from_lps(const float *lp, int n) {
    if (n <= 0) return 0.5f;
    double acc = 0.0;
    for (int i = 0; i < n; i++) {
        float lpv = bns_clamp_lp_for_exp(lp[i]);
        float p   = expf(lpv);
        if (p > 1.0f) p = 1.0f;
        if (p < 0.0f) p = 0.0f;
        acc += (double)(1.0f - p);
    }
    return (float)(acc / (double)n);
}

static int parse_completion_response(const char *body, size_t body_len,
                                     cos_bitnet_server_result_t *out);
static size_t json_encode_string(char *dst, size_t cap, const char *s);

/* --- adaptive σ: verbal confidence + optional consistency ------------- */

static int bns_env_flag1(const char *name) {
    const char *v = getenv(name);
    return (v != NULL && v[0] == '1' && v[1] == '\0') ? 1 : 0;
}

static float bns_sigma_clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

/** Jaccard overlap of whitespace/punctuation token sets (ASCII fold). */
static float bns_word_jaccard_overlap(const char *a, const char *b) {
    char bufa[2048], bufb[2048];
    char *wa[128], *wb[128];
    int   na = 0, nb = 0;

    strncpy(bufa, a != NULL ? a : "", sizeof(bufa) - 1);
    bufa[sizeof(bufa) - 1] = '\0';
    strncpy(bufb, b != NULL ? b : "", sizeof(bufb) - 1);
    bufb[sizeof(bufb) - 1] = '\0';

    for (char *tok = strtok(bufa, " \t\n\r.,!?;:\"'()[]{}");
         tok != NULL && na < 128; tok = strtok(NULL, " \t\n\r.,!?;:\"'()[]{}")) {
        for (char *p = tok; *p; ++p)
            if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
        wa[na++] = tok;
    }
    for (char *tok = strtok(bufb, " \t\n\r.,!?;:\"'()[]{}");
         tok != NULL && nb < 128; tok = strtok(NULL, " \t\n\r.,!?;:\"'()[]{}")) {
        for (char *p = tok; *p; ++p)
            if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
        wb[nb++] = tok;
    }
    if (na == 0 && nb == 0) return 1.0f;
    int inter = 0;
    for (int i = 0; i < na; i++) {
        for (int j = 0; j < nb; j++) {
            if (strcmp(wa[i], wb[j]) == 0) {
                inter++;
                break;
            }
        }
    }
    int uni = na + nb - inter;
    if (uni <= 0) return 0.0f;
    return (float)inter / (float)uni;
}

/** Leading integer in s (after spaces); returns -1 if none in 0..100. */
static int bns_parse_confidence_0_100(const char *s) {
    if (s == NULL) return -1;
    const char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p < '0' || *p > '9') return -1;
    int v = atoi(p);
    if (v < 0 || v > 100) return -1;
    return v;
}

/** One-shot user message; restores g_bns.text_buf after parse.
 *  On success, copies assistant text into answer_copy (aux->text = answer_copy). */
static int bns_aux_user_chat(const char *user_msg, float temperature, int max_tok,
                             int want_logprobs, cos_bitnet_server_result_t *aux,
                             char *answer_copy, size_t answer_copy_cap) {
    if (user_msg == NULL || aux == NULL || answer_copy == NULL
        || answer_copy_cap == 0)
        return -1;
    bns_alloc_scratch();

    static char hold_main[BNS_TEXT_CAP];
    strncpy(hold_main, g_bns.text_buf, sizeof(hold_main) - 1);
    hold_main[sizeof(hold_main) - 1] = '\0';

    const char *model = getenv("COS_OLLAMA_MODEL");
    if (model == NULL || model[0] == '\0')
        model = getenv("COS_BITNET_CHAT_MODEL");
    if (model == NULL || model[0] == '\0')
        model = bns_default_openai_compat_chat_model();

    char *jb  = g_bns.req_buf + 4096;
    size_t w  = 0;
    int    n;
    const size_t JSON_CAP = BNS_REQ_CAP - 4096;

    if (g_bns.backend_ollama) {
        n = snprintf(jb + w, JSON_CAP - w, "{\"model\":");
        if (n < 0 || (size_t)n >= JSON_CAP - w) goto fail;
        w += (size_t)n;
        {
            size_t mw = json_encode_string(jb + w, JSON_CAP - w, model);
            if (mw == 0) goto fail;
            w += mw;
        }
        n = snprintf(jb + w, JSON_CAP - w, ",\"stream\":false,\"messages\":[");
        if (n < 0 || (size_t)n >= JSON_CAP - w) goto fail;
        w += (size_t)n;
        n = snprintf(jb + w, JSON_CAP - w, "{\"role\":\"user\",\"content\":");
        if (n < 0) goto fail;
        w += (size_t)n;
        {
            size_t sw = json_encode_string(jb + w, JSON_CAP - w, user_msg);
            if (sw == 0) goto fail;
            w += sw;
        }
        if (want_logprobs) {
            n = snprintf(jb + w, JSON_CAP - w,
                          "}],\"max_tokens\":%d,\"logprobs\":true,"
                          "\"top_logprobs\":3,\"temperature\":%.3f",
                          max_tok, (double)temperature);
        } else {
            n = snprintf(jb + w, JSON_CAP - w,
                          "}],\"max_tokens\":%d,\"temperature\":%.3f",
                          max_tok, (double)temperature);
        }
        if (n < 0 || (size_t)n >= JSON_CAP - w) goto fail;
        w += (size_t)n;
        n = snprintf(jb + w, JSON_CAP - w, "}");
        if (n < 0 || (size_t)n >= JSON_CAP - w) goto fail;
        w += (size_t)n;
    } else {
        n = snprintf(jb + w, JSON_CAP - w, "{\"model\":");
        if (n < 0 || (size_t)n >= JSON_CAP - w) goto fail;
        w += (size_t)n;
        {
            const char *mid = getenv("COS_BITNET_CHAT_MODEL");
            if (mid == NULL || mid[0] == '\0')
                mid = bns_default_openai_compat_chat_model();
            size_t mw = json_encode_string(jb + w, JSON_CAP - w, mid);
            if (mw == 0) goto fail;
            w += mw;
        }
        n = snprintf(jb + w, JSON_CAP - w, ",\"messages\":[");
        if (n < 0) goto fail;
        w += (size_t)n;
        n = snprintf(jb + w, JSON_CAP - w, "{\"role\":\"user\",\"content\":");
        if (n < 0) goto fail;
        w += (size_t)n;
        {
            size_t sw = json_encode_string(jb + w, JSON_CAP - w, user_msg);
            if (sw == 0) goto fail;
            w += sw;
        }
        if (want_logprobs) {
            n = snprintf(jb + w, JSON_CAP - w,
                          "}],\"max_tokens\":%d,\"logprobs\":true,\"top_logprobs\":3",
                          max_tok);
        } else {
            n = snprintf(jb + w, JSON_CAP - w, "}],\"max_tokens\":%d", max_tok);
        }
        if (n < 0) goto fail;
        w += (size_t)n;
        if (temperature > 0.0f) {
            n = snprintf(jb + w, JSON_CAP - w, ",\"temperature\":%.3f",
                           (double)temperature);
            if (n < 0) goto fail;
            w += (size_t)n;
        }
        n = snprintf(jb + w, JSON_CAP - w, "}");
        if (n < 0) goto fail;
        w += (size_t)n;
    }

    static char json_tmp[BNS_REQ_CAP];
    if (w >= sizeof(json_tmp)) goto fail;
    memcpy(json_tmp, jb, w);

    const char *resp = NULL;
    size_t      rlen = 0;
    const char *path = "/v1/chat/completions";
    if (bns_http_post_json(path, json_tmp, w, &resp, &rlen) != 0
        || resp == NULL) {
        memcpy(g_bns.text_buf, hold_main, strlen(hold_main) + 1);
        return -1;
    }
    memset(aux, 0, sizeof(*aux));
    if (parse_completion_response(resp, rlen, aux) != 0) {
        memcpy(g_bns.text_buf, hold_main, strlen(hold_main) + 1);
        return -1;
    }
    strncpy(answer_copy, g_bns.text_buf, answer_copy_cap - 1);
    answer_copy[answer_copy_cap - 1] = '\0';
    aux->text = answer_copy;
    memcpy(g_bns.text_buf, hold_main, strlen(hold_main) + 1);
    return 0;

fail:
    memcpy(g_bns.text_buf, hold_main, strlen(hold_main) + 1);
    return -1;
}

static float bns_sigma_verbal_from_answer(const char *answer,
                                          const cos_bitnet_server_params_t *params) {
    (void)params;
    char meta[2048];
    char snip[201];
    char axbuf[512];
    if (answer == NULL) answer = "";
    {
        size_t al = strlen(answer);
        if (al > 200) al = 200;
        memcpy(snip, answer, al);
        snip[al] = '\0';
    }
    int mr = snprintf(meta, sizeof(meta),
                      "You just answered: %s\n"
                      "Rate your confidence in this answer from 0 to 100.\n"
                      "Reply with ONLY a number, nothing else.",
                      snip);
    if (mr < 0 || (size_t)mr >= sizeof(meta)) return -1.0f;

    cos_bitnet_server_result_t ax;
    memset(&ax, 0, sizeof(ax));
    if (bns_aux_user_chat(meta, 0.05f, 32, 0, &ax, axbuf, sizeof(axbuf)) != 0)
        return -1.0f;
    int c = bns_parse_confidence_0_100(ax.text != NULL ? ax.text : "");
    if (c < 0) return -1.0f;
    return 1.0f - (float)c / 100.0f;
}

static float bns_sigma_consistency_pair(const char *prompt,
                                        const cos_bitnet_server_params_t *params) {
    if (prompt == NULL || prompt[0] == '\0') return -1.0f;
    int mt = 64;
    if (params != NULL && params->n_predict > 0 && params->n_predict < mt)
        mt = params->n_predict;

    char buf1[BNS_TEXT_CAP];
    char buf2[BNS_TEXT_CAP];
    cos_bitnet_server_result_t a1, a2;
    memset(&a1, 0, sizeof(a1));
    memset(&a2, 0, sizeof(a2));
    if (bns_aux_user_chat(prompt, 0.3f, mt, 0, &a1, buf1, sizeof(buf1)) != 0)
        return -1.0f;
    if (bns_aux_user_chat(prompt, 0.9f, mt, 0, &a2, buf2, sizeof(buf2)) != 0)
        return -1.0f;
    const char *t1 = (a1.text != NULL) ? a1.text : "";
    const char *t2 = (a2.text != NULL) ? a2.text : "";
    float ov       = bns_word_jaccard_overlap(t1, t2);
    float sigma_c  = 1.0f - ov;
    return bns_sigma_clamp01(sigma_c);
}

static float bns_sigma_combine_adaptive(float s_lp, float s_v, float s_c) {
    const int have_lp = (s_lp >= 0.0f);
    const int have_v  = (s_v >= 0.0f);
    const int have_c  = (s_c >= 0.0f);

    if (have_lp && bns_env_flag1("COS_BITNET_SIGMA_FULL_BLEND")) {
        float v = have_v ? s_v : 0.5f;
        float c = have_c ? s_c : 0.5f;
        return bns_sigma_clamp01(0.6f * s_lp + 0.2f * v + 0.2f * c);
    }
    if (have_lp) return bns_sigma_clamp01(s_lp);
    if (have_v && have_c) return bns_sigma_clamp01(0.5f * s_v + 0.5f * s_c);
    if (have_v) return bns_sigma_clamp01(s_v);
    if (have_c) return bns_sigma_clamp01(s_c);
    return 0.5f;
}

/** Optional second (and third) inference pass when logprobs are missing. */
static void bns_apply_adaptive_sigma(const char *user_prompt,
                                     const cos_bitnet_server_params_t *params,
                                     cos_bitnet_server_result_t       *out) {
    if (!bns_env_flag1("COS_BITNET_SIGMA_ADAPTIVE")) return;
    if (out == NULL) return;

    const int have_lp = (g_bns_last_tok_n > 0);
    float     s_lp    = have_lp ? out->sigma : -1.0f;
    float     s_v     = -1.0f;
    float     s_c     = -1.0f;

    const char *ans = (out->text != NULL) ? out->text : "";

    if (!have_lp) {
        s_v = bns_sigma_verbal_from_answer(ans, params);
        if (bns_env_flag1("COS_BITNET_SIGMA_CONSISTENCY") && user_prompt != NULL)
            s_c = bns_sigma_consistency_pair(user_prompt, params);
    } else if (bns_env_flag1("COS_BITNET_SIGMA_FULL_BLEND")) {
        s_v = bns_sigma_verbal_from_answer(ans, params);
        if (bns_env_flag1("COS_BITNET_SIGMA_CONSISTENCY") && user_prompt != NULL)
            s_c = bns_sigma_consistency_pair(user_prompt, params);
    }

    float sig = bns_sigma_combine_adaptive(s_lp, s_v, s_c);
    out->sigma      = sig;
    out->mean_sigma = sig;
}

static int parse_completion_response(const char *body, size_t body_len,
                                     cos_bitnet_server_result_t *out) {
    g_bns_last_tok_n = 0;
    const char *end = body + body_len;
    const char *p;
    int msg_has_content       = 0; /* non-empty message.content JSON */
    int msg_has_reasoning     = 0;
    int n_lp                  = 0;

    bns_alloc_scratch();
    g_bns.text_buf[0]      = '\0';
    g_bns.reasoning_buf[0] = '\0';

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
            if (n > 0) msg_has_content = 1;
        } else {
            g_bns.text_buf[0] = '\0';
            out->text = g_bns.text_buf;
        }
    } else {
        g_bns.text_buf[0] = '\0';
        out->text = g_bns.text_buf;
    }
    /* Parse reasoning into a side buffer so we can split logprobs when
     * both reasoning and visible content are present (Qwen thinking). */
    p = json_find_key(scan_from, end, "reasoning_content");
    if (p != NULL && *p == '"') {
        size_t n;
        if (json_parse_string(&p, end, g_bns.reasoning_buf, BNS_TEXT_CAP, &n) == 0
            && n > 0)
            msg_has_reasoning = 1;
    }
    if (!msg_has_reasoning) {
        p = json_find_key(scan_from, end, "reasoning");
        if (p != NULL && *p == '"') {
            size_t n;
            if (json_parse_string(&p, end, g_bns.reasoning_buf, BNS_TEXT_CAP, &n) == 0
                && n > 0)
                msg_has_reasoning = 1;
        }
    }
    /* Ollama /api/chat Qwen3 may expose chain-of-thought as `thinking`. */
    if (!msg_has_reasoning) {
        p = json_find_key(scan_from, end, "thinking");
        if (p != NULL && *p == '"') {
            size_t n;
            if (json_parse_string(&p, end, g_bns.reasoning_buf, BNS_TEXT_CAP, &n) == 0
                && n > 0)
                msg_has_reasoning = 1;
        }
    }
    if (!msg_has_content && msg_has_reasoning) {
        memcpy(g_bns.text_buf, g_bns.reasoning_buf, BNS_TEXT_CAP);
        g_bns.text_buf[BNS_TEXT_CAP - 1] = '\0';
    }
    out->text = g_bns.text_buf;

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
     * Collect natural log of the sampled token into g_bns_lp_scratch. */
    p = json_find_key(body, end, "completion_probabilities");
    if (p != NULL && *p == '[') {
        const char *q = p + 1;
        while (q < end) {
            if (!json_skip_ws(&q, end)) break;
            if (*q == ']') break;
            if (*q != '{') break;
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

            const char *pp = json_find_key(obj_start, obj_end, "probs");
            float lg_store = -80.0f;
            if (pp != NULL && *pp == '[') {
                const char *pq = pp + 1;
                json_skip_ws(&pq, obj_end);
                if (pq < obj_end && *pq == '{') {
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
                        while (pr < fo_end
                               && (*pr == ' ' || *pr == '\t')) pr++;
                        if (pr + 4 <= fo_end && pr[0] == 'n' && pr[1] == 'u'
                            && pr[2] == 'l' && pr[3] == 'l'
                            && (pr + 4 >= fo_end || pr[4] == ',' || pr[4] == '}'
                                || pr[4] == ' ' || pr[4] == '\n' || pr[4] == '\r'
                                || pr[4] == '\t')) {
                            lg_store = -80.0f;
                        } else {
                            double cprob = strtod(pr, NULL);
                            if (cprob > 1e-12 && cprob <= 1.0)
                                lg_store = (float)log(cprob);
                            else
                                lg_store = -80.0f;
                        }
                    } else {
                        const char *lpk =
                            json_find_key(fo_start, fo_end, "logprob");
                        if (lpk != NULL) {
                            double lg = strtod(lpk, NULL);
                            lg_store = (float)lg;
                        }
                    }
                }
            }
            if (n_lp < COS_BITNET_SERVER_MAX_TOKENS)
                g_bns_lp_scratch[n_lp++] = lg_store;

            q = obj_end;
            json_skip_ws(&q, end);
            if (q < end && *q == ',') q++;
        }
    }

    /* OpenAI-style chat: choices[].logprobs.content[].logprob */
    if (n_lp == 0) {
        const char *anchor = bns_find_bytes(body, end, "\"logprobs\"", 10);
        if (anchor != NULL) {
            const char *cq =
                bns_find_bytes(anchor, end, "\"content\"", 9);
            const char *arr_in = NULL;
            if (cq != NULL) {
                const char *v = cq + 9;
                while (v < end && (*v == ' ' || *v == '\t')) v++;
                if (v < end && *v == ':') {
                    v++;
                    while (v < end && (*v == ' ' || *v == '\t' || *v == '\n'
                                       || *v == '\r')) v++;
                    if (v < end && *v == '[') arr_in = v + 1;
                }
            }
            if (arr_in != NULL) {
                const char *q = arr_in;
                while (q < end) {
                    if (!json_skip_ws(&q, end)) break;
                    if (*q == ']') break;
                    if (*q != '{') break;
                    const char *obj_start = q;
                    int depth = 0;
                    const char *obj_end = q;
                    while (obj_end < end) {
                        if (*obj_end == '"') {
                            obj_end++;
                            while (obj_end < end && *obj_end != '"') {
                                if (*obj_end == '\\'
                                    && obj_end + 1 < end) obj_end += 2;
                                else obj_end++;
                            }
                            if (obj_end >= end) break;
                            obj_end++;
                            continue;
                        }
                        if (*obj_end == '{') depth++;
                        if (*obj_end == '}') {
                            depth--;
                            obj_end++;
                            if (depth == 0) break;
                            continue;
                        }
                        obj_end++;
                    }
                    if (obj_end > end) obj_end = end;

                    const char *lpr =
                        json_find_key(obj_start, obj_end, "logprob");
                    if (lpr != NULL) {
                        while (lpr < obj_end && (*lpr == ' ' || *lpr == '\t'))
                            lpr++;
                        if (lpr < obj_end
                            && (*lpr == '-' || (*lpr >= '0' && *lpr <= '9'))) {
                            double lg = strtod(lpr, NULL);
                            if (n_lp < COS_BITNET_SERVER_MAX_TOKENS)
                                g_bns_lp_scratch[n_lp++] = (float)lg;
                        }
                    }

                    q = obj_end;
                    json_skip_ws(&q, end);
                    if (q < end && *q == ',') q++;
                }
            }
        }
    }

    /* Ollama native /api/chat: top-level "logprobs":[{...},…] */
    if (n_lp == 0) {
        p = json_find_key(body, end, "logprobs");
        if (p != NULL) {
            const char *t0 = p;
            json_skip_ws(&t0, end);
            if (t0 < end && *t0 == '[') {
                const char *q = t0 + 1;
                while (q < end) {
                    if (!json_skip_ws(&q, end)) break;
                    if (*q == ']') break;
                    if (*q != '{') break;
                    const char *obj_start = q;
                    int depth = 0;
                    const char *obj_end = q;
                    while (obj_end < end) {
                        if (*obj_end == '"') {
                            obj_end++;
                            while (obj_end < end && *obj_end != '"') {
                                if (*obj_end == '\\'
                                    && obj_end + 1 < end) obj_end += 2;
                                else obj_end++;
                            }
                            if (obj_end >= end) break;
                            obj_end++;
                            continue;
                        }
                        if (*obj_end == '{') depth++;
                        if (*obj_end == '}') {
                            depth--;
                            obj_end++;
                            if (depth == 0) break;
                            continue;
                        }
                        obj_end++;
                    }
                    if (obj_end > end) obj_end = end;

                    const char *lpr =
                        json_find_key(obj_start, obj_end, "logprob");
                    if (lpr != NULL) {
                        while (lpr < obj_end && (*lpr == ' ' || *lpr == '\t'))
                            lpr++;
                        if (lpr < obj_end
                            && (*lpr == '-' || (*lpr >= '0' && *lpr <= '9'))) {
                            double lg = strtod(lpr, NULL);
                            if (n_lp < COS_BITNET_SERVER_MAX_TOKENS)
                                g_bns_lp_scratch[n_lp++] = (float)lg;
                        }
                    }

                    q = obj_end;
                    json_skip_ws(&q, end);
                    if (q < end && *q == ',') q++;
                }
            }
        }
    }

    int n_skip = 0;
    if (msg_has_content && msg_has_reasoning && n_lp > 0) {
        size_t nr = strlen(g_bns.reasoning_buf);
        size_t nc = strlen(g_bns.text_buf);
        double den = (double)nr + (double)nc;
        if (den > 1e-12) {
            long ns = lround((double)n_lp * (double)nr / den);
            if (ns < 0) ns = 0;
            if (ns > n_lp - 1) ns = (long)(n_lp > 0 ? n_lp - 1 : 0);
            n_skip = (int)ns;
        }
    }

    const float *lp_use = g_bns_lp_scratch + n_skip;
    int            n_use = n_lp - n_skip;

    g_bns_last_tok_n = 0;
    for (int i = n_skip; i < n_lp && g_bns_last_tok_n < COS_BITNET_SERVER_MAX_TOKENS;
         i++) {
        float lpv = bns_clamp_lp_for_exp(g_bns_lp_scratch[i]);
        g_bns_last_tok_sigma[g_bns_last_tok_n++] = 1.0f - expf(lpv);
    }

    if (n_lp <= 0) {
        out->sigma      = 0.5f;
        out->mean_sigma = 0.5f;
    } else {
        out->sigma      = bns_sigma_from_logprobs_agg(lp_use, n_use);
        out->mean_sigma = bns_mean_sigma_from_lps(lp_use, n_use);
        if (out->token_count == 0) out->token_count = n_lp;
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
    char body[4096];
    if (g_bns.backend_ollama) {
        if (bns_http_get("/api/tags", body, sizeof(body)) != 0)
            return 0;
        /* Any JSON listing models counts as alive. */
        return (strstr(body, "\"models\"") != NULL
                || strstr(body, "models") != NULL) ? 1 : 0;
    }
    if (bns_http_get("/health", body, sizeof(body)) != 0) {
        /* External Ollama: OpenAI-compat on :11434 has no /health; /api/tags
         * proves the daemon when COS_BITNET_SERVER_EXTERNAL=1. */
        if (g_bns.external
            && bns_http_get("/api/tags", body, sizeof(body)) == 0
            && strstr(body, "models") != NULL)
            return 1;
        return 0;
    }
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

/* POLISH-3: Ctrl-C handler — forwards SIGINT to the child so the
 * whole process tree unwinds cleanly (SQLite flush, pid-file clear,
 * TCP close), then restores default handling so the second Ctrl-C
 * (if anyone is holding it down) actually kills us. */
static void bns_sigint(int sig) {
    (void)sig;
    cos_bitnet_server_shutdown();
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}

static void bns_install_signal_handlers(void) {
    static int installed = 0;
    if (installed) return;
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = bns_sigint;
    sa.sa_flags   = 0;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    /* Ignore SIGPIPE — a closed HTTP socket must not tear us down. */
    signal(SIGPIPE, SIG_IGN);
    installed = 1;
}

/* POLISH-1 PID file helpers: write ~/.cos/llama_server.pid after the
 * child is reaped-by-us healthy, so inspection / external watchdogs
 * (and `cos-health --live`) can pick up the PID without re-forking.
 * Nothing cares if the file is missing or stale — it's advisory. */
static void bns_pidfile_path(char *out, size_t cap) {
    const char *h = getenv("HOME");
    if (h == NULL || h[0] == '\0') {
        struct passwd *pw = getpwuid(getuid());
        h = (pw && pw->pw_dir) ? pw->pw_dir : ".";
    }
    snprintf(out, cap, "%s/.cos/llama_server.pid", h);
}

static void bns_pidfile_write(pid_t pid) {
    char path[1024];
    bns_pidfile_path(path, sizeof path);
    /* Best-effort mkdir ~/.cos if missing. */
    char dir[1024];
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; (void)mkdir(dir, 0700); }
    FILE *f = fopen(path, "w");
    if (f == NULL) return;
    fprintf(f, "%ld\n", (long)pid);
    fclose(f);
}

static void bns_pidfile_clear(void) {
    char path[1024];
    bns_pidfile_path(path, sizeof path);
    (void)unlink(path);
}

/* POLISH-1 / POLISH-3 pre-flight: before we fork+exec, check that the
 * model file and the server binary actually exist.  If not, print a
 * single actionable error line so the user knows exactly what to fix
 * instead of "execvp failed". */
/* Upstream llama.cpp exposes --jinja; older BitNet forks reject it.
 * Probe --help once so Qwen-oriented defaults still spawn on stock builds. */
static int bns_help_has_jinja_flag(const char *exe) {
    char cmd[768];
    if (exe == NULL || exe[0] == '\0') return 0;
    if (strpbrk(exe, ";|&`$()<>\\\"") != NULL) return 0;
    if (snprintf(cmd, sizeof cmd, "\"%s\" --help 2>&1", exe) >= (int)sizeof cmd)
        return 0;
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) return 0;
    char line[512];
    int has = 0;
    size_t total = 0;
    while (fgets(line, sizeof line, fp) != NULL) {
        if (strstr(line, "--jinja") != NULL) {
            has = 1;
            break;
        }
        total += strlen(line);
        if (total > 400000u) break;
    }
    (void)pclose(fp);
    return has;
}

static int bns_preflight(void) {
    static int warned = 0;
    struct stat st;
    if (stat(g_bns.exe, &st) != 0) {
        if (!warned) {
            fprintf(stderr,
                    "cos: llama-server binary not found at %s\n"
                    "  → run: ./scripts/install.sh  (builds third_party/bitnet)\n"
                    "  → or set COS_BITNET_SERVER_EXE=/path/to/llama-server\n",
                    g_bns.exe);
            warned = 1;
        }
        return -1;
    }
    if (stat(g_bns.model, &st) != 0) {
        if (!warned) {
            fprintf(stderr,
                    "cos: GGUF model not found at %s\n"
                    "  → run: bash scripts/real/setup_qwen_gguf.sh  (Qwen) or BitNet setup\n"
                    "  → or set COS_BITNET_SERVER_MODEL=/path/to/model.gguf\n",
                    g_bns.model);
            warned = 1;
        }
        return -1;
    }
    return 0;
}

int cos_bitnet_server_ensure(void) {
    bns_load_config();

    /* Already healthy?  (external mode or pre-existing spawn.) */
    if (cos_bitnet_server_is_healthy()) return 0;

    if (g_bns.backend_ollama) {
        fprintf(stderr,
                "cos: Ollama not reachable at http://%s:%d/api/tags\n"
                "  → run: ollama serve\n"
                "  → pull: ollama pull %s\n",
                g_bns.host, g_bns.http_port,
                env_or("COS_OLLAMA_MODEL", "gemma3:4b"));
        return -1;
    }

    if (g_bns.external) {
        /* Caller promised an external server; propagate failure. */
        fprintf(stderr,
                "cos: COS_BITNET_SERVER_EXTERNAL=1 but no server answers "
                "http://%s:%d/health (llama-server) or /api/tags (Ollama)\n"
                "  → start llama-server or `ollama serve`, or unset the variable\n",
                g_bns.host, g_bns.http_port);
        return -1;
    }

    if (bns_preflight() != 0) return -1;

    const int want_jinja = bns_help_has_jinja_flag(g_bns.exe);

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

        char *argv[64];
        int a = 0;
        argv[a++] = (char *)g_bns.exe;
        argv[a++] = (char *)"--model";
        argv[a++] = (char *)g_bns.model;
        argv[a++] = (char *)"--host";
        argv[a++] = (char *)g_bns.host;
        argv[a++] = (char *)"--port";
        argv[a++] = port_s;
        argv[a++] = (char *)"-c";
        argv[a++] = ctx_s;
        argv[a++] = (char *)"--parallel";
        argv[a++] = (char *)"1";
        argv[a++] = (char *)"-ngl";
        argv[a++] = (char *)"0";
        if (want_jinja) argv[a++] = (char *)"--jinja";
        argv[a++] = (char *)"--temp";
        argv[a++] = (char *)"0.6";
        argv[a++] = (char *)"--top-k";
        argv[a++] = (char *)"20";
        argv[a++] = (char *)"--top-p";
        argv[a++] = (char *)"0.95";
        {
            int bat = env_int_or("COS_LLAMA_BATCH", 512);
            int thr = env_int_or("COS_LLAMA_THREADS", 4);
            if (bat < 1) bat = 512;
            if (thr < 1) thr = 4;
            snprintf(bns_spawn_batch_s, sizeof bns_spawn_batch_s, "%d", bat);
            snprintf(bns_spawn_ubatch_s, sizeof bns_spawn_ubatch_s, "%d", bat);
            snprintf(bns_spawn_threads_s, sizeof bns_spawn_threads_s, "%d", thr);
            const char *no_flash = getenv("COS_LLAMA_NO_FLASH_ATTN");
            if (no_flash == NULL || no_flash[0] != '1')
                argv[a++] = (char *)"--flash-attn";
            argv[a++] = (char *)"-b";
            argv[a++] = bns_spawn_batch_s;
            argv[a++] = (char *)"-ub";
            argv[a++] = bns_spawn_ubatch_s;
            argv[a++] = (char *)"--cache-type-k";
            argv[a++] = (char *)"q8_0";
            argv[a++] = (char *)"--cache-type-v";
            argv[a++] = (char *)"q8_0";
            argv[a++] = (char *)"-t";
            argv[a++] = bns_spawn_threads_s;
            const char *no_mlock = getenv("COS_LLAMA_NO_MLOCK");
            if (no_mlock == NULL || no_mlock[0] != '1')
                argv[a++] = (char *)"--mlock";
        }
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
    bns_install_signal_handlers();
    bns_pidfile_write(pid);

    if (bns_wait_ready(BNS_READY_TIMEOUT_S) != 0) {
        fprintf(stderr,
                "cos: llama-server did not respond to /health within %ds\n"
                "  → check /tmp/cos-bitnet-server.log for the child's stderr\n",
                BNS_READY_TIMEOUT_S);
        cos_bitnet_server_shutdown();
        return -1;
    }
    return 0;
}

void cos_bitnet_server_invalidate_config(void) {
    g_bns.initialized = 0;
    g_bns_consecutive_fail   = 0;
    g_bns_circuit_open_until = 0;
    g_bns_io_cancel          = 0;
}

int cos_bitnet_circuit_check(void)
{
    time_t now = time(NULL);
    if (g_bns_circuit_open_until != 0 && now < g_bns_circuit_open_until)
        return -1;
    if (g_bns_circuit_open_until != 0 && now >= g_bns_circuit_open_until) {
        g_bns_circuit_open_until = 0;
        g_bns_consecutive_fail   = 0;
    }
    return 0;
}

void cos_bitnet_circuit_update(int success)
{
    if (success) {
        g_bns_consecutive_fail = 0;
        return;
    }
    g_bns_consecutive_fail++;
    if (g_bns_consecutive_fail >= BNS_CIRCUIT_FAIL_MAX) {
        g_bns_circuit_open_until = time(NULL) + (time_t)BNS_CIRCUIT_PAUSE_SEC;
        fprintf(stderr,
                "[circuit] Ollama HTTP: %d consecutive failures "
                "-> pause %ds\n",
                g_bns_consecutive_fail, BNS_CIRCUIT_PAUSE_SEC);
    }
}

void cos_bitnet_server_io_cancel_request(void) { g_bns_io_cancel = 1; }

void cos_bitnet_server_io_cancel_clear(void) { g_bns_io_cancel = 0; }

void cos_bitnet_server_shutdown(void) {
    if (g_bns.child > 0) {
        /* POLISH-1: honour --keep-server (COS_BITNET_KEEP_SERVER=1).
         * Useful for dev loops where `cos chat --once ...` is invoked
         * repeatedly and paying the 2-3 s cold-start each time is
         * wasteful.  The pid file remains so the next invocation can
         * reconnect via is_healthy(). */
        const char *keep = getenv("COS_BITNET_KEEP_SERVER");
        if (keep != NULL && keep[0] == '1') {
            g_bns.child = 0;
            return;
        }
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
        bns_pidfile_clear();
    }
}

void cos_bitnet_server_diag(const char **out_host, int *out_port,
                            int *out_pid) {
    bns_load_config();
    if (out_host) *out_host = g_bns.host;
    if (out_port) *out_port = g_bns.port;
    if (out_pid)  *out_pid  = (int)g_bns.child;
}

/* --- Ollama HTTP backend (COS_INFERENCE_BACKEND=ollama) ------------ */

static int bns_ollama_chat_complete(const char                       *prompt,
                                    const cos_bitnet_server_params_t *params,
                                    cos_bitnet_server_result_t       *out)
{
    if (cos_bitnet_server_ensure() != 0)
        return -2;

    cos_bitnet_server_io_cancel_clear();
    if (cos_bitnet_circuit_check() != 0) {
        fprintf(stderr,
                "[circuit] Ollama HTTP paused after recent failures; "
                "skipping request (no socket I/O)\n");
        return -4;
    }

    int top_lp = (params && params->n_probs > 0) ? params->n_probs
                                                 : g_bns.n_probs_default;
    if (top_lp < 1) top_lp = 1;
    if (top_lp > 3) top_lp = 3;

    int         n_predict = params && params->n_predict > 0 ? params->n_predict
                                                            : g_bns.n_predict_default;
    float       temperature = params ? params->temperature : 0.0f;
    float       temp_o      = (temperature > 0.0f)
                                  ? temperature
                                  : bns_env_float_default("COS_TEMPERATURE", 0.7f);
    if (temp_o < 0.0f) temp_o = 0.0f;
    if (temp_o > 2.0f) temp_o = 2.0f;
    const char *syspr = params ? params->system_prompt : NULL;
    const char *model = getenv("COS_OLLAMA_MODEL");
    if (model == NULL || model[0] == '\0')
        model = getenv("COS_BITNET_CHAT_MODEL");
    if (model == NULL || model[0] == '\0')
        model = "gemma3:4b";

    char *b = g_bns.req_buf;
    const char *enc_prompt = prompt;
    {
        /* Qwen3: append " /no_think" unless disabled; skip for Gemma models. */
        int is_gemma = 0;
        for (const char *q = model; q != NULL && *q; ++q) {
            if ((q[0] == 'g' || q[0] == 'G') && (q[1] == 'e' || q[1] == 'E')
                && (q[2] == 'm' || q[2] == 'M')) {
                is_gemma = 1;
                break;
            }
        }
        const char *nt = getenv("COS_OLLAMA_APPEND_NO_THINK");
        int append_nt = (nt == NULL || nt[0] != '0') && !is_gemma;
        if (append_nt && prompt != NULL) {
            size_t pl = strlen(prompt);
            if (pl + 12u < 4096u) {
                snprintf(b, 4096u, "%s /no_think", prompt);
                enc_prompt = b;
            }
        }
    }
    const size_t JSON_CAP = BNS_REQ_CAP - 4096;
    char *jb = b + 4096;
    size_t w = 0;
    int    n;

    n = snprintf(jb + w, JSON_CAP - w, "{\"model\":");
    if (n < 0 || (size_t)n >= JSON_CAP - w) return -3;
    w += (size_t)n;
    {
        size_t mw = json_encode_string(jb + w, JSON_CAP - w, model);
        if (mw == 0) return -3;
        w += mw;
    }
    n = snprintf(jb + w, JSON_CAP - w, ",\"stream\":false,\"messages\":[");
    if (n < 0 || (size_t)n >= JSON_CAP - w) return -3;
    w += (size_t)n;

    if (syspr != NULL && syspr[0] != '\0') {
        n = snprintf(jb + w, JSON_CAP - w, "{\"role\":\"system\",\"content\":");
        if (n < 0) return -3;
        w += (size_t)n;
        size_t sw = json_encode_string(jb + w, JSON_CAP - w, syspr);
        if (sw == 0) return -3;
        w += sw;
        n = snprintf(jb + w, JSON_CAP - w, "},");
        if (n < 0) return -3;
        w += (size_t)n;
    }
    n = snprintf(jb + w, JSON_CAP - w, "{\"role\":\"user\",\"content\":");
    if (n < 0) return -3;
    w += (size_t)n;
    {
        size_t sw = json_encode_string(jb + w, JSON_CAP - w, enc_prompt);
        if (sw == 0) return -3;
        w += sw;
    }
    n = snprintf(jb + w, JSON_CAP - w,
                  "}],\"max_tokens\":%d,\"logprobs\":true,\"top_logprobs\":%d,"
                  "\"temperature\":%.3f",
                  n_predict, top_lp, (double)temp_o);
    if (n < 0 || (size_t)n >= JSON_CAP - w) return -3;
    w += (size_t)n;
    {
        int top_ko = bns_env_int_clamp("COS_TOP_K", 0, 0, 1000000);
        if (top_ko > 0) {
            n = snprintf(jb + w, JSON_CAP - w, ",\"top_k\":%d", top_ko);
            if (n < 0 || (size_t)n >= JSON_CAP - w) return -3;
            w += (size_t)n;
        }
    }
    n = snprintf(jb + w, JSON_CAP - w, "}");
    if (n < 0 || (size_t)n >= JSON_CAP - w) return -3;
    w += (size_t)n;

    double      t0 = now_ms();
    static char json_tmp[BNS_REQ_CAP];
    if (w >= sizeof(json_tmp)) return -3;
    memcpy(json_tmp, jb, w);

    const char *body = NULL;
    size_t      blen = 0;
    int         rc   = bns_http_post_json("/v1/chat/completions", json_tmp, w,
                                          &body, &blen);
    if (rc == -17) {
        cos_bitnet_server_io_cancel_clear();
        memset(out, 0, sizeof(*out));
        out->sigma      = 1.0f;
        out->mean_sigma = 1.0f;
        out->elapsed_ms = now_ms() - t0;
        return -17;
    }
    if (rc == -5) {
        fprintf(stderr, "[timeout] Ollama inference exceeded %ds\n",
                bns_io_timeout_sec());
        cos_bitnet_circuit_update(0);
        memset(out, 0, sizeof(*out));
        out->sigma      = 1.0f;
        out->mean_sigma = 1.0f;
        out->elapsed_ms = now_ms() - t0;
        return 0;
    }
    if (rc != 0 || body == NULL) {
        cos_bitnet_circuit_update(0);
        return -4;
    }

    rc = parse_completion_response(body, blen, out);
    out->elapsed_ms = now_ms() - t0;
    out->cost_eur   = 0.0001;

    {
        const char *end = body + blen;
        const char *pe  = json_find_key(body, end, "eval_count");
        if (pe != NULL && out->token_count <= 0)
            out->token_count = atoi(pe);
    }
    /* OpenAI-compat: if the response carried no token logprobs, use neutral σ. */
    if (g_bns_last_tok_n == 0) {
        float sd = strtof(env_or("COS_OLLAMA_DEFAULT_SIGMA", "0.5"), NULL);
        if (sd < 0.0f) sd = 0.0f;
        if (sd > 1.0f) sd = 1.0f;
        out->sigma      = sd;
        out->mean_sigma = sd;
    }

    bns_apply_adaptive_sigma(enc_prompt, params, out);
    if (rc == 0)
        cos_bitnet_circuit_update(1);
    else
        cos_bitnet_circuit_update(0);
    return rc;
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
    cos_bitnet_server_clear_ttft();
    bns_load_config();
    if (g_bns.backend_ollama)
        return bns_ollama_chat_complete(prompt, params, out);
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
    const char *enc_prompt = prompt;
    {
        const char *nt = getenv("COS_OLLAMA_APPEND_NO_THINK");
        if (nt != NULL && nt[0] == '1' && prompt != NULL) {
            size_t pl = strlen(prompt);
            if (pl + 12u < 4096u) {
                snprintf(b, 4096u, "%s /no_think", prompt);
                enc_prompt = b;
            }
        }
    }
    const size_t JSON_CAP = BNS_REQ_CAP - 4096;
    char *jb = b + 4096;
    size_t w = 0;
    int n;
    n = snprintf(jb + w, JSON_CAP - w, "{\"model\":");
    if (n < 0 || (size_t)n >= JSON_CAP - w) return -3;
    w += (size_t)n;
    {
        const char *mid = getenv("COS_BITNET_CHAT_MODEL");
        if (mid == NULL || mid[0] == '\0')
            mid = bns_default_openai_compat_chat_model();
        size_t mw = json_encode_string(jb + w, JSON_CAP - w, mid);
        if (mw == 0) return -3;
        w += mw;
    }
    n = snprintf(jb + w, JSON_CAP - w, ",\"messages\":[");
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
        size_t sw = json_encode_string(jb + w, JSON_CAP - w, enc_prompt);
        if (sw == 0) return -3; w += sw;
    }
    n = snprintf(jb + w, JSON_CAP - w,
                 "}],\"max_tokens\":%d,\"logprobs\":true,"
                 "\"top_logprobs\":%d",
                 n_predict, n_probs);
    if (n < 0) return -3; w += (size_t)n;
    {
        const char *cp = getenv("COS_BITNET_CHAT_CACHE_PROMPT");
        if (cp != NULL && strcmp(cp, "1") == 0) {
            n = snprintf(jb + w, JSON_CAP - w, ",\"cache_prompt\":true");
            if (n < 0) return -3; w += (size_t)n;
        }
    }

    {
        float eff_temp = temperature;
        const char *et = getenv("COS_TEMPERATURE");
        if (eff_temp <= 0.0f && et != NULL && et[0] != '\0')
            eff_temp = strtof(et, NULL);
        if (eff_temp > 0.0f) {
            n = snprintf(jb + w, JSON_CAP - w, ",\"temperature\":%.3f",
                         (double)eff_temp);
            if (n < 0) return -3;
            w += (size_t)n;
        }
    }
    {
        const char *tp = getenv("COS_TOP_P");
        if (tp != NULL && tp[0] != '\0') {
            float tpf = strtof(tp, NULL);
            if (tpf >= 0.0f && tpf <= 1.0f) {
                n = snprintf(jb + w, JSON_CAP - w, ",\"top_p\":%.3f",
                             (double)tpf);
                if (n < 0) return -3;
                w += (size_t)n;
            }
        }
    }
    {
        const char *tk = getenv("COS_TOP_K");
        if (tk != NULL && tk[0] != '\0') {
            int tki = atoi(tk);
            if (tki > 0) {
                n = snprintf(jb + w, JSON_CAP - w, ",\"top_k\":%d", tki);
                if (n < 0) return -3;
                w += (size_t)n;
            }
        }
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
    if (rc == -17) {
        cos_bitnet_server_io_cancel_clear();
        return -17;
    }
    if (rc == -5) {
        fprintf(stderr, "[timeout] inference exceeded %ds, σ=1.0\n",
                bns_io_timeout_sec());
        memset(out, 0, sizeof(*out));
        out->sigma      = 1.0f;
        out->mean_sigma = 1.0f;
        out->elapsed_ms = now_ms() - t0;
        return 0;
    }
    if (rc != 0 || body == NULL)
        return -4;

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
    bns_apply_adaptive_sigma(prompt, params, out);
    return rc;
}

/* =====================================================================
 * DEV-4: streaming completion (/completion + "stream":true)
 *
 * Transport: the response is HTTP/1.1 chunked-transfer SSE.  We
 * decode the chunk framing on the fly and accumulate clean body
 * bytes into a small scratch; whenever a complete `data:` event
 * (terminated by blank-line \n\n or \r\n\r\n) is present we extract
 * it, parse (content, stop, completion_probabilities[0].probs[0].prob),
 * invoke the caller's `token_cb`, and compact the scratch.
 *
 * JSON shape per event (llama.cpp native /completion streaming):
 *   {"content":" Paris","stop":false,"id_slot":0,...,
 *    "completion_probabilities":[{"content":" Paris",
 *        "probs":[{"tok_str":" Paris","prob":0.88},...]}]}
 *
 * On the terminal event `"stop":true` the server also includes
 * `tokens_predicted`, `stopped_eos`, `stopped_limit` — we mirror
 * these into `out`.
 *
 * Why /completion and not /v1/chat/completions:
 *   Chat streaming DROPS probabilities (deltas carry only content)
 *   on the upstream llama.cpp we ship.  We need per-token σ, so we
 *   use the native endpoint and accept the trade-off that the chat
 *   template is not applied automatically.  DEV-6 (Codex as system
 *   prompt) reintroduces instruction-following via system_prompt.
 * ===================================================================== */

#define BNS_STREAM_EVBUF 16384   /* max one SSE event json size */
#define BNS_STREAM_NETBUF 4096

typedef struct {
    int  in_chunk;         /* 0 = reading size line, 1 = reading bytes,
                              2 = reading trailing \r\n */
    long chunk_rem;        /* bytes remaining in current chunk */
    char size_line[32];
    size_t size_len;
} bns_chunked_t;

/* Decode one byte of a chunked-transfer response.  When a clean
 * payload byte is emitted, writes it into *out_byte and returns 1.
 * Returns 0 if the byte was framing (consumed silently), -1 on end-
 * of-stream (terminal 0-length chunk), -2 on protocol error. */
static int bns_chunked_step(bns_chunked_t *s, char c, char *out_byte) {
    if (s->in_chunk == 0) {
        /* accumulating hex size line until \r\n */
        if (c == '\r') return 0;
        if (c == '\n') {
            s->size_line[s->size_len] = '\0';
            long n = strtol(s->size_line, NULL, 16);
            s->size_len = 0;
            if (n < 0) return -2;
            if (n == 0) return -1;  /* terminal chunk */
            s->chunk_rem = n;
            s->in_chunk  = 1;
            return 0;
        }
        if (s->size_len + 1 >= sizeof s->size_line) return -2;
        s->size_line[s->size_len++] = c;
        return 0;
    }
    if (s->in_chunk == 1) {
        /* payload byte */
        *out_byte = c;
        if (--s->chunk_rem == 0) s->in_chunk = 2;
        return 1;
    }
    /* in_chunk == 2: swallow \r\n then back to size line. */
    if (c == '\n') s->in_chunk = 0;
    return 0;
}

/* Scan a single SSE event JSON for "stop":true / false.  Returns
 * 1 on stop=true, 0 on stop=false or absent. */
static int sse_event_is_terminal(const char *ev, size_t elen) {
    const char *p = json_find_key(ev, ev + elen, "stop");
    if (p == NULL) return 0;
    return (strncmp(p, "true", 4) == 0) ? 1 : 0;
}

/* Build the JSON request body for /completion streaming.  Writes
 * into `dst` (cap `cap`).  Returns bytes written, or 0 on overflow. */
static size_t build_stream_request_json(char *dst, size_t cap,
                                        const char *prompt,
                                        const char *system_prompt,
                                        int n_predict, int n_probs,
                                        float temperature, int seed,
                                        const char *stop_word) {
    size_t w = 0;
    int n;
    n = snprintf(dst + w, cap - w, "{\"prompt\":");
    if (n < 0) return 0; w += (size_t)n;

    if (system_prompt != NULL && system_prompt[0] != '\0') {
        /* Manual prepend: "system\n\nuser" single string.  Full
         * Atlantean Codex is ~31KB so we use a generously-sized
         * static buffer; anything larger than 64KiB gets silently
         * truncated to preserve the stream request shape.  The
         * user-visible symptom of over-truncation is a low-context
         * model response, which surfaces as high σ → RETHINK. */
        static char combo[65536];
        int cn = snprintf(combo, sizeof combo, "%s\n\n%s",
                          system_prompt, prompt);
        if (cn <= 0) return 0;
        if ((size_t)cn >= sizeof combo) cn = (int)sizeof combo - 1;
        size_t sw = json_encode_string(dst + w, cap - w, combo);
        if (sw == 0) return 0; w += sw;
    } else {
        size_t sw = json_encode_string(dst + w, cap - w, prompt);
        if (sw == 0) return 0; w += sw;
    }

    n = snprintf(dst + w, cap - w,
                 ",\"n_predict\":%d,\"n_probs\":%d,\"stream\":true",
                 n_predict, n_probs);
    if (n < 0) return 0; w += (size_t)n;
    {
        const char *cp = getenv("COS_BITNET_CHAT_CACHE_PROMPT");
        if (cp != NULL && strcmp(cp, "1") == 0) {
            n = snprintf(dst + w, cap - w, ",\"cache_prompt\":true");
            if (n < 0) return 0; w += (size_t)n;
        }
    }

    if (temperature > 0.0f) {
        n = snprintf(dst + w, cap - w, ",\"temperature\":%.3f",
                     (double)temperature);
        if (n < 0) return 0; w += (size_t)n;
    }
    if (seed >= 0) {
        n = snprintf(dst + w, cap - w, ",\"seed\":%d", seed);
        if (n < 0) return 0; w += (size_t)n;
    }
    if (stop_word != NULL && stop_word[0] != '\0') {
        n = snprintf(dst + w, cap - w, ",\"stop\":[");
        if (n < 0) return 0; w += (size_t)n;
        size_t sw = json_encode_string(dst + w, cap - w, stop_word);
        if (sw == 0) return 0; w += sw;
        n = snprintf(dst + w, cap - w, "]");
        if (n < 0) return 0; w += (size_t)n;
    }
    n = snprintf(dst + w, cap - w, "}");
    if (n < 0) return 0; w += (size_t)n;
    return w;
}

int cos_bitnet_server_complete_stream(
    const char                       *prompt,
    const cos_bitnet_server_params_t *params,
    cos_bitnet_server_token_cb_t      token_cb,
    void                             *cb_ctx,
    cos_bitnet_server_result_t       *out) {
    if (prompt == NULL || out == NULL) return -1;
    memset(out, 0, sizeof(*out));
    bns_load_config();
    if (g_bns.backend_ollama) {
        fputs("cos: streaming completion is not available when "
              "COS_INFERENCE_BACKEND=ollama\n",
              stderr);
        return -1;
    }
    if (cos_bitnet_server_ensure() != 0) return -2;

    int   n_predict = params && params->n_predict > 0
                          ? params->n_predict : g_bns.n_predict_default;
    int   n_probs   = params && params->n_probs   > 0
                          ? params->n_probs   : g_bns.n_probs_default;
    int   seed      = params ? params->seed : -1;
    float temp      = params ? params->temperature : 0.0f;
    const char *stp = params ? params->stop_word    : NULL;
    const char *sp  = params ? params->system_prompt: NULL;

    static char json_tmp[BNS_REQ_CAP];
    size_t jlen = build_stream_request_json(json_tmp, sizeof json_tmp,
                                            prompt, sp, n_predict,
                                            n_probs, temp, seed, stp);
    if (jlen == 0) return -3;

    int fd = bns_tcp_connect();
    if (fd < 0)
        return -4;

    int hn = snprintf(g_bns.req_buf, BNS_REQ_CAP,
                      "POST /completion HTTP/1.1\r\n"
                      "Host: %s:%d\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: %zu\r\n"
                      "Accept: text/event-stream\r\n"
                      "Connection: close\r\n\r\n",
                      g_bns.host, g_bns.port, jlen);
    if (hn < 0 || (size_t)hn + jlen >= BNS_REQ_CAP) { close(fd); return -3; }
    memcpy(g_bns.req_buf + hn, json_tmp, jlen);
    if (bns_send_all(fd, g_bns.req_buf, (size_t)hn + jlen) != 0) {
        close(fd); return -5;
    }

    double t0 = now_ms();
    cos_bitnet_server_clear_ttft();
    cos_bitnet_server_io_cancel_clear();

    /* --- read + parse phase --- */
    char   netbuf[BNS_STREAM_NETBUF];
    char   body[BNS_STREAM_EVBUF];
    size_t blen = 0;
    int    headers_done = 0;
    int    is_chunked   = 0;
    int    status_ok    = 0;
    char   hbuf[2048];     /* header accumulator */
    size_t hlen = 0;
    bns_chunked_t cs = {0};

    float  max_sigma = 0.0f;
    double sum_sigma = 0.0;
    int    n_tokens  = 0;
    int    aborted   = 0;
    int    hit_terminal = 0;
    int    io_timed_out = 0;
    int    io_user_cancel = 0;

    /* text accumulator (owned by module; caller sees out->text) */
    g_bns.text_buf[0] = '\0';
    size_t text_w = 0;

    for (;;) {
        ssize_t r = recv(fd, netbuf, sizeof netbuf, 0);
        if (r < 0) {
            if (errno == EINTR) {
                if (g_bns_io_cancel) {
                    io_user_cancel = 1;
                    break;
                }
                continue;
            }
            {
                int timed = 0;
#ifdef EAGAIN
                if (errno == EAGAIN)
                    timed = 1;
#endif
#ifdef EWOULDBLOCK
                if (errno == EWOULDBLOCK)
                    timed = 1;
#endif
#ifdef ETIMEDOUT
                if (errno == ETIMEDOUT)
                    timed = 1;
#endif
                if (timed)
                    io_timed_out = 1;
            }
            break;
        }
        if (r == 0) break;

        size_t i = 0;
        while (i < (size_t)r) {
            if (!headers_done) {
                char c = netbuf[i++];
                if (hlen + 1 < sizeof hbuf) hbuf[hlen++] = c;
                if (hlen >= 4
                    && hbuf[hlen-4] == '\r' && hbuf[hlen-3] == '\n'
                    && hbuf[hlen-2] == '\r' && hbuf[hlen-1] == '\n') {
                    hbuf[hlen] = '\0';
                    status_ok  = (strncmp(hbuf, "HTTP/1.1 200", 12) == 0);
                    is_chunked = (strcasestr(hbuf,
                                             "Transfer-Encoding: chunked") != NULL);
                    headers_done = 1;
                    if (!status_ok) { close(fd); return -6; }
                }
                continue;
            }

            /* body byte */
            char c = netbuf[i++];
            char out_c;
            int bs;
            if (is_chunked) {
                bs = bns_chunked_step(&cs, c, &out_c);
                if (bs == -1) { /* end of stream */ i = (size_t)r; break; }
                if (bs == -2) { close(fd); return -7; }
                if (bs == 0) continue; /* framing consumed */
            } else {
                out_c = c;
            }

            /* accumulate + dispatch */
            if (blen + 1 >= sizeof body) {
                /* event too large; drop first quarter */
                memmove(body, body + sizeof body / 4,
                        blen - sizeof body / 4);
                blen -= sizeof body / 4;
            }
            body[blen++] = out_c;

            /* Look for complete event boundary: "\n\n" or "\r\n\r\n". */
            if (blen >= 2 && body[blen-2] == '\n' && body[blen-1] == '\n') {
                /* isolate this event text */
                body[blen] = '\0';
                const char *ev = body;
                size_t elen = blen;

                /* Expect "data: " prefix. */
                const char *dp = strstr(ev, "data:");
                if (dp != NULL) {
                    dp += 5;
                    while (*dp == ' ' || *dp == '\t') dp++;
                    size_t dlen = elen - (size_t)(dp - ev);

                    /* "[DONE]" marker? */
                    if (strncmp(dp, "[DONE]", 6) == 0) {
                        hit_terminal = 1;
                    } else {
                        /* Parse JSON content + probs + stop. */
                        char tok_buf[2048];
                        tok_buf[0] = '\0';
                        const char *cp = json_find_key(dp, dp + dlen, "content");
                        if (cp != NULL && *cp == '"') {
                            size_t n; const char *q = cp;
                            if (json_parse_string(&q, dp + dlen,
                                                  tok_buf, sizeof tok_buf,
                                                  &n) != 0) {
                                tok_buf[0] = '\0';
                            }
                        }

                        /* σ from first completion_probabilities entry. */
                        float sigma_tok = 1.0f;
                        const char *cp_arr = json_find_key(dp, dp + dlen,
                                                           "completion_probabilities");
                        if (cp_arr != NULL && *cp_arr == '[') {
                            const char *pq = cp_arr + 1;
                            json_skip_ws(&pq, dp + dlen);
                            if (pq < dp + dlen && *pq == '{') {
                                const char *obj = pq;
                                /* Find nested probs → first entry → prob. */
                                const char *probs = json_find_key(obj, dp + dlen, "probs");
                                if (probs != NULL && *probs == '[') {
                                    const char *eq = probs + 1;
                                    json_skip_ws(&eq, dp + dlen);
                                    if (eq < dp + dlen && *eq == '{') {
                                        const char *pr = json_find_key(eq, dp + dlen, "prob");
                                        if (pr != NULL) {
                                            float v = (float)strtod(pr, NULL);
                                            if (v >= 0.0f && v <= 1.0f) {
                                                sigma_tok = 1.0f - v;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        int is_stop = sse_event_is_terminal(dp, dlen);

                        /* Append token text to accumulator. */
                        if (tok_buf[0] != '\0') {
                            if (g_bns_last_ttft_ms < 0.0)
                                g_bns_last_ttft_ms = now_ms() - t0;
                            size_t tlen = strlen(tok_buf);
                            if (text_w + tlen + 1 < BNS_TEXT_CAP) {
                                memcpy(g_bns.text_buf + text_w, tok_buf, tlen);
                                text_w += tlen;
                                g_bns.text_buf[text_w] = '\0';
                            }
                        }

                        /* Only count non-empty tokens toward σ. */
                        if (tok_buf[0] != '\0') {
                            n_tokens++;
                            sum_sigma += sigma_tok;
                            if (sigma_tok > max_sigma) max_sigma = sigma_tok;
                        }

                        /* Fire callback per event.  Empty tok_buf on the
                         * terminal event is expected; cb can distinguish
                         * via is_last. */
                        if (token_cb != NULL) {
                            int rc = token_cb(tok_buf, sigma_tok,
                                              is_stop, cb_ctx);
                            if (rc != 0) {
                                aborted = 1;
                                hit_terminal = 1;
                            }
                        }

                        if (is_stop) {
                            /* Harvest terminal-event stats. */
                            const char *tp = json_find_key(dp, dp + dlen, "tokens_predicted");
                            if (tp != NULL) out->token_count = atoi(tp);
                            const char *se = json_find_key(dp, dp + dlen, "stopped_eos");
                            if (se != NULL && strncmp(se, "true", 4) == 0)
                                out->stopped_eos = 1;
                            const char *sl = json_find_key(dp, dp + dlen, "stopped_limit");
                            if (sl != NULL && strncmp(sl, "true", 4) == 0)
                                out->stopped_limit = 1;
                            hit_terminal = 1;
                        }
                    }
                }
                blen = 0;
                if (hit_terminal) { i = (size_t)r; break; }
            }
            /* Also handle \r\n\r\n terminators. */
            if (blen >= 4
                && body[blen-4] == '\r' && body[blen-3] == '\n'
                && body[blen-2] == '\r' && body[blen-1] == '\n') {
                blen -= 2;  /* let the \n\n path catch it next iter */
            }
        }

        if (hit_terminal) break;
    }
    close(fd);

    if (io_user_cancel) {
        cos_bitnet_server_io_cancel_clear();
        memset(out, 0, sizeof(*out));
        out->sigma       = 1.0f;
        out->mean_sigma  = 1.0f;
        out->text        = g_bns.text_buf;
        out->elapsed_ms  = now_ms() - t0;
        out->cost_eur    = 0.0001;
        out->stopped_limit = 1;
        return -17;
    }

    /* Finalize summary. */
    if (io_timed_out) {
        fprintf(stderr, "[timeout] inference exceeded %ds, σ=1.0\n",
                bns_io_timeout_sec());
        g_bns.text_buf[0] = '\0';
        max_sigma = 1.0f;
        n_tokens  = 0;
        sum_sigma = 0.0;
    }
    if (out->token_count == 0) out->token_count = n_tokens;
    out->sigma      = (max_sigma > 0.0f) ? max_sigma
                                         : (n_tokens == 0 ? 1.0f : 0.0f);
    out->mean_sigma = (n_tokens > 0) ? (float)(sum_sigma / n_tokens) : 1.0f;
    out->text       = g_bns.text_buf;
    out->elapsed_ms = now_ms() - t0;
    out->cost_eur   = 0.0001;
    if (aborted && !out->stopped_limit && !out->stopped_eos) {
        out->stopped_limit = 1;  /* caller-aborted → treat as truncation */
    }
    return 0;
}

typedef struct {
    FILE *out;
} bns_stream_file_ctx_t;

static int bns_stream_file_cb(const char *tok_text, float sigma,
                              int is_last, void *ctx)
{
    bns_stream_file_ctx_t *c = (bns_stream_file_ctx_t *)ctx;
    (void)sigma;
    (void)is_last;
    if (c != NULL && c->out != NULL && tok_text != NULL && tok_text[0] != '\0')
        fputs(tok_text, c->out);
    if (c != NULL && c->out != NULL)
        fflush(c->out);
    return 0;
}

int cos_bitnet_stream_response(const char *prompt, const char *system_prompt,
                               FILE *output, char *full_response, int max_len,
                               float *ttft_ms)
{
    if (prompt == NULL || full_response == NULL || max_len <= 0)
        return -1;
    cos_bitnet_server_params_t par;
    memset(&par, 0, sizeof(par));
    par.system_prompt = system_prompt;
    cos_bitnet_server_result_t outr;
    bns_stream_file_ctx_t ctx = { output };
    int rc = cos_bitnet_server_complete_stream(prompt, &par,
                                               bns_stream_file_cb, &ctx,
                                               &outr);
    full_response[0] = '\0';
    if (outr.text != NULL)
        snprintf(full_response, (size_t)max_len, "%s", outr.text);
    if (ttft_ms != NULL) {
        double g = cos_bitnet_server_last_ttft_ms();
        *ttft_ms = (float)((g >= 0.0) ? g : outr.elapsed_ms);
    }
    return rc;
}

int cos_bitnet_server_copy_last_token_sigmas(float *dst, int cap,
                                             int *n_out) {
    if (n_out == NULL) return -1;
    int n = g_bns_last_tok_n;
    *n_out = n;
    if (dst == NULL || cap <= 0) return 0;
    int c = (n < cap) ? n : cap;
    if (c > 0)
        memcpy(dst, g_bns_last_tok_sigma, (size_t)c * sizeof(float));
    return 0;
}

char *cos_bitnet_query_temp_with_options(int port, const char *prompt,
                                         const char *system,
                                         float temperature, int max_tokens)
{
    if (prompt == NULL)
        return NULL;

    char        prev_buf[96];
    int         had_prev = 0;
    const char *prev = NULL;

    if (port > 0) {
        prev = getenv("COS_BITNET_SERVER_PORT");
        if (prev != NULL && prev[0] != '\0') {
            (void)snprintf(prev_buf, sizeof prev_buf, "%s", prev);
            had_prev = 1;
        }
        char portbuf[32];
        (void)snprintf(portbuf, sizeof portbuf, "%d", port);
        if (setenv("COS_BITNET_SERVER_PORT", portbuf, 1) != 0)
            return NULL;
        cos_bitnet_server_invalidate_config();
    }

    int np = max_tokens;
    if (np < 8)
        np = 8;
    if (np > 512)
        np = 512;

    cos_bitnet_server_params_t pp;
    memset(&pp, 0, sizeof(pp));
    pp.n_predict     = np;
    pp.n_probs       = 3;
    pp.temperature   = temperature;
    pp.system_prompt = system;
    pp.seed          = -1;

    cos_bitnet_server_result_t rr;
    memset(&rr, 0, sizeof(rr));
    int rc = cos_bitnet_server_complete(prompt, &pp, &rr);

    char *out = NULL;
    if (rc == 0 && rr.text != NULL && rr.text[0] != '\0')
        out = strdup(rr.text);

    if (port > 0) {
        if (had_prev)
            (void)setenv("COS_BITNET_SERVER_PORT", prev_buf, 1);
        else
            (void)unsetenv("COS_BITNET_SERVER_PORT");
        cos_bitnet_server_invalidate_config();
    }
    return out;
}

char *cos_bitnet_query_temp(int port, const char *prompt,
                            const char *system, float temperature)
{
    return cos_bitnet_query_temp_with_options(port, prompt, system,
                                              temperature, 96);
}

char *cos_bitnet_query_model(int port, const char *prompt, const char *system,
                             const char *model, float temperature,
                             int max_tokens)
{
    if (prompt == NULL || model == NULL || model[0] == '\0')
        return NULL;

    char        prev_port_buf[96];
    char        prev_bcm_buf[512];
    char        prev_oll_buf[512];
    int         had_port = 0, had_bcm = 0, had_oll = 0;
    const char *prev_port = NULL;
    const char *prev_bcm  = NULL;
    const char *prev_oll  = NULL;

    if (port > 0) {
        prev_port = getenv("COS_BITNET_SERVER_PORT");
        if (prev_port != NULL && prev_port[0] != '\0') {
            (void)snprintf(prev_port_buf, sizeof prev_port_buf, "%s", prev_port);
            had_port = 1;
        }
        char portbuf[32];
        (void)snprintf(portbuf, sizeof portbuf, "%d", port);
        if (setenv("COS_BITNET_SERVER_PORT", portbuf, 1) != 0)
            return NULL;
        cos_bitnet_server_invalidate_config();
    }

    prev_bcm = getenv("COS_BITNET_CHAT_MODEL");
    if (prev_bcm != NULL && prev_bcm[0] != '\0') {
        (void)snprintf(prev_bcm_buf, sizeof prev_bcm_buf, "%s", prev_bcm);
        had_bcm = 1;
    }
    prev_oll = getenv("COS_OLLAMA_MODEL");
    if (prev_oll != NULL && prev_oll[0] != '\0') {
        (void)snprintf(prev_oll_buf, sizeof prev_oll_buf, "%s", prev_oll);
        had_oll = 1;
    }
    if (setenv("COS_BITNET_CHAT_MODEL", model, 1) != 0
        || setenv("COS_OLLAMA_MODEL", model, 1) != 0) {
        if (port > 0) {
            if (had_port)
                (void)setenv("COS_BITNET_SERVER_PORT", prev_port_buf, 1);
            else
                (void)unsetenv("COS_BITNET_SERVER_PORT");
            cos_bitnet_server_invalidate_config();
        }
        return NULL;
    }
    cos_bitnet_server_invalidate_config();

    int np = max_tokens;
    if (np < 8)
        np = 8;
    if (np > 512)
        np = 512;

    cos_bitnet_server_params_t pp;
    memset(&pp, 0, sizeof(pp));
    pp.n_predict     = np;
    pp.n_probs       = 3;
    pp.temperature   = temperature;
    pp.system_prompt = system;
    pp.seed          = -1;

    cos_bitnet_server_result_t rr;
    memset(&rr, 0, sizeof(rr));
    int rc = cos_bitnet_server_complete(prompt, &pp, &rr);

    char *out = NULL;
    if (rc == 0 && rr.text != NULL && rr.text[0] != '\0')
        out = strdup(rr.text);

    if (had_bcm)
        (void)setenv("COS_BITNET_CHAT_MODEL", prev_bcm_buf, 1);
    else
        (void)unsetenv("COS_BITNET_CHAT_MODEL");
    if (had_oll)
        (void)setenv("COS_OLLAMA_MODEL", prev_oll_buf, 1);
    else
        (void)unsetenv("COS_OLLAMA_MODEL");

    if (port > 0) {
        if (had_port)
            (void)setenv("COS_BITNET_SERVER_PORT", prev_port_buf, 1);
        else
            (void)unsetenv("COS_BITNET_SERVER_PORT");
    }
    cos_bitnet_server_invalidate_config();
    return out;
}
