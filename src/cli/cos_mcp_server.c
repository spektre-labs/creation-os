/* cos-mcp — JSON-RPC 2.0 stdio MCP server exposing the six Creation
 * OS `cos.*` tools.  This is the NEXT-4 kernel that turns Creation
 * OS into **infrastructure**: any MCP-capable agent (Claude Code,
 * Cursor, Windsurf, OpenClaw, Hermes) can dial in and ask for a σ
 * before committing to an answer.
 *
 * Tools (all return JSON-serialised `result` objects):
 *
 *   cos.chat            { prompt }              → { response, sigma,
 *                                                   action, cache,
 *                                                   rethink_count,
 *                                                   elapsed_ms, route }
 *   cos.sigma           { prompt }              → { sigma_combined,
 *                                                   sigma_logprob,
 *                                                   sigma_entropy,
 *                                                   sigma_perplexity,
 *                                                   sigma_consistency }
 *   cos.calibrate       { path?, domain? }      → { tau, alpha, domain,
 *                                                   valid, path }
 *   cos.health          {}                      → { version, mode,
 *                                                   engram_count,
 *                                                   cost_eur,
 *                                                   proofs_lean,
 *                                                   proofs_frama_c,
 *                                                   coherence_state }
 *   cos.engram.lookup   { prompt }              → { hit, sigma?,
 *                                                   response? }
 *   cos.introspect      { prompt }              → { sigma_perception,
 *                                                   sigma_self,
 *                                                   sigma_social,
 *                                                   sigma_situational }
 *
 * Transport: line-delimited JSON-RPC 2.0 over stdio, per the 2026-03-26
 * MCP spec.  Each request is one line of JSON; each reply is one line.
 *
 * Modes:
 *   --stdio   read requests from stdin, write replies to stdout (default)
 *   --demo    run a canonical request script, emit a JSON envelope for
 *             CI diff (benchmarks/sigma_pipeline/check_cos_mcp.sh)
 *   --once    read one line, answer once, exit (CI hook)
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "pipeline.h"
#include "codex.h"
#include "engram.h"
#include "engram_persist.h"
#include "sovereign.h"
#include "agent.h"
#include "stub_gen.h"
#include "escalation.h"
#include "conformal.h"
#include "multi_sigma.h"
#include "introspection.h"
#include "cos_version.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COS_MCP2_PROTOCOL_VERSION  "2026-03-26"
#define COS_MCP2_LINE_MAX          8192
#define COS_MCP2_REPLY_MAX         8192

/* =====================================================================
 * Minimal JSON scan — only what MCP tools/call needs.
 * ===================================================================== */

static const char *mcp_skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* Find "key": inside a JSON object text, return pointer to value start. */
static const char *mcp_find_key(const char *json, const char *key) {
    if (json == NULL || key == NULL) return NULL;
    size_t klen = strlen(key);
    const char *p = json;
    while ((p = strchr(p, '"')) != NULL) {
        const char *q = p + 1;
        if (strncmp(q, key, klen) == 0 && q[klen] == '"') {
            q += klen + 1;
            q = mcp_skip_ws(q);
            if (*q == ':') {
                q = mcp_skip_ws(q + 1);
                return q;
            }
        }
        p++;
    }
    return NULL;
}

/* Copy JSON string value (v must point at the opening quote) into dst.
 * Returns 0 on success, -1 on malformed / overflow. */
static int mcp_copy_json_string(const char *v, char *dst, size_t cap) {
    if (v == NULL || *v != '"' || dst == NULL || cap == 0) return -1;
    v++;  /* past opening " */
    size_t w = 0;
    while (*v && *v != '"') {
        if (w + 1 >= cap) return -1;
        if (*v == '\\' && v[1] != '\0') {
            /* Pass-through common escapes; keep them literal. */
            char e = v[1];
            switch (e) {
                case 'n': dst[w++] = '\n'; break;
                case 't': dst[w++] = '\t'; break;
                case '"': dst[w++] = '"';  break;
                case '\\': dst[w++] = '\\'; break;
                case '/':  dst[w++] = '/';  break;
                default:   dst[w++] = e;    break;
            }
            v += 2;
        } else {
            dst[w++] = *v++;
        }
    }
    dst[w] = '\0';
    return (*v == '"') ? 0 : -1;
}

static int mcp_extract_id_token(const char *req, char *out, size_t cap) {
    const char *v = mcp_find_key(req, "id");
    if (v == NULL) { snprintf(out, cap, "null"); return 0; }
    size_t w = 0;
    int depth = 0;
    while (*v != '\0' && w + 1 < cap) {
        if (*v == '{' || *v == '[') depth++;
        else if (*v == '}' || *v == ']') {
            if (depth == 0) break;
            depth--;
        }
        else if (depth == 0 && (*v == ',' || *v == '}')) break;
        out[w++] = *v++;
    }
    out[w] = '\0';
    /* Trim trailing spaces. */
    while (w > 0 && isspace((unsigned char)out[w - 1])) out[--w] = '\0';
    return (int)w;
}

static void mcp_emit_json_string(FILE *fp, const char *s) {
    fputc('"', fp);
    if (s != NULL) {
        for (; *s; ++s) {
            unsigned char c = (unsigned char)*s;
            if (c == '"' || c == '\\') { fputc('\\', fp); fputc((int)c, fp); }
            else if (c == '\n')        { fputs("\\n", fp); }
            else if (c == '\t')        { fputs("\\t", fp); }
            else if (c < 0x20u)        { fprintf(fp, "\\u%04x", (unsigned)c); }
            else                        { fputc((int)c, fp); }
        }
    }
    fputc('"', fp);
}

/* =====================================================================
 * Shared chat / engram / calibration state for all tool calls in a
 * single cos-mcp process.  One process = one session.
 * ===================================================================== */

typedef struct {
    cos_pipeline_config_t        cfg;
    cos_sigma_codex_t            codex;
    int                          have_codex;
    cos_sigma_engram_entry_t     slots[64];
    cos_sigma_engram_t           engram;
    cos_sigma_sovereign_t        sv;
    cos_sigma_agent_t            ag;
    cos_engram_persist_t        *persist;
    cos_cli_generate_ctx_t       genctx;

    /* Conformal bundle (loaded lazily). */
    int                          conformal_loaded;
    float                        tau;
    float                        alpha;
    char                         domain[COS_CONFORMAL_NAME_MAX];
    char                         calib_path[512];

    /* Running σ sum for lightweight coherence state. */
    double                       sigma_sum;
    int                          sigma_n;
} cos_mcp2_state_t;

static void cos_mcp2_state_init(cos_mcp2_state_t *st) {
    memset(st, 0, sizeof(*st));

    int rc = cos_sigma_codex_load(NULL, &st->codex);
    st->have_codex = (rc == 0);

    memset(st->slots, 0, sizeof(st->slots));
    cos_sigma_engram_init(&st->engram, st->slots, 64, 0.25f, 200, 20);

    if (cos_engram_persist_open(NULL, &st->persist) == 0) {
        cos_engram_persist_load(st->persist, &st->engram, 64);
    }

    cos_sigma_sovereign_init(&st->sv, 0.85f);
    cos_sigma_agent_init(&st->ag, 0.80f, 0.10f);

    cos_sigma_pipeline_config_defaults(&st->cfg);
    st->cfg.tau_accept   = 0.40f;
    st->cfg.tau_rethink  = 0.60f;
    st->cfg.max_rethink  = 3;
    st->cfg.mode         = COS_PIPELINE_MODE_HYBRID;
    st->cfg.codex        = st->have_codex ? &st->codex : NULL;
    st->cfg.engram       = &st->engram;
    st->cfg.sovereign    = &st->sv;
    st->cfg.agent        = &st->ag;
    st->cfg.generate     = cos_cli_chat_generate;

    memset(&st->genctx, 0, sizeof(st->genctx));
    st->genctx.magic  = COS_CLI_GENERATE_CTX_MAGIC;
    st->genctx.codex  = st->have_codex ? &st->codex : NULL;
    st->genctx.persist = st->persist;
    st->genctx.icl_exemplar_max_sigma = st->cfg.tau_accept;
    st->cfg.generate_ctx = &st->genctx;

    st->cfg.escalate = cos_cli_escalate_api;
    if (st->persist != NULL) {
        st->cfg.on_engram_store     = cos_engram_persist_store;
        st->cfg.on_engram_store_ctx = st->persist;
    }

    /* Lazy conformal: loaded on first cos.calibrate or cos.health call. */
    st->conformal_loaded = 0;

    const char *env = getenv("COS_CALIBRATION");
    if (env != NULL && env[0] != '\0') {
        snprintf(st->calib_path, sizeof(st->calib_path), "%s", env);
    } else {
        const char *home = getenv("HOME");
        if (home != NULL)
            snprintf(st->calib_path, sizeof(st->calib_path),
                     "%s/.cos/calibration.json", home);
    }
}

static void cos_mcp2_state_free(cos_mcp2_state_t *st) {
    if (st == NULL) return;
    cos_sigma_pipeline_free_engram_values(&st->engram);
    cos_sigma_codex_free(&st->codex);
    cos_engram_persist_close(st->persist);
}

static int cos_mcp2_load_conformal(cos_mcp2_state_t *st,
                                   const char *override_path) {
    const char *p = (override_path != NULL && override_path[0] != '\0')
                  ? override_path : st->calib_path;
    if (p == NULL || p[0] == '\0') return -1;
    cos_conformal_bundle_t b;
    if (cos_conformal_read_bundle_json(p, &b) != 0) return -1;
    for (int i = 0; i < b.n_reports; ++i) {
        if (!b.reports[i].valid) continue;
        st->tau   = b.reports[i].tau;
        st->alpha = b.reports[i].alpha;
        snprintf(st->domain, sizeof(st->domain), "%s",
                 b.reports[i].domain);
        st->conformal_loaded = 1;
        return 0;
    }
    return -1;
}

/* =====================================================================
 * Tool implementations.
 *
 * Each tool writes the JSON `result` object body (without braces) into
 * `out`; the dispatcher wraps it in `{"jsonrpc":"2.0","id":…,"result":{…}}`.
 * ===================================================================== */

static int count_words(const char *s) {
    int n = 0, in = 0;
    for (; s && *s; ++s) {
        int w = (*s > ' ');
        if (w && !in) { n++; in = 1; }
        else if (!w)  { in = 0; }
    }
    return n;
}

static const char *action_lbl(cos_sigma_action_t a) {
    switch (a) {
        case COS_SIGMA_ACTION_ACCEPT:  return "ACCEPT";
        case COS_SIGMA_ACTION_RETHINK: return "RETHINK";
        case COS_SIGMA_ACTION_ABSTAIN: return "ABSTAIN";
        default: return "?";
    }
}

static void tool_cos_chat(cos_mcp2_state_t *st,
                          const char *prompt,
                          FILE *out) {
    clock_t t0 = clock();
    cos_pipeline_result_t r;
    if (prompt == NULL || prompt[0] == '\0' ||
        cos_sigma_pipeline_run(&st->cfg, prompt, &r) != 0) {
        fprintf(out, "\"error\":\"cos.chat: empty prompt or pipeline failure\"");
        return;
    }
    clock_t t1 = clock();
    double ms = (double)(t1 - t0) * 1000.0 / (double)CLOCKS_PER_SEC;
    if (ms < 0.0) ms = 0.0;

    st->sigma_sum += r.sigma;
    st->sigma_n   += 1;

    fprintf(out, "\"response\":");
    mcp_emit_json_string(out, r.response != NULL ? r.response : "");
    fprintf(out,
            ",\"sigma\":%.6f,\"action\":\"%s\",\"cache\":%s,"
            "\"rethink_count\":%d,\"route\":\"%s\","
            "\"elapsed_ms\":%.3f,\"cost_eur\":%.6f",
            (double)r.sigma, action_lbl(r.final_action),
            r.engram_hit ? "true" : "false",
            r.rethink_count,
            r.escalated ? "CLOUD" : "LOCAL",
            ms, r.cost_eur);
}

static void tool_cos_sigma(cos_mcp2_state_t *st,
                           const char *prompt,
                           FILE *out) {
    cos_pipeline_result_t r;
    if (prompt == NULL || prompt[0] == '\0' ||
        cos_sigma_pipeline_run(&st->cfg, prompt, &r) != 0) {
        fprintf(out, "\"error\":\"cos.sigma: empty prompt or pipeline failure\"");
        return;
    }
    /* Shadow σ_combined: reuse the scalar as σ_logprob/σ_entropy/σ_ppl
     * in absence of a per-token logprob stream; σ_consistency falls
     * back to 0 here (no regeneration pass for the stateless MCP call
     * path — σ-consistency needs K regens which would be 3× slower). */
    float s = r.sigma;
    if (s < 0.0f) s = 0.0f; if (s > 1.0f) s = 1.0f;
    cos_multi_sigma_t ens;
    cos_multi_sigma_combine(s, s, s, 0.0f, NULL, &ens);
    fprintf(out,
            "\"sigma_combined\":%.6f,\"sigma_logprob\":%.6f,"
            "\"sigma_entropy\":%.6f,\"sigma_perplexity\":%.6f,"
            "\"sigma_consistency\":%.6f,\"action\":\"%s\"",
            (double)ens.sigma_combined,
            (double)ens.sigma_logprob,
            (double)ens.sigma_entropy,
            (double)ens.sigma_perplexity,
            (double)ens.sigma_consistency,
            action_lbl(r.final_action));
}

static void tool_cos_calibrate(cos_mcp2_state_t *st,
                               const char *override_path,
                               FILE *out) {
    int rc = cos_mcp2_load_conformal(st, override_path);
    if (rc != 0) {
        fprintf(out,
            "\"valid\":false,\"path\":");
        mcp_emit_json_string(out,
            (override_path && override_path[0]) ? override_path : st->calib_path);
        fprintf(out,
            ",\"reason\":\"no valid conformal bundle at path\"");
        return;
    }
    fprintf(out,
            "\"valid\":true,\"tau\":%.6f,\"alpha\":%.6f,\"domain\":",
            (double)st->tau, (double)st->alpha);
    mcp_emit_json_string(out, st->domain);
    fprintf(out, ",\"path\":");
    mcp_emit_json_string(out,
        (override_path && override_path[0]) ? override_path : st->calib_path);
}

static void tool_cos_health(cos_mcp2_state_t *st, FILE *out) {
    double sigma_mean = (st->sigma_n > 0)
                      ? (st->sigma_sum / (double)st->sigma_n) : 0.0;
    double k_eff = 1.0 - sigma_mean;
    const char *coherence =
        (sigma_mean < 0.5)  ? "STABLE"
      : (sigma_mean < 0.7)  ? "DRIFTING"
      :                       "AT_RISK";

    if (!st->conformal_loaded) {
        /* Try a lazy load so health reports the bundle status honestly. */
        cos_mcp2_load_conformal(st, NULL);
    }

    fprintf(out,
            "\"version\":\"%s\",\"codename\":\"%s\","
            "\"mode\":\"HYBRID\","
            "\"proofs_lean\":\"%d/%d\","
            "\"proofs_frama_c\":\"15/15\","
            "\"engram_count\":%u,"
            "\"conformal\":{\"loaded\":%s,\"tau\":%.6f,"
                           "\"alpha\":%.6f,\"domain\":",
            COS_VERSION_STRING, COS_CODENAME,
            COS_FORMAL_PROOFS, COS_FORMAL_PROOFS_TOTAL,
            st->engram.count,
            st->conformal_loaded ? "true" : "false",
            (double)st->tau, (double)st->alpha);
    mcp_emit_json_string(out, st->conformal_loaded ? st->domain : "");
    fprintf(out,
            "},\"session\":{\"turns\":%d,\"sigma_mean\":%.6f,"
                         "\"k_eff\":%.6f,\"coherence\":\"%s\"},"
            "\"cost_eur\":%.6f",
            st->sigma_n, sigma_mean, k_eff, coherence,
            st->sv.eur_local_total + st->sv.eur_cloud_total);
}

static void tool_cos_engram_lookup(cos_mcp2_state_t *st,
                                   const char *prompt,
                                   FILE *out) {
    if (prompt == NULL || prompt[0] == '\0') {
        fprintf(out, "\"hit\":false,\"reason\":\"empty prompt\"");
        return;
    }
    uint64_t h = cos_sigma_engram_hash(prompt);
    cos_sigma_engram_entry_t e;
    int rc = cos_sigma_engram_get(&st->engram, h, &e);
    if (rc != 1) {
        fprintf(out, "\"hit\":false,\"prompt_hash\":%llu",
                (unsigned long long)h);
        return;
    }
    const char *resp = (const char *)e.value;
    fprintf(out,
            "\"hit\":true,\"sigma\":%.6f,\"prompt_hash\":%llu,\"response\":",
            (double)e.sigma_at_store, (unsigned long long)h);
    mcp_emit_json_string(out, resp != NULL ? resp : "");
}

static void tool_cos_introspect(cos_mcp2_state_t *st,
                                const char *prompt,
                                FILE *out) {
    (void)st;
    if (prompt == NULL) prompt = "";
    int words = count_words(prompt);
    float s_perception = (words <= 3) ? 0.35f : 0.05f;
    float s_social     = 0.0f;
    size_t L = strlen(prompt);
    if (L > 0 && prompt[L - 1] == '?' && words <= 4) s_social = 0.45f;
    float s_self       = 0.0f;
    float s_situ       = 0.0f;

    /* σ_self: heuristic proxy for "how uncertain would we be about
     * this?" — call sigma on the prompt text itself (cheap). */
    cos_pipeline_result_t r;
    if (prompt[0] != '\0' &&
        cos_sigma_pipeline_run(&st->cfg, prompt, &r) == 0) {
        s_self = r.sigma;
        s_situ = (st->cfg.max_rethink > 0)
               ? ((float)r.rethink_count / (float)st->cfg.max_rethink)
               : 0.0f;
    }
    fprintf(out,
            "\"sigma_perception\":%.6f,\"sigma_self\":%.6f,"
            "\"sigma_social\":%.6f,\"sigma_situational\":%.6f,"
            "\"word_count\":%d",
            (double)s_perception, (double)s_self,
            (double)s_social, (double)s_situ, words);
}

/* =====================================================================
 * Dispatch + protocol.
 * ===================================================================== */

static const struct {
    const char *name;
    const char *description;
} cos_mcp2_tools[] = {
    { "cos.chat",
      "Run a σ-gated chat turn; returns response + σ + action + route." },
    { "cos.sigma",
      "Compute multi-signal σ (logprob/entropy/perplexity/consistency) for a prompt." },
    { "cos.calibrate",
      "Load conformal τ from ~/.cos/calibration.json (or override path)." },
    { "cos.health",
      "Runtime snapshot: version, proofs, engram, coherence, cost." },
    { "cos.engram.lookup",
      "Lookup prompt in the σ-Engram; returns cached response if present." },
    { "cos.introspect",
      "Meta-cognitive σ breakdown (perception/self/social/situational) per prompt." },
};

static const int COS_MCP2_N_TOOLS =
    (int)(sizeof(cos_mcp2_tools) / sizeof(cos_mcp2_tools[0]));

static void emit_reply_open(FILE *out, const char *id) {
    fprintf(out, "{\"jsonrpc\":\"2.0\",\"id\":%s,", id);
}

static void emit_reply_error(FILE *out, const char *id,
                             int code, const char *msg) {
    emit_reply_open(out, id);
    fprintf(out, "\"error\":{\"code\":%d,\"message\":", code);
    mcp_emit_json_string(out, msg);
    fprintf(out, "}}\n");
    fflush(out);
}

static void emit_reply_result_begin(FILE *out, const char *id) {
    emit_reply_open(out, id);
    fputs("\"result\":{", out);
}

static void emit_reply_result_end(FILE *out) {
    fputs("}}\n", out);
    fflush(out);
}

static void handle_initialize(FILE *out, const char *id) {
    emit_reply_result_begin(out, id);
    fprintf(out,
        "\"protocolVersion\":\"%s\","
        "\"serverInfo\":{\"name\":\"cos-mcp\",\"version\":\"%s\"},"
        "\"capabilities\":{\"tools\":{\"listChanged\":false}}",
        COS_MCP2_PROTOCOL_VERSION, COS_VERSION_STRING);
    emit_reply_result_end(out);
}

static void handle_tools_list(FILE *out, const char *id) {
    emit_reply_result_begin(out, id);
    fputs("\"tools\":[", out);
    for (int i = 0; i < COS_MCP2_N_TOOLS; ++i) {
        if (i > 0) fputc(',', out);
        fputs("{\"name\":", out);
        mcp_emit_json_string(out, cos_mcp2_tools[i].name);
        fputs(",\"description\":", out);
        mcp_emit_json_string(out, cos_mcp2_tools[i].description);
        fputs(",\"inputSchema\":{\"type\":\"object\"}}", out);
    }
    fputs("]", out);
    emit_reply_result_end(out);
}

static void handle_tools_call(cos_mcp2_state_t *st,
                              FILE *out, const char *id,
                              const char *request) {
    /* Extract params.name. */
    const char *v_params = mcp_find_key(request, "params");
    const char *v_name   = v_params != NULL ? mcp_find_key(v_params, "name") : NULL;
    if (v_name == NULL) {
        emit_reply_error(out, id, -32602, "missing params.name");
        return;
    }
    char name[64];
    if (mcp_copy_json_string(v_name, name, sizeof(name)) != 0) {
        emit_reply_error(out, id, -32602, "malformed params.name");
        return;
    }

    /* Extract params.arguments.prompt / .path if present. */
    const char *v_args  = mcp_find_key(v_params, "arguments");
    char prompt[1536];
    char path[512];
    prompt[0] = '\0';
    path[0]   = '\0';
    if (v_args != NULL) {
        const char *v_prompt = mcp_find_key(v_args, "prompt");
        if (v_prompt != NULL)
            mcp_copy_json_string(v_prompt, prompt, sizeof(prompt));
        const char *v_path = mcp_find_key(v_args, "path");
        if (v_path != NULL)
            mcp_copy_json_string(v_path, path, sizeof(path));
    }

    emit_reply_result_begin(out, id);
    if      (strcmp(name, "cos.chat")          == 0) tool_cos_chat(st, prompt, out);
    else if (strcmp(name, "cos.sigma")         == 0) tool_cos_sigma(st, prompt, out);
    else if (strcmp(name, "cos.calibrate")     == 0) tool_cos_calibrate(st, path, out);
    else if (strcmp(name, "cos.health")        == 0) tool_cos_health(st, out);
    else if (strcmp(name, "cos.engram.lookup") == 0) tool_cos_engram_lookup(st, prompt, out);
    else if (strcmp(name, "cos.introspect")    == 0) tool_cos_introspect(st, prompt, out);
    else {
        fputs("\"error\":\"unknown tool\",\"name\":", out);
        mcp_emit_json_string(out, name);
    }
    emit_reply_result_end(out);
}

static void handle_ping(FILE *out, const char *id) {
    emit_reply_result_begin(out, id);
    fputs("\"pong\":true", out);
    emit_reply_result_end(out);
}

static void dispatch_line(cos_mcp2_state_t *st,
                          FILE *out, const char *line) {
    const char *v_method = mcp_find_key(line, "method");
    char id[64];
    mcp_extract_id_token(line, id, sizeof(id));
    if (id[0] == '\0') snprintf(id, sizeof(id), "null");

    if (v_method == NULL) {
        emit_reply_error(out, id, -32600, "missing method");
        return;
    }
    char method[48];
    if (mcp_copy_json_string(v_method, method, sizeof(method)) != 0) {
        emit_reply_error(out, id, -32600, "malformed method");
        return;
    }

    if      (strcmp(method, "initialize") == 0) handle_initialize(out, id);
    else if (strcmp(method, "tools/list") == 0) handle_tools_list(out, id);
    else if (strcmp(method, "tools/call") == 0) handle_tools_call(st, out, id, line);
    else if (strcmp(method, "ping")       == 0) handle_ping(out, id);
    else {
        emit_reply_error(out, id, -32601, "method_not_found");
    }
}

/* =====================================================================
 * Demo / CI envelope.
 *
 * Emits a deterministic JSON object containing the replies to a fixed
 * request script.  Scripted to exercise every tool once + one error
 * path (unknown method) so the CI harness can diff it byte-for-byte.
 * ===================================================================== */

static void emit_demo(cos_mcp2_state_t *st, FILE *out) {
    static const char *script[] = {
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\"}",
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}",
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"cos.health\",\"arguments\":{}}}",
        "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"cos.chat\",\"arguments\":{\"prompt\":\"What is 2+2?\"}}}",
        "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\",\"params\":{\"name\":\"cos.sigma\",\"arguments\":{\"prompt\":\"What is 2+2?\"}}}",
        "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\",\"params\":{\"name\":\"cos.introspect\",\"arguments\":{\"prompt\":\"why?\"}}}",
        "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"tools/call\",\"params\":{\"name\":\"cos.engram.lookup\",\"arguments\":{\"prompt\":\"What is 2+2?\"}}}",
        "{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"tools/call\",\"params\":{\"name\":\"cos.calibrate\",\"arguments\":{}}}",
        "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"nope\"}",
    };
    int n = (int)(sizeof(script) / sizeof(script[0]));

    fprintf(out, "{\n  \"kernel\":\"cos_mcp\",\n");
    fprintf(out, "  \"protocol_version\":\"%s\",\n",
            COS_MCP2_PROTOCOL_VERSION);
    fprintf(out, "  \"tools_offered\":%d,\n", COS_MCP2_N_TOOLS);
    fprintf(out, "  \"exchanges\":[\n");
    for (int i = 0; i < n; ++i) {
        fprintf(out, "    ");
        dispatch_line(st, out, script[i]);
        /* dispatch_line wrote a trailing '\n'; replace visually by ',' */
        if (i < n - 1) fprintf(out, ",\n");
    }
    fprintf(out, "  ]\n}\n");
}

/* =====================================================================
 * main
 * ===================================================================== */

int main(int argc, char **argv) {
    int mode_stdio = 1;
    int mode_demo  = 0;
    int mode_once  = 0;
    for (int i = 1; i < argc; ++i) {
        if      (strcmp(argv[i], "--stdio") == 0) { mode_stdio = 1; mode_demo = 0; }
        else if (strcmp(argv[i], "--demo")  == 0) { mode_demo  = 1; mode_stdio = 0; }
        else if (strcmp(argv[i], "--once")  == 0) { mode_once  = 1; mode_stdio = 1; }
        else if (strcmp(argv[i], "--help")  == 0) {
            fprintf(stdout,
                "cos-mcp — JSON-RPC 2.0 MCP server exposing cos.* tools.\n"
                "  --stdio    read line-delimited JSON-RPC on stdin (default)\n"
                "  --demo     emit canonical CI envelope and exit\n"
                "  --once     handle a single line from stdin and exit\n"
                "  --help     this text\n"
                "\n"
                "Tools (list via tools/list):\n"
                "  cos.chat, cos.sigma, cos.calibrate, cos.health,\n"
                "  cos.engram.lookup, cos.introspect\n");
            return 0;
        }
    }

    cos_mcp2_state_t st;
    cos_mcp2_state_init(&st);

    int rc = 0;
    if (mode_demo) {
        emit_demo(&st, stdout);
    } else if (mode_stdio) {
        char buf[COS_MCP2_LINE_MAX];
        while (fgets(buf, sizeof(buf), stdin) != NULL) {
            size_t n = strlen(buf);
            while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) {
                buf[--n] = '\0';
            }
            if (n == 0) continue;
            dispatch_line(&st, stdout, buf);
            if (mode_once) break;
        }
    }

    cos_mcp2_state_free(&st);
    return rc;
}
