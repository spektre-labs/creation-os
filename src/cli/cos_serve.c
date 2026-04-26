/* cos-serve — minimal HTTP API for σ-gate (enterprise gateway stub).
 *
 * Endpoints (HTTP/1.1, JSON bodies):
 *   POST /v1/gate   { "prompt", "model"? }
 *   POST /v1/sigma — alias of /v1/gate (OpenAI-shaped body with "prompt")
 *   POST /v1/verify { "text", "model"? } — per-sentence semantic-entropy σ (JSON array)
 *   GET  /v1/health
 *   GET  /v1/audit/{audit_id}
 *
 * Append-only audit: ~/.cos/audit/YYYY-MM-DD.jsonl
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "pipeline.h"
#include "codex.h"
#include "engram.h"
#include "engram_persist.h"
#include "sovereign.h"
#include "agent.h"
#include "stub_gen.h"
#include "escalation.h"
#include "conformal.h"
#include "constitution.h"
#include "license_attest.h"
#include "audit_log.h"
#include "../../include/cos_version.h"
#include "../vendor/picohttpparser.h"
#include "bitnet_server.h"
#include "cos_verify_claims.h"
#include "crypto/sha256.h"
#include "cache/response_cache.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SERVE_REQ_MAX   262144
#define SERVE_PATH_MAX  512
#define SERVE_JSON_MAX  65536

typedef struct {
    cos_pipeline_config_t cfg;
    cos_sigma_codex_t     codex;
    int                   have_codex;
    cos_sigma_engram_entry_t slots[64];
    cos_sigma_engram_t    engram;
    cos_sigma_sovereign_t sv;
    cos_sigma_agent_t     ag;
    cos_engram_persist_t *persist;
    cos_cli_generate_ctx_t genctx;

    int     conformal_loaded;
    float   tau;
    float   alpha;
    char    domain[COS_CONFORMAL_NAME_MAX];
    char    calib_path[512];

    time_t  started;
    char    auroc_ctx[128];
    float   health_auroc;
} cos_serve_state_t;

static const char *action_label(cos_sigma_action_t a)
{
    switch (a) {
    case COS_SIGMA_ACTION_ACCEPT:  return "ACCEPT";
    case COS_SIGMA_ACTION_RETHINK: return "RETHINK";
    case COS_SIGMA_ACTION_ABSTAIN: return "ABSTAIN";
    default: return "?";
    }
}

static const char *json_skip_ws(const char *p)
{
    while (*p && isspace((unsigned char)*p))
        ++p;
    return p;
}

static const char *json_find_key(const char *json, const char *key)
{
    if (!json || !key)
        return NULL;
    size_t klen = strlen(key);
    const char *p = json;
    while ((p = strchr(p, '"')) != NULL) {
        const char *q = p + 1;
        if (strncmp(q, key, klen) == 0 && q[klen] == '"') {
            q += klen + 1;
            q = json_skip_ws(q);
            if (*q == ':') {
                q = json_skip_ws(q + 1);
                return q;
            }
        }
        ++p;
    }
    return NULL;
}

static int json_copy_string(const char *v, char *dst, size_t cap)
{
    if (!v || *v != '"' || !dst || cap == 0)
        return -1;
    ++v;
    size_t w = 0;
    while (*v && *v != '"') {
        if (w + 1 >= cap)
            return -1;
        if (*v == '\\' && v[1]) {
            char e = v[1];
            switch (e) {
            case 'n': dst[w++] = '\n'; break;
            case 't': dst[w++] = '\t'; break;
            case '"': dst[w++] = '"'; break;
            case '\\': dst[w++] = '\\'; break;
            default: dst[w++] = e; break;
            }
            v += 2;
        } else
            dst[w++] = *v++;
    }
    dst[w] = '\0';
    return (*v == '"') ? 0 : -1;
}

static void sha256_hex(const void *data, size_t len, char out65[65])
{
    cos_sha256_hex((const uint8_t *)data, len, out65);
}

static void gen_audit_id(char out17[17])
{
    static const char *hx = "0123456789abcdef";
    uint8_t b[8];
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        if (read(fd, b, sizeof b) != (ssize_t)sizeof b) {
            for (size_t i = 0; i < sizeof b; ++i)
                b[i] = (uint8_t)(rand() >> (i % 8));
        }
        close(fd);
    } else {
        for (size_t i = 0; i < sizeof b; ++i)
            b[i] = (uint8_t)(rand() >> (i % 8));
    }
    for (int i = 0; i < 8; ++i) {
        out17[i * 2]     = hx[(b[i] >> 4) & 15];
        out17[i * 2 + 1] = hx[b[i] & 15];
    }
    out17[16] = '\0';
}

/* Pilot default: if Ollama answers on 127.0.0.1:11434, use COS_INFERENCE_BACKEND=ollama
 * and pick COS_OLLAMA_MODEL from /api/tags (prefer gemma3:4b) without requiring env vars. */
static int serve_tcp_probe_port(uint16_t port, int timeout_ms)
{
    struct sockaddr_in a;
    int                s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        return -1;
    int fl = fcntl(s, F_GETFL, 0);
    if (fl < 0 || fcntl(s, F_SETFL, fl | O_NONBLOCK) < 0) {
        close(s);
        return -1;
    }
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port   = htons(port);
    if (inet_pton(AF_INET, "127.0.0.1", &a.sin_addr) != 1) {
        close(s);
        return -1;
    }
    if (connect(s, (struct sockaddr *)&a, sizeof a) < 0 && errno != EINPROGRESS) {
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

static int serve_http_get_ollama_tags(char *resp, size_t cap)
{
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
    a.sin_port   = htons(11434);
    if (inet_pton(AF_INET, "127.0.0.1", &a.sin_addr) != 1) {
        close(s);
        return -1;
    }
    if (connect(s, (struct sockaddr *)&a, sizeof a) != 0) {
        close(s);
        return -1;
    }
    const char *req =
        "GET /api/tags HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    if (send(s, req, strlen(req), 0) < 0) {
        close(s);
        return -1;
    }
    size_t n = 0;
    while (n + 1 < cap) {
        ssize_t r = recv(s, resp + n, cap - n - 1, 0);
        if (r <= 0)
            break;
        n += (size_t)r;
    }
    resp[n] = '\0';
    close(s);
    return 0;
}

static void serve_pick_model_from_tags_body(const char *http, char *model, size_t mcap)
{
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
    /* Prefer any gemma3 tag */
    {
        const char *p = body;
        while ((p = strstr(p, "\"name\":\"gemma3")) != NULL) {
            p += 9;
            size_t w = 0;
            while (*p && *p != '"' && w + 1 < mcap)
                model[w++] = *p++;
            model[w] = '\0';
            if (model[0])
                return;
        }
    }
    /* First "name":"…" in JSON */
    {
        const char *p = strstr(body, "\"name\":\"");
        if (p) {
            p += 8;
            size_t w = 0;
            while (*p && *p != '"' && w + 1 < mcap)
                model[w++] = *p++;
            model[w] = '\0';
        }
    }
}

static void serve_autodetect_ollama_env(void)
{
    const char *be = getenv("COS_INFERENCE_BACKEND");
    if (!be || !be[0]) {
        if (serve_tcp_probe_port(11434, 800) == 0)
            (void)setenv("COS_INFERENCE_BACKEND", "ollama", 1);
    }
    if (getenv("COS_INFERENCE_BACKEND") != NULL
        && strcmp(getenv("COS_INFERENCE_BACKEND"), "ollama") == 0) {
        if (getenv("COS_BITNET_SERVER_EXTERNAL") == NULL
            || getenv("COS_BITNET_SERVER_EXTERNAL")[0] == '\0')
            (void)setenv("COS_BITNET_SERVER_EXTERNAL", "1", 1);
        if (getenv("COS_BITNET_SERVER_PORT") == NULL
            || getenv("COS_BITNET_SERVER_PORT")[0] == '\0')
            (void)setenv("COS_BITNET_SERVER_PORT", "11434", 1);
    }
    if (getenv("COS_INFERENCE_BACKEND") == NULL
        || strcmp(getenv("COS_INFERENCE_BACKEND"), "ollama") != 0)
        return;
    if (!getenv("COS_OLLAMA_HOST") || !getenv("COS_OLLAMA_HOST")[0])
        (void)setenv("COS_OLLAMA_HOST", "127.0.0.1", 1);
    if (!getenv("COS_OLLAMA_PORT") || !getenv("COS_OLLAMA_PORT")[0])
        (void)setenv("COS_OLLAMA_PORT", "11434", 1);

    int have_m = getenv("COS_OLLAMA_MODEL") && getenv("COS_OLLAMA_MODEL")[0];
    int have_b = getenv("COS_BITNET_CHAT_MODEL") && getenv("COS_BITNET_CHAT_MODEL")[0];
    if (have_m && have_b)
        return;
    char tags[65536];
    char m[128];
    m[0] = '\0';
    if (serve_http_get_ollama_tags(tags, sizeof tags) == 0)
        serve_pick_model_from_tags_body(tags, m, sizeof m);
    if (!m[0])
        snprintf(m, sizeof m, "gemma3:4b");
    if (!have_m)
        (void)setenv("COS_OLLAMA_MODEL", m, 1);
    if (!have_b) {
        const char *om = getenv("COS_OLLAMA_MODEL");
        if (om && om[0])
            (void)setenv("COS_BITNET_CHAT_MODEL", om, 1);
        else
            (void)setenv("COS_BITNET_CHAT_MODEL", m, 1);
    }
}

static int serve_load_conformal(cos_serve_state_t *st, const char *override)
{
    const char *p = (override && override[0]) ? override : st->calib_path;
    if (!p || !p[0])
        return -1;
    cos_conformal_bundle_t b;
    if (cos_conformal_read_bundle_json(p, &b) != 0)
        return -1;
    for (int i = 0; i < b.n_reports; ++i) {
        if (!b.reports[i].valid)
            continue;
        st->tau   = b.reports[i].tau;
        st->alpha = b.reports[i].alpha;
        snprintf(st->domain, sizeof st->domain, "%s", b.reports[i].domain);
        st->conformal_loaded = 1;
        st->cfg.tau_accept  = st->tau;
        st->cfg.tau_rethink = st->tau + 0.25f;
        if (st->cfg.tau_rethink > 0.99f)
            st->cfg.tau_rethink = 0.99f;
        if (st->cfg.tau_rethink <= st->cfg.tau_accept)
            st->cfg.tau_rethink = st->cfg.tau_accept + 0.05f;
        st->genctx.icl_exemplar_max_sigma = st->cfg.tau_accept;
        return 0;
    }
    return -1;
}

static void serve_state_init(cos_serve_state_t *st)
{
    memset(st, 0, sizeof(*st));
    st->started = time(NULL);
    snprintf(st->auroc_ctx, sizeof st->auroc_ctx, "%s",
             getenv("COS_SERVE_AUROC_CONTEXT") ? getenv("COS_SERVE_AUROC_CONTEXT")
                                                : "graded export (see benchmarks/graded/)");
    {
        const char *ah = getenv("COS_SERVE_HEALTH_AUROC");
        st->health_auroc = (ah && ah[0]) ? (float)atof(ah) : 0.812f;
    }

    int rc = cos_sigma_codex_load(NULL, &st->codex);
    st->have_codex = (rc == 0);

    memset(st->slots, 0, sizeof(st->slots));
    cos_sigma_engram_init(&st->engram, st->slots, 64, 0.25f, 200, 20);
    if (cos_engram_persist_open(NULL, &st->persist) == 0)
        cos_engram_persist_load(st->persist, &st->engram, 64);

    cos_sigma_sovereign_init(&st->sv, 0.85f);
    cos_sigma_agent_init(&st->ag, 0.80f, 0.10f);

    cos_sigma_pipeline_config_defaults(&st->cfg);
    st->cfg.tau_accept  = 0.40f;
    st->cfg.tau_rethink = 0.60f;
    st->cfg.max_rethink = 3;
    st->cfg.mode        = COS_PIPELINE_MODE_HYBRID;
    st->cfg.codex       = st->have_codex ? &st->codex : NULL;
    st->cfg.engram      = &st->engram;
    st->cfg.sovereign   = &st->sv;
    st->cfg.agent       = &st->ag;
    st->cfg.generate    = cos_cli_chat_generate;

    memset(&st->genctx, 0, sizeof(st->genctx));
    st->genctx.magic                   = COS_CLI_GENERATE_CTX_MAGIC;
    st->genctx.codex                   = st->have_codex ? &st->codex : NULL;
    st->genctx.persist                 = st->persist;
    st->genctx.icl_exemplar_max_sigma  = st->cfg.tau_accept;
    st->cfg.generate_ctx               = &st->genctx;
    st->cfg.escalate_ctx               = &st->genctx;
    st->cfg.escalate                   = cos_cli_escalate_api;
    if (st->persist != NULL) {
        st->cfg.on_engram_store     = cos_engram_persist_store;
        st->cfg.on_engram_store_ctx = st->persist;
    }

    st->conformal_loaded = 0;
    const char *env = getenv("COS_CALIBRATION");
    if (env && env[0])
        snprintf(st->calib_path, sizeof st->calib_path, "%s", env);
    else {
        const char *home = getenv("HOME");
        if (home)
            snprintf(st->calib_path, sizeof st->calib_path,
                     "%s/.cos/calibration.json", home);
    }
    (void)serve_load_conformal(st, NULL);

    serve_autodetect_ollama_env();

    if (cos_cli_use_bitnet_http() != 0)
        st->cfg.mode = COS_PIPELINE_MODE_LOCAL_ONLY;

    (void)cos_constitution_init("data/codex/atlantean_codex_compact.txt");
}

static void serve_state_free(cos_serve_state_t *st)
{
    if (!st)
        return;
    cos_sigma_pipeline_free_engram_values(&st->engram);
    cos_sigma_codex_free(&st->codex);
    cos_engram_persist_close(st->persist);
}

static void codex_sha256_field(const cos_serve_state_t *st, char *out, size_t outcap)
{
    char hx[65];
    if (st->have_codex && st->codex.bytes && st->codex.size > 0) {
        sha256_hex(st->codex.bytes, st->codex.size, hx);
        snprintf(out, outcap, "sha256:%s", hx);
    } else {
        snprintf(out, outcap, "%s",
                 "sha256:0000000000000000000000000000000000000000000000000000000000000000");
    }
}

static int http_send_all(int fd, const char *s, size_t n)
{
    size_t off = 0;
    while (off < n) {
        ssize_t w = send(fd, s + off, n - off, 0);
        if (w <= 0)
            return -1;
        off += (size_t)w;
    }
    return 0;
}

static int http_reply_json(int fd, int code, const char *json)
{
    char hdr[256];
    size_t jl = strlen(json);
    int    n = snprintf(hdr, sizeof hdr,
                        "HTTP/1.1 %d %s\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: %zu\r\n"
                        "Connection: close\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "\r\n",
                        code, code == 200 ? "OK" : "Error", jl);
    if (n < 0 || (size_t)n >= sizeof hdr)
        return -1;
    if (http_send_all(fd, hdr, (size_t)n) != 0)
        return -1;
    return http_send_all(fd, json, jl);
}

static int scan_audit_id(const char *id, char *out_line, size_t cap)
{
    const char *home = getenv("HOME");
    if (!home || !home[0])
        home = "/tmp";
    /* Scan last 3 days of append-only audit files. */
    for (int day = 0; day < 3; ++day) {
        time_t    t = time(NULL) - (time_t)day * 86400;
        struct tm tmv;
        if (gmtime_r(&t, &tmv) == NULL)
            continue;
        char date[32];
        char path[640];
        strftime(date, sizeof date, "%Y-%m-%d", &tmv);
        snprintf(path, sizeof path, "%s/.cos/audit/%s.jsonl", home, date);
        FILE *fp = fopen(path, "r");
        if (!fp)
            continue;
        while (fgets(out_line, (int)cap, fp) != NULL) {
            char pat[48];
            snprintf(pat, sizeof pat, "\"audit_id\":\"%s\"", id);
            if (strstr(out_line, pat) != NULL) {
                fclose(fp);
                return 0;
            }
        }
        fclose(fp);
    }
    return -1;
}

static int phr_eq(const char *s, size_t slen, const char *lit)
{
    size_t ll = strlen(lit);
    return slen == ll && memcmp(s, lit, ll) == 0;
}

static int handle_request(cos_serve_state_t *st, int cfd, char *req, size_t reqlen)
{
    const char *method;
    size_t        method_len;
    const char *path;
    size_t        path_len;
    int           minor_version;
    struct phr_header headers[64];
    size_t        num_headers = sizeof(headers) / sizeof(headers[0]);

    int pret = phr_parse_request(req, reqlen, &method, &method_len, &path, &path_len,
                                 &minor_version, headers, &num_headers, 0);
    if (pret == -2)
        return http_reply_json(cfd, 400, "{\"error\":\"partial_request\"}");
    if (pret < 0)
        return http_reply_json(cfd, 400, "{\"error\":\"bad_request\"}");

    const char *body = req + pret;
    size_t      body_len = reqlen - (size_t)pret;
    for (size_t hi = 0; hi < num_headers; ++hi) {
        if (!headers[hi].name || headers[hi].name_len != 14)
            continue;
        if (strncasecmp(headers[hi].name, "Content-Length", 14) != 0)
            continue;
        if (!headers[hi].value)
            break;
        char tmp[48];
        if (headers[hi].value_len >= sizeof tmp)
            break;
        memcpy(tmp, headers[hi].value, headers[hi].value_len);
        tmp[headers[hi].value_len] = '\0';
        long cl = strtol(tmp, NULL, 10);
        if (cl >= 0 && (size_t)cl <= body_len)
            body_len = (size_t)cl;
        break;
    }

    char pathz[SERVE_PATH_MAX];
    {
        size_t pl = path_len < sizeof(pathz) - 1 ? path_len : sizeof(pathz) - 1;
        memcpy(pathz, path, pl);
        pathz[pl] = '\0';
    }

    if (phr_eq(method, method_len, "OPTIONS")) {
        static const char opt[] =
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Content-Length: 0\r\n"
            "\r\n";
        return http_send_all(cfd, opt, sizeof opt - 1u);
    }

    if (phr_eq(method, method_len, "GET") && strcmp(pathz, "/v1/health") == 0) {
        char js[SERVE_JSON_MAX];
        const char *m = getenv("COS_OLLAMA_MODEL");
        if (!m || !m[0])
            m = getenv("COS_BITNET_CHAT_MODEL");
        if (!m || !m[0])
            m = "default";
        double up = difftime(time(NULL), st->started);
        snprintf(js, sizeof js,
                 "{\"status\":\"ok\",\"model\":\"%s\",\"version\":\"%s\","
                 "\"auroc\":%.3f,\"uptime\":%.0f,"
                 "\"tau_accept\":%.4f,\"conformal_loaded\":%s}",
                 m, COS_VERSION_STRING, (double)st->health_auroc, up,
                 (double)st->cfg.tau_accept,
                 st->conformal_loaded ? "true" : "false");
        return http_reply_json(cfd, 200, js);
    }

    if (phr_eq(method, method_len, "GET") && strncmp(pathz, "/v1/audit/", 10) == 0) {
        const char *id = pathz + 10;
        if (!id || !id[0] || strlen(id) > 32)
            return http_reply_json(cfd, 404, "{\"error\":\"not_found\"}");
        char line[8192];
        if (scan_audit_id(id, line, sizeof line) != 0)
            return http_reply_json(cfd, 404, "{\"error\":\"not_found\"}");
        /* line is raw JSON object line — strip newline */
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        return http_reply_json(cfd, 200, line);
    }

    if (!phr_eq(method, method_len, "POST") && !phr_eq(method, method_len, "PUT"))
        return http_reply_json(cfd, 404, "{\"error\":\"not_found\"}");

    if (strcmp(pathz, "/v1/gate") != 0 && strcmp(pathz, "/v1/sigma") != 0
        && strcmp(pathz, "/v1/verify") != 0)
        return http_reply_json(cfd, 404, "{\"error\":\"not_found\"}");

    if (!body || body_len == 0)
        return http_reply_json(cfd, 400, "{\"error\":\"empty_body\"}");

    char jb[SERVE_JSON_MAX];
    if (body_len >= sizeof jb)
        return http_reply_json(cfd, 400, "{\"error\":\"body_too_large\"}");
    memcpy(jb, body, body_len);
    jb[body_len] = '\0';

    char prompt[16384];
    char model[128];
    prompt[0] = '\0';
    model[0]  = '\0';

    {
        const char *jp = json_find_key(jb, "prompt");
        const char *jt = json_find_key(jb, "text");
        const char *jm = json_find_key(jb, "model");
        if (strcmp(pathz, "/v1/verify") == 0) {
            if (jt && *jt == '"') {
                if (json_copy_string(jt, prompt, sizeof prompt) != 0)
                    return http_reply_json(cfd, 400, "{\"error\":\"bad_json\"}");
            } else
                return http_reply_json(cfd, 400, "{\"error\":\"missing_text\"}");
        } else {
            if (jp && *jp == '"') {
                if (json_copy_string(jp, prompt, sizeof prompt) != 0)
                    return http_reply_json(cfd, 400, "{\"error\":\"bad_json\"}");
            } else
                return http_reply_json(cfd, 400, "{\"error\":\"missing_prompt\"}");
        }
        if (jm && *jm == '"' && json_copy_string(jm, model, sizeof model) != 0)
            return http_reply_json(cfd, 400, "{\"error\":\"bad_json\"}");
    }

    if (model[0])
        (void)setenv("COS_BITNET_CHAT_MODEL", model, 1);

    if (strcmp(pathz, "/v1/verify") == 0) {
        if (!cos_bitnet_server_is_healthy())
            return http_reply_json(cfd, 503,
                                   "{\"error\":\"ollama_unavailable\"}");
        char vjs[SERVE_JSON_MAX];
        int  vrc = cos_verify_claims_sentences_json(prompt, vjs, sizeof vjs);
        if (vrc == -2)
            return http_reply_json(cfd, 400, "{\"error\":\"text_too_large\"}");
        if (vrc != 0)
            return http_reply_json(cfd, 500, "{\"error\":\"verify_encode_failed\"}");
        return http_reply_json(cfd, 200, vjs);
    }

    if (!cos_bitnet_server_is_healthy())
        return http_reply_json(cfd, 503, "{\"error\":\"ollama_unavailable\"}");

    {
        const char *mod_cache =
            model[0] ? model : getenv("COS_BITNET_CHAT_MODEL");
        if (mod_cache == NULL || mod_cache[0] == '\0')
            mod_cache = "gemma3:4b";
        cos_cache_entry_t cce;
        if (cos_cache_lookup(prompt, mod_cache, &cce) == 0) {
            char          audit_id[17];
            char          ph[65], rh[65];
            char          phf[96], rhf[96];
            char          esc[8192];
            size_t        w = 0;
            const char   *cresp = (cce.response[0] != '\0') ? cce.response : "";
            gen_audit_id(audit_id);
            sha256_hex(prompt, strlen(prompt), ph);
            sha256_hex(cresp, strlen(cresp), rh);
            snprintf(phf, sizeof phf, "sha256:%s", ph);
            snprintf(rhf, sizeof rhf, "sha256:%s", rh);
            for (const char *s = cresp; *s && w + 2 < sizeof esc; ++s) {
                unsigned char c = (unsigned char)*s;
                if (c == '"' || c == '\\') {
                    esc[w++] = '\\';
                    esc[w++] = (char)c;
                } else if (c == '\n') {
                    esc[w++] = '\\';
                    esc[w++] = 'n';
                } else if (c < 0x20u)
                    w += (size_t)snprintf(esc + w, sizeof esc - w, "\\u%04x", c);
                else
                    esc[w++] = (char)c;
            }
            esc[w] = '\0';
            float temp = 0.3f;
            {
                const char *te = getenv("COS_SERVE_DEFAULT_TEMPERATURE");
                if (te && te[0])
                    temp = (float)atof(te);
            }
            {
                char codexh[96];
                codex_sha256_field(st, codexh, sizeof codexh);
                (void)cos_audit_append_serve_row(
                    audit_id, phf, prompt, (double)cce.sigma, (double)cce.sigma,
                    cce.action[0] ? cce.action : "ACCEPT", mod_cache, rhf, 0.0,
                    temp, st->cfg.tau_accept, st->cfg.tau_rethink, 0, codexh);
            }
            char receipt_disp[24];
            snprintf(receipt_disp, sizeof receipt_disp, "%.16s", rh);
            char js[SERVE_JSON_MAX];
            snprintf(
                js, sizeof js,
                "{\"response\":\"%s\",\"sigma\":%.6f,\"action\":\"%s\","
                "\"receipt\":\"%s\",\"model\":\"%s\","
                "\"gated\":true,\"audit_id\":\"%s\",\"latency_ms\":%.1f,"
                "\"cached\":true}",
                esc, (double)cce.sigma,
                (cce.action[0] != '\0') ? cce.action : "ACCEPT", receipt_disp,
                mod_cache, audit_id, 0.0);
            return http_reply_json(cfd, 200, js);
        }
    }

    cos_pipeline_result_t r;
    memset(&r, 0, sizeof r);
    clock_t t0 = clock();
    int     prc = cos_sigma_pipeline_run(&st->cfg, prompt, &r);
    clock_t t1 = clock();
    double  ms = (double)(t1 - t0) * 1000.0 / (double)CLOCKS_PER_SEC;
    if (ms < 0)
        ms = 0;

    if (prc != 0)
        return http_reply_json(cfd, 502, "{\"error\":\"pipeline_failed\"}");

    char audit_id[17];
    char ph[65], rh[65];
    gen_audit_id(audit_id);
    sha256_hex(prompt, strlen(prompt), ph);
    const char *resp = r.response ? r.response : "";
    sha256_hex(resp, strlen(resp), rh);
    char phf[96], rhf[96];
    snprintf(phf, sizeof phf, "sha256:%s", ph);
    snprintf(rhf, sizeof rhf, "sha256:%s", rh);

    float temp = 0.3f;
    {
        const char *te = getenv("COS_SERVE_DEFAULT_TEMPERATURE");
        if (te && te[0])
            temp = (float)atof(te);
    }

    {
        char codexh[96];
        codex_sha256_field(st, codexh, sizeof codexh);
        const char *mod = model[0] ? model : "default";
        (void)cos_audit_append_serve_row(audit_id, phf, prompt, (double)r.sigma,
                                         (double)r.sigma, action_label(r.final_action),
                                         mod, rhf, ms, temp, st->cfg.tau_accept,
                                         st->cfg.tau_rethink, 0, codexh);
    }

    const char *mod_json = model[0] ? model : getenv("COS_BITNET_CHAT_MODEL");
    if (mod_json == NULL || mod_json[0] == '\0')
        mod_json = "gemma3:4b";

    char receipt_disp[24];
    snprintf(receipt_disp, sizeof receipt_disp, "%.16s", rh);

    char js[SERVE_JSON_MAX];
    /* Only /v1/gate and /v1/sigma reach here */
    {
        char esc[8192];
        size_t w = 0;
        const char *s = resp;
        for (; *s && w + 2 < sizeof esc; ++s) {
            unsigned char c = (unsigned char)*s;
            if (c == '"' || c == '\\') {
                esc[w++] = '\\';
                esc[w++] = (char)c;
            } else if (c == '\n') {
                esc[w++] = '\\';
                esc[w++] = 'n';
            } else if (c < 0x20u)
                w += (size_t)snprintf(esc + w, sizeof esc - w, "\\u%04x", c);
            else
                esc[w++] = (char)c;
        }
        esc[w] = '\0';
        snprintf(js, sizeof js,
                 "{\"response\":\"%s\",\"sigma\":%.6f,\"action\":\"%s\","
                 "\"receipt\":\"%s\",\"model\":\"%s\","
                 "\"gated\":true,\"audit_id\":\"%s\",\"latency_ms\":%.1f}",
                 esc, (double)r.sigma, action_label(r.final_action), receipt_disp,
                 mod_json, audit_id, ms);
    }
    cos_cache_store(prompt, mod_json, resp, r.sigma, action_label(r.final_action));
    return http_reply_json(cfd, 200, js);
}

static int read_full_request(int fd, char *buf, size_t cap, size_t *outlen)
{
    size_t n = 0;
    for (;;) {
        if (n + 4096 >= cap)
            return -1;
        ssize_t r = recv(fd, buf + n, cap - n - 1, 0);
        if (r <= 0)
            break;
        n += (size_t)r;
        buf[n] = '\0';
        char *end = strstr(buf, "\r\n\r\n");
        if (end) {
            size_t hlen = (size_t)(end - buf + 4);
            const char *cl = strstr(buf, "Content-Length:");
            if (!cl || cl > end) {
                *outlen = n;
                return 0;
            }
            cl += strlen("Content-Length:");
            while (*cl == ' ' || *cl == '\t')
                ++cl;
            long bodylen = strtol(cl, NULL, 10);
            if (bodylen < 0 || bodylen > (long)(cap - hlen - 8))
                return -1;
            while (n < hlen + (size_t)bodylen) {
                ssize_t r2 = recv(fd, buf + n, cap - n - 1, 0);
                if (r2 <= 0)
                    return -1;
                n += (size_t)r2;
            }
            buf[n] = '\0';
            *outlen = n;
            return 0;
        }
    }
    *outlen = n;
    return (n > 0) ? 0 : -1;
}

int cos_serve_main(int argc, char **argv)
{
    int         port        = 3001;
    const char *bind_ip    = "127.0.0.1";
    int         self_test  = 0;

    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    /* argv[0] may be the program name (./cos-serve) or the first flag (cos serve --port …). */
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
            port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--bind") == 0 && i + 1 < argc)
            bind_ip = argv[++i];
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr,
                    "cos-serve — HTTP σ-gate API\n"
                    "  --port N     listen port (default 3001)\n"
                    "  --bind ADDR  default 127.0.0.1\n"
                    "  --self-test  bind, probe /v1/health, exit\n");
            return 0;
        } else if (strcmp(argv[i], "--self-test") == 0) {
            self_test = 1;
            port        = 0; /* kernel assigns */
        }
    }

    cos_serve_state_t st;
    if (!self_test)
        serve_state_init(&st);

    int ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls < 0) {
        perror("cos-serve: socket");
        if (!self_test)
            serve_state_free(&st);
        return 1;
    }
    int one = 1;
    (void)setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)port);
    addr.sin_addr.s_addr = inet_addr(bind_ip);
    if (bind(ls, (struct sockaddr *)&addr, sizeof addr) != 0) {
        perror("cos-serve: bind");
        close(ls);
        if (!self_test)
            serve_state_free(&st);
        return 1;
    }
    if (listen(ls, 8) != 0) {
        perror("cos-serve: listen");
        close(ls);
        if (!self_test)
            serve_state_free(&st);
        return 1;
    }

    if (self_test) {
        struct sockaddr_in sa;
        socklen_t          sl = sizeof sa;
        if (getsockname(ls, (struct sockaddr *)&sa, &sl) != 0) {
            perror("cos-serve: getsockname");
            close(ls);
            return 1;
        }
        int p = (int)ntohs(sa.sin_port);
        pid_t ch = fork();
        if (ch < 0) {
            perror("cos-serve: fork");
            close(ls);
            return 1;
        }
        if (ch == 0) {
            cos_serve_state_t cst;
            serve_state_init(&cst);
            struct sockaddr_in ca;
            socklen_t          clen = sizeof ca;
            int                afd = accept(ls, (struct sockaddr *)&ca, &clen);
            if (afd < 0)
                _exit(1);
            char  buf[SERVE_REQ_MAX];
            size_t blen = 0;
            if (read_full_request(afd, buf, sizeof buf, &blen) == 0)
                (void)handle_request(&cst, afd, buf, blen);
            close(afd);
            serve_state_free(&cst);
            _exit(0);
        }
        int cfd = socket(AF_INET, SOCK_STREAM, 0);
        if (cfd < 0) {
            kill(ch, SIGTERM);
            waitpid(ch, NULL, 0);
            close(ls);
            return 1;
        }
        if (connect(cfd, (struct sockaddr *)&sa, sizeof sa) != 0) {
            perror("cos-serve: connect self-test");
            close(cfd);
            kill(ch, SIGTERM);
            waitpid(ch, NULL, 0);
            close(ls);
            return 1;
        }
        const char *req =
            "GET /v1/health HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
        if (send(cfd, req, strlen(req), 0) < 0) {
            close(cfd);
            kill(ch, SIGTERM);
            waitpid(ch, NULL, 0);
            close(ls);
            return 1;
        }
        char    buf[8192];
        size_t  tot = 0;
        for (int k = 0; k < 32 && tot + 1 < sizeof buf; ++k) {
            ssize_t nr = recv(cfd, buf + tot, sizeof buf - tot - 1, 0);
            if (nr <= 0)
                break;
            tot += (size_t)nr;
            buf[tot] = '\0';
            if (strstr(buf, "\"status\":\"ok\"") != NULL)
                break;
        }
        close(cfd);
        (void)waitpid(ch, NULL, 0);
        close(ls);
        if (tot == 0 || strstr(buf, "\"status\":\"ok\"") == NULL) {
            fprintf(stderr, "cos-serve: --self-test failed\n");
            return 1;
        }
        fprintf(stderr, "cos-serve: --self-test OK (port %d)\n", p);
        return 0;
    }

    fprintf(stderr, "cos-serve: listening on http://%s:%d/\n", bind_ip, port);

    for (;;) {
        struct sockaddr_in ca;
        socklen_t          clen = sizeof ca;
        int cfd = accept(ls, (struct sockaddr *)&ca, &clen);
        if (cfd < 0) {
            if (errno == EINTR)
                continue;
            perror("cos-serve: accept");
            break;
        }
        char  buf[SERVE_REQ_MAX];
        size_t blen = 0;
        if (read_full_request(cfd, buf, sizeof buf, &blen) == 0)
            (void)handle_request(&st, cfd, buf, blen);
        close(cfd);
    }
    close(ls);
    serve_state_free(&st);
    return 0;
}

#ifdef COS_SERVE_MAIN
int main(int argc, char **argv) { return cos_serve_main(argc, argv); }
#endif
