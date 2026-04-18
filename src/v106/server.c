/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v106 σ-Server — POSIX HTTP loop.
 *
 * Single-threaded accept-handle-close per connection.  This is a
 * deliberate design choice: the v101 bridge holds a llama_context
 * which is not thread-safe, and one-prompt-at-a-time matches the
 * "local LLM on a laptop" target use case.
 *
 * The handler is a switch on method + path prefix:
 *     GET  /health               → "ok\n"
 *     GET  /v1/models            → {data:[{id,object,owned_by}]}
 *     GET  /v1/sigma-profile     → last σ snapshot JSON
 *     POST /v1/chat/completions  → completion (SSE if stream=true)
 *     POST /v1/completions       → text completion
 *     POST /v1/reason            → v111.2 multi-candidate σ-reasoning
 *     GET  /                     → web/index.html (v108)
 *     GET  /{static}             → simple mime-typed static file
 *
 * Routing is literal prefix matching.  For production we'd want a
 * router, but at six routes the switch is clearer than a table.
 */
#include "server.h"

#include "../v101/bridge.h"
#include "../v111/reason.h"
#include "../v112/tools.h"
#include "../v113/sandbox.h"
#include "../v114/swarm.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#if defined(_WIN32)
int cos_v106_server_run(const cos_v106_config_t *cfg)
{
    (void)cfg;
    fprintf(stderr, "cos_v106: POSIX-only in this tree.\n");
    return -1;
}
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* ---------------------------------------------------------------- */
/* Last-σ snapshot (single-threaded server → no lock needed).       */
/* ---------------------------------------------------------------- */

static cos_v106_sigma_snapshot_t g_last_sigma = {0};

static void sigma_snapshot_update(const char *model_id,
                                  double tau_abstain,
                                  cos_v101_sigma_agg_t agg,
                                  float sigma_mean_val,
                                  float sigma_product_val,
                                  float sigma_max_token_val,
                                  const float profile[8],
                                  int abstained)
{
    g_last_sigma.generation++;
    snprintf(g_last_sigma.aggregator, sizeof g_last_sigma.aggregator, "%s",
             cos_v101_aggregator_name(agg));
    g_last_sigma.sigma = (agg == COS_V101_AGG_MEAN)
                             ? sigma_mean_val : sigma_product_val;
    g_last_sigma.sigma_mean       = sigma_mean_val;
    g_last_sigma.sigma_product    = sigma_product_val;
    g_last_sigma.sigma_max_token  = sigma_max_token_val;
    for (int i = 0; i < 8; i++) {
        g_last_sigma.sigma_profile[i] = profile ? profile[i] : 0.f;
    }
    g_last_sigma.abstained   = abstained;
    g_last_sigma.tau_abstain = tau_abstain;
    snprintf(g_last_sigma.model_id, sizeof g_last_sigma.model_id, "%s",
             model_id ? model_id : "creation-os");
}

/* ---------------------------------------------------------------- */
/* HTTP primitives.                                                 */
/* ---------------------------------------------------------------- */

static int write_all(int fd, const void *buf, size_t n)
{
    const char *p = (const char *)buf;
    size_t left = n;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += w;
        left -= (size_t)w;
    }
    return 0;
}

static int read_request_header(int fd, char *hdr, size_t cap, size_t *hlen)
{
    size_t h = 0;
    while (h + 4 < cap) {
        ssize_t n = read(fd, hdr + h, 1);
        if (n <= 0) return -1;
        h++;
        if (h >= 4 && memcmp(hdr + h - 4, "\r\n\r\n", 4) == 0) {
            hdr[h] = '\0';
            if (hlen) *hlen = h;
            return 0;
        }
    }
    return -1;
}

static size_t parse_content_length(const char *hdr)
{
    /* Case-insensitive match. */
    const char *p = hdr;
    const char needle[] = "content-length:";
    size_t nlen = sizeof(needle) - 1;
    while (*p) {
        size_t i;
        for (i = 0; i < nlen; i++) {
            if (!p[i]) return 0;
            if (tolower((unsigned char)p[i]) != needle[i]) break;
        }
        if (i == nlen) {
            p += nlen;
            while (*p == ' ' || *p == '\t') p++;
            return (size_t)strtoull(p, NULL, 10);
        }
        p++;
    }
    return 0;
}

static int read_request_body(int fd, char *buf, size_t cap, size_t want)
{
    if (want + 1 > cap) return -1;
    size_t got = 0;
    while (got < want) {
        ssize_t n = read(fd, buf + got, want - got);
        if (n <= 0) return -1;
        got += (size_t)n;
    }
    buf[got] = '\0';
    return 0;
}

static int write_status(int fd, int code, const char *reason,
                        const char *content_type, const char *body, size_t body_len)
{
    char hdr[512];
    int n = snprintf(hdr, sizeof hdr,
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %zu\r\n"
                     "Access-Control-Allow-Origin: *\r\n"
                     "Connection: close\r\n\r\n",
                     code, reason, content_type, body_len);
    if (n < 0 || (size_t)n >= sizeof(hdr)) return -1;
    if (write_all(fd, hdr, (size_t)n) != 0) return -1;
    if (body_len > 0 && body)
        if (write_all(fd, body, body_len) != 0) return -1;
    return 0;
}

static int write_sse_header(int fd)
{
    const char *h =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n";
    return write_all(fd, h, strlen(h));
}

static int write_sse_chunk(int fd, const char *line)
{
    char buf[8192];
    int n = snprintf(buf, sizeof buf, "data: %s\n\n", line);
    if (n < 0 || (size_t)n >= sizeof buf) return -1;
    return write_all(fd, buf, (size_t)n);
}

static int write_sse_done(int fd)
{
    const char *done = "data: [DONE]\n\n";
    return write_all(fd, done, strlen(done));
}

/* ---------------------------------------------------------------- */
/* JSON encoder (tiny, enough for the response shapes we emit).     */
/* ---------------------------------------------------------------- */

static int json_escape_into(const char *in, char *out, size_t cap)
{
    size_t w = 0;
    if (cap < 1) return -1;
    for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
        const char *esc = NULL;
        char buf[8];
        switch (*p) {
        case '"':  esc = "\\\""; break;
        case '\\': esc = "\\\\"; break;
        case '\n': esc = "\\n";  break;
        case '\r': esc = "\\r";  break;
        case '\t': esc = "\\t";  break;
        case '\b': esc = "\\b";  break;
        case '\f': esc = "\\f";  break;
        default:
            if (*p < 0x20) {
                snprintf(buf, sizeof buf, "\\u%04x", *p);
                esc = buf;
            }
            break;
        }
        if (esc) {
            size_t el = strlen(esc);
            if (w + el >= cap) return -1;
            memcpy(out + w, esc, el);
            w += el;
        } else {
            if (w + 1 >= cap) return -1;
            out[w++] = (char)*p;
        }
    }
    out[w] = '\0';
    return (int)w;
}

/* ---------------------------------------------------------------- */
/* Handlers.                                                        */
/* ---------------------------------------------------------------- */

/* Bridge handle owned by the server instance. NULL in stub mode. */
typedef struct server_state {
    const cos_v106_config_t *cfg;
    cos_v101_bridge_t       *bridge;  /* may be NULL */
    cos_v101_sigma_agg_t     agg;
} server_state_t;

static int handle_health(int fd)
{
    const char body[] = "ok\n";
    return write_status(fd, 200, "OK", "text/plain", body, sizeof body - 1);
}

static int handle_models(int fd, const server_state_t *st)
{
    char body[512];
    int ready = (st->bridge != NULL) ? 1 : 0;
    int n = snprintf(body, sizeof body,
                     "{"
                     "\"object\":\"list\","
                     "\"data\":[{"
                     "\"id\":\"%s\","
                     "\"object\":\"model\","
                     "\"created\":%ld,"
                     "\"owned_by\":\"creation-os\","
                     "\"ready\":%s,"
                     "\"sigma_aggregator\":\"%s\""
                     "}]}",
                     st->cfg->model_id,
                     (long)time(NULL),
                     ready ? "true" : "false",
                     cos_v101_aggregator_name(st->agg));
    if (n < 0 || (size_t)n >= sizeof body) return -1;
    return write_status(fd, 200, "OK", "application/json", body, (size_t)n);
}

static int handle_sigma_profile(int fd, const server_state_t *st)
{
    char body[2048];
    const cos_v106_sigma_snapshot_t *s = &g_last_sigma;
    int n = snprintf(body, sizeof body,
                     "{"
                     "\"generation\":%llu,"
                     "\"model\":\"%s\","
                     "\"sigma_aggregator\":\"%s\","
                     "\"tau_abstain\":%.6f,"
                     "\"sigma\":%.6f,"
                     "\"sigma_mean\":%.6f,"
                     "\"sigma_product\":%.6f,"
                     "\"sigma_max_token\":%.6f,"
                     "\"sigma_profile\":["
                         "{\"name\":\"entropy_norm\",\"value\":%.6f},"
                         "{\"name\":\"margin\",\"value\":%.6f},"
                         "{\"name\":\"top_k_mass\",\"value\":%.6f},"
                         "{\"name\":\"tail_mass\",\"value\":%.6f},"
                         "{\"name\":\"logit_spread\",\"value\":%.6f},"
                         "{\"name\":\"p_max\",\"value\":%.6f},"
                         "{\"name\":\"n_effective\",\"value\":%.6f},"
                         "{\"name\":\"logit_std\",\"value\":%.6f}"
                     "],"
                     "\"abstained\":%s,"
                     "\"has_data\":%s"
                     "}",
                     (unsigned long long)s->generation,
                     s->model_id[0] ? s->model_id : st->cfg->model_id,
                     s->aggregator[0] ? s->aggregator : cos_v101_aggregator_name(st->agg),
                     s->tau_abstain,
                     (double)s->sigma,
                     (double)s->sigma_mean,
                     (double)s->sigma_product,
                     (double)s->sigma_max_token,
                     (double)s->sigma_profile[0], (double)s->sigma_profile[1],
                     (double)s->sigma_profile[2], (double)s->sigma_profile[3],
                     (double)s->sigma_profile[4], (double)s->sigma_profile[5],
                     (double)s->sigma_profile[6], (double)s->sigma_profile[7],
                     s->abstained ? "true" : "false",
                     s->generation > 0 ? "true" : "false");
    if (n < 0 || (size_t)n >= sizeof body) return -1;
    return write_status(fd, 200, "OK", "application/json", body, (size_t)n);
}

/* --- inference path: shared by /v1/chat/completions and /v1/completions -- */

/* Perform inference.  Returns 0 on success, -1 on error (writes
 * canned 503 to `fd` for stub-mode failures; caller should return
 * 0 to drop the connection without further action). */
static int run_inference(int fd, server_state_t *st,
                         const char *prompt,
                         int max_tokens,
                         float sigma_threshold_override,
                         char *out_buf, int out_cap,
                         int *abstained_out,
                         float sigma_profile_out[8],
                         float *sigma_mean_out,
                         float *sigma_product_out,
                         float *sigma_max_tok_out)
{
    if (!st->bridge) {
        const char body[] =
            "{\"error\":{\"message\":\"model not loaded — "
            "configure [model].gguf_path in config.toml or run the "
            "installer\",\"type\":\"not_loaded\"}}";
        write_status(fd, 503, "Service Unavailable", "application/json",
                     body, sizeof body - 1);
        return -1;
    }
    int abstained = 0;
    int n = cos_v101_bridge_generate(st->bridge, prompt,
                                     /*until=*/NULL, /*n_until=*/0,
                                     max_tokens,
                                     (float)st->cfg->tau_abstain,
                                     out_buf, out_cap,
                                     sigma_profile_out,
                                     &abstained);
    (void)sigma_threshold_override;
    if (n < 0) {
        const char body[] =
            "{\"error\":{\"message\":\"generation failed\",\"type\":\"internal\"}}";
        write_status(fd, 500, "Internal Server Error", "application/json",
                     body, sizeof body - 1);
        return -1;
    }
    /* Approximate post-hoc aggregator scalars from the returned
     * profile mean.  A future API addition (`generate_v105`) will
     * return the per-continuation mean/product pair directly; until
     * then we reconstruct from the profile to feed /v1/sigma-profile
     * with reasonable numbers. */
    double sum = 0.0, logsum = 0.0;
    const double EPS = 1e-6;
    for (int i = 0; i < 8; i++) {
        double v = sigma_profile_out ? (double)sigma_profile_out[i] : 0.0;
        sum    += v;
        double w = (v < EPS) ? EPS : v;
        logsum += log(w);
    }
    float mean    = (float)(sum / 8.0);
    float product = (float)exp(logsum / 8.0);
    if (sigma_mean_out)    *sigma_mean_out    = mean;
    if (sigma_product_out) *sigma_product_out = product;
    if (sigma_max_tok_out) *sigma_max_tok_out = 0.f;  /* not returned by generate() */
    if (abstained_out)     *abstained_out     = abstained;
    sigma_snapshot_update(st->cfg->model_id,
                          st->cfg->tau_abstain, st->agg,
                          mean, product, 0.f,
                          sigma_profile_out, abstained);
    return 0;
}

/* v112 σ-Agent: σ-gated tool selection.  Called only when the request
 * body contains a top-level "tools":[…] array.  Returns 1 if the request
 * was handled here (response already written), 0 to fall through to the
 * normal chat.completions path, <0 on transport error. */
static int maybe_handle_tools(int fd, server_state_t *st, const char *body,
                              const char *prompt, long max_tokens,
                              const char *model_req)
{
    if (!strstr(body, "\"tools\"")) return 0;

    cos_v112_tool_def_t tools[COS_V112_TOOLS_MAX];
    int n = cos_v112_parse_tools(body, tools, COS_V112_TOOLS_MAX);
    if (n <= 0) return 0;

    cos_v112_tool_choice_t choice;
    cos_v112_parse_tool_choice(body, &choice);

    cos_v112_opts_t opts;
    cos_v112_opts_defaults(&opts);
    opts.max_tokens = (int)max_tokens;
    double tau_body = cos_v106_json_get_double(body, "tau_tool", -1.0);
    if (tau_body >= 0.0 && tau_body <= 1.0) opts.tau_tool = (float)tau_body;

    cos_v112_tool_call_report_t report;
    int rc = cos_v112_run_tool_selection(st->bridge, prompt,
                                         tools, (size_t)n,
                                         &choice, &opts, &report);
    if (rc != 0) {
        const char err[] =
            "{\"error\":{\"message\":\"tool selection failed\","
            "\"type\":\"internal\"}}";
        return write_status(fd, 500, "Internal Server Error",
                            "application/json", err, sizeof err - 1);
    }

    /* Propagate σ telemetry to /v1/sigma-profile. */
    sigma_snapshot_update(model_req, st->cfg->tau_abstain, st->agg,
                          report.sigma_mean, report.sigma_product,
                          report.sigma_max_token,
                          report.sigma_profile, report.abstained);

    char message_js[8192];
    int  mlen = cos_v112_report_to_message_json(&report, message_js,
                                                sizeof message_js);
    if (mlen < 0) {
        const char err[] =
            "{\"error\":{\"message\":\"tool report overflow\","
            "\"type\":\"internal\"}}";
        return write_status(fd, 500, "Internal Server Error",
                            "application/json", err, sizeof err - 1);
    }

    char payload[16384];
    long created = (long)time(NULL);
    int  pn = snprintf(payload, sizeof payload,
        "{"
        "\"id\":\"chatcmpl-tool-%ld\","
        "\"object\":\"chat.completion\","
        "\"created\":%ld,"
        "\"model\":\"%s\","
        "\"choices\":[{"
            "\"index\":0,"
            "\"message\":%s,"
            "\"finish_reason\":\"%s\""
        "}],"
        "\"creation_os\":{"
            "\"tool_gate\":true,"
            "\"tau_tool\":%.4f,"
            "\"sigma_product\":%.6f,"
            "\"sigma_mean\":%.6f,"
            "\"abstained\":%s"
        "}"
        "}",
        created, created, model_req, message_js,
        report.abstained ? "cos_abstained"
                         : (report.tool_called ? "tool_calls" : "stop"),
        (double)report.tau_tool,
        (double)report.sigma_product, (double)report.sigma_mean,
        report.abstained ? "true" : "false");
    if (pn < 0 || (size_t)pn >= sizeof payload) {
        const char err[] = "{\"error\":{\"message\":\"tool payload overflow\"}}";
        return write_status(fd, 500, "Internal Server Error",
                            "application/json", err, sizeof err - 1);
    }
    return write_status(fd, 200, "OK", "application/json",
                        payload, (size_t)pn) < 0 ? -1 : 1;
}

static int handle_chat_completions(int fd, server_state_t *st, const char *body)
{
    char prompt[8192];
    if (cos_v106_json_extract_last_user(body, prompt, sizeof prompt) < 0) {
        const char err[] = "{\"error\":{\"message\":\"no user message\",\"type\":\"invalid_request\"}}";
        return write_status(fd, 400, "Bad Request", "application/json",
                            err, sizeof err - 1);
    }
    long max_tokens = cos_v106_json_get_int(body, "max_tokens", 256);
    if (max_tokens < 1)   max_tokens = 1;
    if (max_tokens > 4096) max_tokens = 4096;
    int stream = cos_v106_json_stream_flag(body);

    char model_req[128];
    if (cos_v106_json_get_string(body, "model", model_req, sizeof model_req) < 0) {
        snprintf(model_req, sizeof model_req, "%s", st->cfg->model_id);
    }

    /* v112 σ-Agent: σ-gated tool calling.  If `tools` is present, the
     * v112 driver takes ownership of the response. */
    {
        int tool_rc = maybe_handle_tools(fd, st, body, prompt,
                                         max_tokens, model_req);
        if (tool_rc != 0) return (tool_rc < 0) ? -1 : 0;
    }

    char out_buf[8192];
    int  abstained = 0;
    float profile[8] = {0};
    float sigma_mean = 0.f, sigma_product = 0.f, sigma_max_tok = 0.f;

    /* Non-streaming path ---------------------------------------------- */
    if (!stream) {
        if (run_inference(fd, st, prompt, (int)max_tokens, 0.f,
                          out_buf, sizeof out_buf,
                          &abstained, profile,
                          &sigma_mean, &sigma_product, &sigma_max_tok) != 0) {
            return 0;
        }
        char esc[16384];
        if (json_escape_into(out_buf, esc, sizeof esc) < 0) {
            const char err[] = "{\"error\":{\"message\":\"escape overflow\",\"type\":\"internal\"}}";
            return write_status(fd, 500, "Internal Server Error", "application/json",
                                err, sizeof err - 1);
        }
        char payload[32768];
        long created = (long)time(NULL);
        int  n = snprintf(payload, sizeof payload,
            "{"
            "\"id\":\"chatcmpl-%ld\","
            "\"object\":\"chat.completion\","
            "\"created\":%ld,"
            "\"model\":\"%s\","
            "\"choices\":[{"
                "\"index\":0,"
                "\"message\":{\"role\":\"assistant\",\"content\":\"%s\"},"
                "\"finish_reason\":\"%s\""
            "}],"
            "\"creation_os\":{"
                "\"sigma_aggregator\":\"%s\","
                "\"tau_abstain\":%.6f,"
                "\"sigma\":%.6f,"
                "\"sigma_mean\":%.6f,"
                "\"sigma_product\":%.6f,"
                "\"sigma_profile\":[%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f],"
                "\"abstained\":%s"
            "}"
            "}",
            created, created, st->cfg->model_id, esc,
            abstained ? "abstain" : "stop",
            cos_v101_aggregator_name(st->agg),
            st->cfg->tau_abstain,
            (st->agg == COS_V101_AGG_MEAN) ? (double)sigma_mean : (double)sigma_product,
            (double)sigma_mean, (double)sigma_product,
            (double)profile[0], (double)profile[1], (double)profile[2], (double)profile[3],
            (double)profile[4], (double)profile[5], (double)profile[6], (double)profile[7],
            abstained ? "true" : "false");
        if (n < 0 || (size_t)n >= sizeof payload) {
            const char err[] = "{\"error\":{\"message\":\"response overflow\",\"type\":\"internal\"}}";
            return write_status(fd, 500, "Internal Server Error", "application/json",
                                err, sizeof err - 1);
        }
        return write_status(fd, 200, "OK", "application/json", payload, (size_t)n);
    }

    /* Streaming path (SSE) ------------------------------------------- */
    /* For now we run the full completion then deliver it in a single
     * chunk followed by a σ-annotated terminator chunk.  True
     * token-by-token streaming requires threading a callback into
     * the bridge, which is a follow-up.  The client still gets
     * OpenAI-compatible framing: a single chat.completion.chunk with
     * delta.content=full, then [DONE]. */
    if (write_sse_header(fd) != 0) return -1;
    if (run_inference(fd, st, prompt, (int)max_tokens, 0.f,
                      out_buf, sizeof out_buf,
                      &abstained, profile,
                      &sigma_mean, &sigma_product, &sigma_max_tok) != 0) {
        /* status already written, but SSE header was already out —
         * emit a final error chunk and terminate. */
        write_sse_chunk(fd, "{\"error\":\"inference failed\"}");
        write_sse_done(fd);
        return 0;
    }
    {
        char esc[16384];
        if (json_escape_into(out_buf, esc, sizeof esc) < 0) {
            write_sse_chunk(fd, "{\"error\":\"escape overflow\"}");
            write_sse_done(fd);
            return 0;
        }
        char chunk[32768];
        long created = (long)time(NULL);
        int  n = snprintf(chunk, sizeof chunk,
            "{"
            "\"id\":\"chatcmpl-%ld\","
            "\"object\":\"chat.completion.chunk\","
            "\"created\":%ld,"
            "\"model\":\"%s\","
            "\"choices\":[{\"index\":0,\"delta\":{\"role\":\"assistant\",\"content\":\"%s\"},"
                "\"finish_reason\":null}]"
            "}",
            created, created, st->cfg->model_id, esc);
        if (n > 0 && (size_t)n < sizeof chunk)
            write_sse_chunk(fd, chunk);
    }
    {
        /* Final σ-annotated chunk.  OpenAI clients will discard the
         * creation_os block silently; our own clients key on it. */
        char tail[4096];
        long created = (long)time(NULL);
        int  n = snprintf(tail, sizeof tail,
            "{"
            "\"id\":\"chatcmpl-%ld\","
            "\"object\":\"chat.completion.chunk\","
            "\"created\":%ld,"
            "\"model\":\"%s\","
            "\"choices\":[{\"index\":0,\"delta\":{},\"finish_reason\":\"%s\"}],"
            "\"creation_os\":{"
                "\"sigma_aggregator\":\"%s\","
                "\"sigma\":%.6f,"
                "\"sigma_mean\":%.6f,"
                "\"sigma_product\":%.6f,"
                "\"sigma_profile\":[%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f],"
                "\"abstained\":%s"
            "}"
            "}",
            created, created, st->cfg->model_id,
            abstained ? "abstain" : "stop",
            cos_v101_aggregator_name(st->agg),
            (st->agg == COS_V101_AGG_MEAN) ? (double)sigma_mean : (double)sigma_product,
            (double)sigma_mean, (double)sigma_product,
            (double)profile[0], (double)profile[1], (double)profile[2], (double)profile[3],
            (double)profile[4], (double)profile[5], (double)profile[6], (double)profile[7],
            abstained ? "true" : "false");
        if (n > 0 && (size_t)n < sizeof tail)
            write_sse_chunk(fd, tail);
    }
    return write_sse_done(fd);
}

static int handle_completions(int fd, server_state_t *st, const char *body)
{
    char prompt[8192];
    if (cos_v106_json_get_string(body, "prompt", prompt, sizeof prompt) < 0) {
        const char err[] = "{\"error\":{\"message\":\"missing prompt\",\"type\":\"invalid_request\"}}";
        return write_status(fd, 400, "Bad Request", "application/json",
                            err, sizeof err - 1);
    }
    long max_tokens = cos_v106_json_get_int(body, "max_tokens", 256);
    if (max_tokens < 1)   max_tokens = 1;
    if (max_tokens > 4096) max_tokens = 4096;

    char out_buf[8192];
    int  abstained = 0;
    float profile[8] = {0};
    float sigma_mean = 0.f, sigma_product = 0.f, sigma_max_tok = 0.f;

    if (run_inference(fd, st, prompt, (int)max_tokens, 0.f,
                      out_buf, sizeof out_buf,
                      &abstained, profile,
                      &sigma_mean, &sigma_product, &sigma_max_tok) != 0) {
        return 0;
    }
    char esc[16384];
    if (json_escape_into(out_buf, esc, sizeof esc) < 0) {
        const char err[] = "{\"error\":{\"message\":\"escape overflow\",\"type\":\"internal\"}}";
        return write_status(fd, 500, "Internal Server Error", "application/json",
                            err, sizeof err - 1);
    }
    char payload[32768];
    long created = (long)time(NULL);
    int  n = snprintf(payload, sizeof payload,
        "{"
        "\"id\":\"cmpl-%ld\","
        "\"object\":\"text_completion\","
        "\"created\":%ld,"
        "\"model\":\"%s\","
        "\"choices\":[{\"index\":0,\"text\":\"%s\",\"finish_reason\":\"%s\"}],"
        "\"creation_os\":{"
            "\"sigma_aggregator\":\"%s\","
            "\"sigma_mean\":%.6f,"
            "\"sigma_product\":%.6f,"
            "\"sigma_profile\":[%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f],"
            "\"abstained\":%s"
        "}"
        "}",
        created, created, st->cfg->model_id, esc,
        abstained ? "abstain" : "stop",
        cos_v101_aggregator_name(st->agg),
        (double)sigma_mean, (double)sigma_product,
        (double)profile[0], (double)profile[1], (double)profile[2], (double)profile[3],
        (double)profile[4], (double)profile[5], (double)profile[6], (double)profile[7],
        abstained ? "true" : "false");
    if (n < 0 || (size_t)n >= sizeof payload) {
        const char err[] = "{\"error\":{\"message\":\"response overflow\",\"type\":\"internal\"}}";
        return write_status(fd, 500, "Internal Server Error", "application/json",
                            err, sizeof err - 1);
    }
    return write_status(fd, 200, "OK", "application/json", payload, (size_t)n);
}

/* ------------------------------------------------------------------ */
/* v111.2 reasoning endpoint: POST /v1/reason                         */
/* ------------------------------------------------------------------ */

static int handle_reason(int fd, server_state_t *st, const char *body)
{
    /* Validate body before the stub-mode 503 — a missing prompt is a
     * client error regardless of whether the model is loaded. */
    char prompt[8192];
    if (cos_v106_json_get_string(body, "prompt", prompt, sizeof prompt) < 0) {
        /* Fall back to chat-style "messages": pick last user message. */
        if (cos_v106_json_extract_last_user(body, prompt, sizeof prompt) < 0) {
            const char err[] = "{\"error\":{\"message\":\"missing prompt or messages\","
                               "\"type\":\"invalid_request\"}}";
            return write_status(fd, 400, "Bad Request", "application/json",
                                err, sizeof err - 1);
        }
    }

    if (!st->bridge) {
        const char err[] =
            "{\"error\":{\"message\":\"model not loaded — "
            "configure [model].gguf_path in config.toml\","
            "\"type\":\"not_loaded\"}}";
        return write_status(fd, 503, "Service Unavailable", "application/json",
                            err, sizeof err - 1);
    }

    cos_v111_reason_opts_t opts;
    cos_v111_reason_opts_defaults(&opts);
    opts.tau_abstain = (float)st->cfg->tau_abstain;

    long k_cand     = cos_v106_json_get_int(body, "k_candidates", opts.k_candidates);
    long max_tok    = cos_v106_json_get_int(body, "max_tokens",   opts.max_tokens);
    double tau_body = cos_v106_json_get_double(body, "tau_abstain", opts.tau_abstain);
    if (k_cand < 1) k_cand = 1;
    if (k_cand > COS_V111_REASON_MAX_CANDIDATES) k_cand = COS_V111_REASON_MAX_CANDIDATES;
    if (max_tok < 1)  max_tok = 1;
    if (max_tok > 2048) max_tok = 2048;
    opts.k_candidates = (int)k_cand;
    opts.max_tokens   = (int)max_tok;
    opts.tau_abstain  = (float)tau_body;

    cos_v111_reason_report_t report;
    int rc = cos_v111_reason_run(st->bridge, prompt, &opts, &report);
    if (rc != 0) {
        const char err[] =
            "{\"error\":{\"message\":\"reasoning pipeline failed\","
            "\"type\":\"internal\"}}";
        return write_status(fd, 500, "Internal Server Error",
                            "application/json", err, sizeof err - 1);
    }

    /* Update the last-σ snapshot from the chosen candidate so the
     * /v1/sigma-profile endpoint reflects what /v1/reason returned. */
    if (report.chosen_index >= 0 && report.chosen_index < report.n_candidates) {
        const cos_v111_reason_candidate_t *c =
            &report.candidates[report.chosen_index];
        sigma_snapshot_update(st->cfg->model_id,
                              st->cfg->tau_abstain, st->agg,
                              c->sigma_mean, c->sigma_product, 0.f,
                              c->sigma_profile, report.abstained);
    }

    char payload[65536];
    int  n = cos_v111_reason_report_to_json(&report, st->cfg->model_id,
                                            payload, sizeof payload);
    if (n < 0) {
        const char err[] =
            "{\"error\":{\"message\":\"reason payload overflow\","
            "\"type\":\"internal\"}}";
        return write_status(fd, 500, "Internal Server Error",
                            "application/json", err, sizeof err - 1);
    }
    return write_status(fd, 200, "OK", "application/json", payload, (size_t)n);
}

/* --- v113 σ-Sandbox -------------------------------------------- */

static int handle_sandbox_execute(int fd, server_state_t *st, const char *body)
{
    (void)st;
    cos_v113_request_t req;
    if (cos_v113_parse_request(body, &req) != 0) {
        const char err[] =
            "{\"error\":{\"message\":\"malformed sandbox request\","
            "\"type\":\"invalid_request\"}}";
        return write_status(fd, 400, "Bad Request", "application/json",
                            err, sizeof err - 1);
    }
    cos_v113_result_t res;
    if (cos_v113_sandbox_run(&req, &res) != 0) {
        const char err[] =
            "{\"error\":{\"message\":\"sandbox runtime error\","
            "\"type\":\"internal\"}}";
        return write_status(fd, 500, "Internal Server Error",
                            "application/json", err, sizeof err - 1);
    }
    static char js[96 * 1024];
    int jl = cos_v113_result_to_json(&res, js, sizeof js);
    if (jl < 0) {
        const char err[] = "{\"error\":{\"message\":\"receipt overflow\"}}";
        return write_status(fd, 500, "Internal Server Error",
                            "application/json", err, sizeof err - 1);
    }
    return write_status(fd, 200, "OK", "application/json", js, (size_t)jl);
}

/* --- static file serving (v108 will land index.html at web/) ------ */

static const char *mime_for_path(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".js")   == 0) return "application/javascript; charset=utf-8";
    if (strcmp(ext, ".css")  == 0) return "text/css; charset=utf-8";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".svg")  == 0) return "image/svg+xml";
    if (strcmp(ext, ".png")  == 0) return "image/png";
    if (strcmp(ext, ".ico")  == 0) return "image/x-icon";
    return "application/octet-stream";
}

static int serve_static(int fd, const server_state_t *st, const char *url_path)
{
    /* Defence in depth: refuse any ".." segment.  This is a local
     * loopback server, not an Internet-facing static host, but cheap
     * to do right. */
    if (strstr(url_path, "..") != NULL) {
        const char b[] = "forbidden\n";
        return write_status(fd, 403, "Forbidden", "text/plain", b, sizeof b - 1);
    }
    char full[1024];
    const char *rel = (url_path[0] == '/') ? url_path + 1 : url_path;
    if (!*rel || strcmp(rel, "") == 0) rel = "index.html";
    int n = snprintf(full, sizeof full, "%s/%s", st->cfg->web_root, rel);
    if (n < 0 || (size_t)n >= sizeof full) {
        const char b[] = "path overflow\n";
        return write_status(fd, 500, "Internal Server Error", "text/plain", b, sizeof b - 1);
    }
    FILE *f = fopen(full, "rb");
    if (!f) {
        const char b[] = "not found\n";
        return write_status(fd, 404, "Not Found", "text/plain", b, sizeof b - 1);
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return -1; }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return -1;
    }
    fclose(f);
    int rc = write_status(fd, 200, "OK", mime_for_path(full), buf, (size_t)sz);
    free(buf);
    return rc;
}

/* ---------------------------------------------------------------- */
/* Per-connection dispatch.                                         */
/* ---------------------------------------------------------------- */

static int handle_one(int cfd, server_state_t *st)
{
    char hdr[8192];
    size_t hlen = 0;
    if (read_request_header(cfd, hdr, sizeof hdr, &hlen) != 0) return -1;

    /* Parse method + path + version (first line). */
    char method[16] = {0}, path[1024] = {0};
    if (sscanf(hdr, "%15s %1023s", method, path) != 2) return -1;

    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/health") == 0)         return handle_health(cfd);
        if (strcmp(path, "/v1/models") == 0)      return handle_models(cfd, st);
        if (strcmp(path, "/v1/sigma-profile") == 0) return handle_sigma_profile(cfd, st);
        if (strcmp(path, "/") == 0)               return serve_static(cfd, st, "/index.html");
        if (strncmp(path, "/v1/", 4) != 0)        return serve_static(cfd, st, path);
        const char b[] = "{\"error\":{\"message\":\"not found\"}}";
        return write_status(cfd, 404, "Not Found", "application/json", b, sizeof b - 1);
    }

    if (strcmp(method, "POST") == 0) {
        size_t cl = parse_content_length(hdr);
        if (cl == 0 || cl > 1024 * 1024) {
            const char b[] = "{\"error\":{\"message\":\"bad content-length\"}}";
            return write_status(cfd, 400, "Bad Request", "application/json", b, sizeof b - 1);
        }
        char *body = (char *)malloc(cl + 1);
        if (!body) return -1;
        if (read_request_body(cfd, body, cl + 1, cl) != 0) {
            free(body); return -1;
        }
        int rc;
        if (strcmp(path, "/v1/chat/completions") == 0)
            rc = handle_chat_completions(cfd, st, body);
        else if (strcmp(path, "/v1/completions") == 0)
            rc = handle_completions(cfd, st, body);
        else if (strcmp(path, "/v1/reason") == 0)
            rc = handle_reason(cfd, st, body);
        else if (strcmp(path, "/v1/sandbox/execute") == 0)
            rc = handle_sandbox_execute(cfd, st, body);
        else {
            const char b[] = "{\"error\":{\"message\":\"not found\"}}";
            rc = write_status(cfd, 404, "Not Found", "application/json", b, sizeof b - 1);
        }
        free(body);
        return rc;
    }

    if (strcmp(method, "OPTIONS") == 0) {
        /* Bare-bones CORS preflight. */
        const char h[] =
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
            "Connection: close\r\n\r\n";
        return write_all(cfd, h, strlen(h));
    }

    const char b[] = "{\"error\":{\"message\":\"method not allowed\"}}";
    return write_status(cfd, 405, "Method Not Allowed", "application/json", b, sizeof b - 1);
}

/* ---------------------------------------------------------------- */
/* Entry point.                                                      */
/* ---------------------------------------------------------------- */

int cos_v106_server_run(const cos_v106_config_t *cfg)
{
    if (!cfg) return -1;

    /* Apply the aggregator config.  CLI / env var still wins via the
     * usual env-first semantics; here we just read what's on the
     * config struct and pin the default aggregator for the rest of
     * the process. */
    cos_v101_sigma_agg_t agg = COS_V101_AGG_PRODUCT;
    if (strcmp(cfg->aggregator, "mean") == 0) agg = COS_V101_AGG_MEAN;
    cos_v101_set_default_aggregator(agg);

    /* Ignore SIGPIPE — we always write defensively. */
    signal(SIGPIPE, SIG_IGN);

    /* Try to open the bridge; stub mode if it fails / path unset. */
    cos_v101_bridge_t *bridge = NULL;
    if (cfg->gguf_path[0]) {
        bridge = cos_v101_bridge_open(cfg->gguf_path, (uint32_t)cfg->n_ctx);
        if (!bridge) {
            fprintf(stderr,
                    "cos_v106: GGUF path '%s' did not open — serving in "
                    "no-model mode (all /v1/*completions return 503).\n",
                    cfg->gguf_path);
        }
    } else {
        fprintf(stderr,
                "cos_v106: no [model].gguf_path configured — serving in "
                "no-model mode (all /v1/*completions return 503).\n");
    }

    server_state_t st = { .cfg = cfg, .bridge = bridge, .agg = agg };

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    int one = 1;
    (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
#ifdef TCP_NODELAY
    (void)setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
#endif

    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    if (inet_pton(AF_INET, cfg->host, &a.sin_addr) != 1) {
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    a.sin_port = htons((uint16_t)cfg->port);

    if (bind(s, (struct sockaddr *)&a, sizeof a) != 0) {
        fprintf(stderr, "cos_v106: bind %s:%d failed: %s\n",
                cfg->host, cfg->port, strerror(errno));
        close(s);
        if (bridge) cos_v101_bridge_close(bridge);
        return -1;
    }
    if (listen(s, 16) != 0) {
        close(s);
        if (bridge) cos_v101_bridge_close(bridge);
        return -1;
    }

    fprintf(stderr,
            "cos_v106: serving on http://%s:%d  model=%s  aggregator=%s  "
            "tau_abstain=%.3f  mode=%s\n",
            cfg->host, cfg->port, cfg->model_id,
            cos_v101_aggregator_name(agg), cfg->tau_abstain,
            bridge ? "real" : "no-model");

    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0) {
            if (errno == EINTR) continue;
            break;
        }
        (void)handle_one(c, &st);
        close(c);
    }
    close(s);
    if (bridge) cos_v101_bridge_close(bridge);
    return 0;
}
#endif /* !_WIN32 */
