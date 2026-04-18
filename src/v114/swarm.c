/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v114 σ-Swarm — implementation.
 */
#include "swarm.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- */

void cos_v114_config_defaults(cos_v114_config_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof *cfg);
    cfg->n                   = 0;
    cfg->consensus_epsilon   = 0.15f;
    cfg->consensus_threshold = 2;
    snprintf(cfg->routing, sizeof cfg->routing, "sigma_product_min");
}

/* Tiny helper: find the start of the `[swarm]` TOML section and
 * return a pointer to the body after the newline, or NULL.  Sections
 * end at the next `[xxx]` header or EOF. */
static const char *find_section(const char *text, const char *name)
{
    if (!text || !name) return NULL;
    char pat[32];
    int n = snprintf(pat, sizeof pat, "[%s]", name);
    if (n < 0) return NULL;
    const char *p = text;
    while ((p = strstr(p, pat)) != NULL) {
        /* require start of line or buffer */
        if (p == text || p[-1] == '\n') {
            const char *nl = strchr(p, '\n');
            return nl ? nl + 1 : p + strlen(pat);
        }
        ++p;
    }
    return NULL;
}

/* Extract a single specialist object `{name = "...", gguf = "...",
 * role = "..."}` from the text at `*cursor`, advancing cursor past
 * the close brace.  Returns 1 on parsed, 0 on no-object-here,
 * -1 on malformed. */
static int parse_specialist(const char **cursor,
                            cos_v114_specialist_t *out)
{
    const char *p = *cursor;
    while (*p && isspace((unsigned char)*p)) ++p;
    if (*p != '{') return 0;
    const char *open  = p + 1;
    const char *close = strchr(open, '}');
    if (!close) return -1;
    memset(out, 0, sizeof *out);

    /* Helper: match `key = "value"` within [open,close). */
    struct { const char *k; char *d; size_t cap; } fields[] = {
        { "name", out->name,      sizeof out->name },
        { "gguf", out->gguf_path, sizeof out->gguf_path },
        { "role", out->role,      sizeof out->role }
    };
    for (size_t i = 0; i < sizeof fields / sizeof fields[0]; ++i) {
        char pat[64];
        snprintf(pat, sizeof pat, "%s", fields[i].k);
        const char *kp = open;
        while (kp < close) {
            while (kp < close && isspace((unsigned char)*kp)) ++kp;
            const char *hit = strstr(kp, pat);
            if (!hit || hit >= close) break;
            const char *eq = strchr(hit, '=');
            if (!eq || eq >= close) break;
            const char *qs = strchr(eq, '"');
            if (!qs || qs >= close) break;
            const char *qe = strchr(qs + 1, '"');
            if (!qe || qe >= close) break;
            size_t len = (size_t)(qe - (qs + 1));
            if (len >= fields[i].cap) len = fields[i].cap - 1;
            memcpy(fields[i].d, qs + 1, len);
            fields[i].d[len] = '\0';
            break;
        }
    }
    *cursor = close + 1;
    return (out->name[0] && out->gguf_path[0]) ? 1 : -1;
}

int cos_v114_parse_config(const char *toml_text, cos_v114_config_t *out)
{
    if (!out) return -1;
    cos_v114_config_defaults(out);
    if (!toml_text) return 0;

    const char *sec = find_section(toml_text, "swarm");
    if (!sec) return 0;

    /* Bound section to the next `[` header or EOF (whichever first). */
    const char *sec_end = sec;
    while (*sec_end) {
        const char *nl = strchr(sec_end, '\n');
        const char *next = nl ? nl + 1 : sec_end + strlen(sec_end);
        const char *q = sec_end;
        while (q < next && isspace((unsigned char)*q)) ++q;
        if (*q == '[' && q != sec) break;
        if (!nl) { sec_end = next; break; }
        sec_end = next;
    }

    /* specialists = [ {...}, {...}, ... ] */
    const char *sp = strstr(sec, "specialists");
    if (sp && sp < sec_end) {
        const char *open = strchr(sp, '[');
        if (open && open < sec_end) {
            const char *cursor = open + 1;
            while (cursor < sec_end &&
                   out->n < COS_V114_MAX_SPECIALISTS) {
                while (cursor < sec_end &&
                       isspace((unsigned char)*cursor)) ++cursor;
                if (*cursor == ']') break;
                if (*cursor == ',') { ++cursor; continue; }
                if (*cursor != '{')  { ++cursor; continue; }
                cos_v114_specialist_t s;
                int rc = parse_specialist(&cursor, &s);
                if (rc == 1) out->specialists[out->n++] = s;
                else if (rc < 0) return -1;
                else ++cursor;
            }
        }
    }

    /* Optional scalars. */
    const char *rp = strstr(sec, "routing");
    if (rp && rp < sec_end) {
        const char *qs = strchr(rp, '"');
        if (qs && qs < sec_end) {
            const char *qe = strchr(qs + 1, '"');
            if (qe && qe < sec_end) {
                size_t len = (size_t)(qe - (qs + 1));
                if (len >= sizeof out->routing) len = sizeof out->routing - 1;
                memcpy(out->routing, qs + 1, len);
                out->routing[len] = '\0';
            }
        }
    }
    const char *ct = strstr(sec, "consensus_threshold");
    if (ct && ct < sec_end) {
        const char *eq = strchr(ct, '=');
        if (eq && eq < sec_end) {
            long v = strtol(eq + 1, NULL, 10);
            if (v >= 1 && v <= COS_V114_MAX_SPECIALISTS)
                out->consensus_threshold = (int)v;
        }
    }
    const char *ce = strstr(sec, "consensus_epsilon");
    if (ce && ce < sec_end) {
        const char *eq = strchr(ce, '=');
        if (eq && eq < sec_end) {
            double v = strtod(eq + 1, NULL);
            if (v >= 0.0 && v <= 1.0) out->consensus_epsilon = (float)v;
        }
    }

    return (int)out->n;
}

/* ---------------------------------------------------------------- */

int cos_v114_arbitrate(const cos_v114_candidate_t *cands, size_t n,
                       const cos_v114_config_t *cfg,
                       cos_v114_decision_t *d)
{
    if (!cands || !d || !cfg) return -1;
    memset(d, 0, sizeof *d);
    d->winner_index = -1;
    d->runner_up_index = -1;
    snprintf(d->routing, sizeof d->routing, "%s", cfg->routing);
    snprintf(d->parallel_mode, sizeof d->parallel_mode, "sequential");

    if (n == 0) { d->all_abstained = 1; return 0; }

    int    active = 0;
    int    best_i = -1, second_i = -1;
    float  best_s = 2.f, second_s = 2.f;
    for (size_t i = 0; i < n; ++i) {
        if (cands[i].abstained) continue;
        ++active;
        float s = cands[i].sigma_product;
        if (s < best_s) {
            second_s = best_s;  second_i = best_i;
            best_s   = s;       best_i   = (int)i;
        } else if (s < second_s) {
            second_s = s; second_i = (int)i;
        }
    }

    if (active == 0) { d->all_abstained = 1; return 0; }

    d->winner_index  = best_i;
    d->winner_sigma  = best_s;
    d->runner_up_index = second_i;
    d->runner_up_sigma = (second_i >= 0) ? second_s : -1.f;

    /* Resonance: at least `consensus_threshold` specialists all
     * produced the same answer (byte-equal content, modulo a tiny
     * whitespace trim) AND their σ-product is within epsilon of
     * the winner. */
    if (best_i >= 0) {
        int agree = 1;
        const char *wc = cands[best_i].content;
        for (size_t i = 0; i < n; ++i) {
            if ((int)i == best_i || cands[i].abstained) continue;
            if (fabsf(cands[i].sigma_product - best_s) >
                cfg->consensus_epsilon) continue;
            if (strcmp(cands[i].content, wc) == 0) ++agree;
        }
        if (agree >= cfg->consensus_threshold) d->resonance = 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- */

/* FNV-1a 32-bit hash — deterministic, tiny, good enough for
 * synthetic stub σ-values. */
static uint32_t fnv1a(const char *s) {
    uint32_t h = 0x811c9dc5u;
    for (; *s; ++s) {
        h ^= (uint8_t)*s;
        h *= 0x01000193u;
    }
    return h;
}

/* Role affinity heuristic: does the role tag describe the prompt? */
static int role_matches(const char *role, const char *prompt)
{
    if (!role || !prompt) return 0;
    const char *keys[][3] = {
        {"math", "logic", "reason"},
        {"code", "program", "python"},
        {"general", "chat", "knowledge"}
    };
    const char *role_lc[64] = {0};
    (void)role_lc;
    for (size_t i = 0; i < 3; ++i) {
        int has_role = 0, has_prompt = 0;
        for (int k = 0; k < 3; ++k) {
            if (!keys[i][k]) break;
            if (strcasestr(role,   keys[i][k])) has_role = 1;
            if (strcasestr(prompt, keys[i][k])) has_prompt = 1;
        }
        if (has_role && has_prompt) return 1;
    }
    return 0;
}

int cos_v114_stub_run(const cos_v114_config_t *cfg, const char *prompt,
                      cos_v114_candidate_t *out, size_t cap)
{
    if (!cfg || !prompt || !out || cap == 0) return 0;
    size_t k = cfg->n < cap ? cfg->n : cap;
    for (size_t i = 0; i < k; ++i) {
        memset(&out[i], 0, sizeof out[i]);
        out[i].specialist_name = cfg->specialists[i].name;
        out[i].role            = cfg->specialists[i].role;

        /* σ bias: role match → lower σ, mismatch → higher. */
        uint32_t h = fnv1a(prompt) ^ fnv1a(cfg->specialists[i].name);
        float base = ((h & 0xFFFF) / 65535.f);           /* 0..1 */
        if (role_matches(cfg->specialists[i].role, prompt)) {
            base *= 0.40f;   /* squash toward 0 — confident */
        } else {
            base = 0.5f + 0.5f * base;                   /* .5..1 */
        }
        /* Build an 8-channel profile by jittering around `base`. */
        for (int c = 0; c < 8; ++c) {
            uint32_t hc = fnv1a(cfg->specialists[i].name) + (uint32_t)c;
            float jitter = ((hc & 0x3FF) / 1023.f - 0.5f) * 0.10f;
            float v = base + jitter;
            if (v < 0.f) v = 0.f;
            if (v > 1.f) v = 1.f;
            out[i].sigma_profile[c] = v;
        }
        /* Aggregate. */
        double acc_m = 0.0, acc_p = 1.0;
        for (int c = 0; c < 8; ++c) {
            acc_m += out[i].sigma_profile[c];
            acc_p *= (double)(out[i].sigma_profile[c] < 1e-6
                                  ? 1e-6 : out[i].sigma_profile[c]);
        }
        out[i].sigma_mean    = (float)(acc_m / 8.0);
        out[i].sigma_product = (float)pow(acc_p, 1.0 / 8.0);
        snprintf(out[i].content, sizeof out[i].content,
                 "[stub:%s] response to prompt of length %zu",
                 cfg->specialists[i].name, strlen(prompt));
        out[i].abstained = (out[i].sigma_product > 0.95f) ? 1 : 0;
    }
    return (int)k;
}

/* ---------------------------------------------------------------- */

static int json_escape(const char *src, char *dst, size_t cap)
{
    size_t w = 0;
    for (const char *p = src; *p && w + 7 < cap; ++p) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  dst[w++] = '\\'; dst[w++] = '"';  break;
            case '\\': dst[w++] = '\\'; dst[w++] = '\\'; break;
            case '\n': dst[w++] = '\\'; dst[w++] = 'n';  break;
            case '\r': dst[w++] = '\\'; dst[w++] = 'r';  break;
            case '\t': dst[w++] = '\\'; dst[w++] = 't';  break;
            default:
                if (c < 0x20) {
                    int n = snprintf(dst + w, cap - w, "\\u%04x", c);
                    if (n < 0 || (size_t)n >= cap - w) break;
                    w += (size_t)n;
                } else dst[w++] = (char)c;
        }
    }
    dst[w] = '\0';
    return (int)w;
}

int cos_v114_build_response(const cos_v114_candidate_t *cands, size_t n,
                            const cos_v114_decision_t *dec,
                            const char *model_id, long created,
                            char *headers_out, size_t hcap,
                            char *body_out, size_t bcap)
{
    if (!cands || !dec || !headers_out || !body_out) return -1;

    const char *w_name = "none";
    float       w_sig  = -1.f;
    const char *ru_name = "";
    float       ru_sig  = -1.f;
    if (dec->winner_index >= 0 && (size_t)dec->winner_index < n) {
        w_name = cands[dec->winner_index].specialist_name;
        w_sig  = dec->winner_sigma;
    }
    if (dec->runner_up_index >= 0 && (size_t)dec->runner_up_index < n) {
        ru_name = cands[dec->runner_up_index].specialist_name;
        ru_sig  = dec->runner_up_sigma;
    }

    int hn = snprintf(headers_out, hcap,
        "X-COS-Specialist: %s\r\n"
        "X-COS-Specialist-Sigma: %.4f\r\n"
        "X-COS-Runner-Up: %s\r\n"
        "X-COS-Runner-Up-Sigma: %.4f\r\n"
        "X-COS-Consensus: %s\r\n"
        "X-COS-Parallel-Mode: %s\r\n"
        "X-COS-Routing: %s\r\n",
        w_name, (double)w_sig,
        ru_name[0] ? ru_name : "-", (double)ru_sig,
        dec->resonance ? "true" : "false",
        dec->parallel_mode, dec->routing);
    if (hn < 0 || (size_t)hn >= hcap) return -1;

    /* Body: OpenAI chat.completion shape with a creation_os.swarm
     * block listing every candidate's σ.  The content of the
     * winning specialist becomes the choices[0].message.content. */
    char content_esc[COS_V114_CONTENT_MAX * 2];
    const char *winner_content = (dec->winner_index >= 0)
        ? cands[dec->winner_index].content
        : "no specialist was confident";
    json_escape(winner_content, content_esc, sizeof content_esc);

    int w = 0;
    int rn = snprintf(body_out + w, bcap - w,
        "{"
        "\"id\":\"chatcmpl-swarm-%ld\","
        "\"object\":\"chat.completion\","
        "\"created\":%ld,"
        "\"model\":\"%s\","
        "\"choices\":[{"
            "\"index\":0,"
            "\"message\":{\"role\":\"assistant\",\"content\":\"%s\"},"
            "\"finish_reason\":\"%s\""
        "}],"
        "\"creation_os\":{"
            "\"swarm\":{"
                "\"winner\":\"%s\","
                "\"winner_sigma\":%.4f,"
                "\"runner_up\":\"%s\","
                "\"runner_up_sigma\":%.4f,"
                "\"resonance\":%s,"
                "\"routing\":\"%s\","
                "\"parallel_mode\":\"%s\","
                "\"candidates\":[",
        created, created, model_id ? model_id : "creation-os-swarm",
        content_esc,
        dec->all_abstained ? "no_confident_specialist" :
            (dec->resonance ? "swarm_resonance" : "swarm_winner"),
        w_name, (double)w_sig,
        ru_name[0] ? ru_name : "-", (double)ru_sig,
        dec->resonance ? "true" : "false",
        dec->routing, dec->parallel_mode);
    if (rn < 0 || (size_t)rn >= bcap - w) return -1;
    w += rn;

    for (size_t i = 0; i < n; ++i) {
        rn = snprintf(body_out + w, bcap - w,
            "%s{\"name\":\"%s\",\"role\":\"%s\","
            "\"sigma_product\":%.4f,\"sigma_mean\":%.4f,"
            "\"abstained\":%s}",
            (i == 0) ? "" : ",",
            cands[i].specialist_name ? cands[i].specialist_name : "-",
            cands[i].role ? cands[i].role : "-",
            (double)cands[i].sigma_product,
            (double)cands[i].sigma_mean,
            cands[i].abstained ? "true" : "false");
        if (rn < 0 || (size_t)rn >= bcap - w) return -1;
        w += rn;
    }

    rn = snprintf(body_out + w, bcap - w, "]}}}");
    if (rn < 0 || (size_t)rn >= bcap - w) return -1;
    w += rn;
    return w;
}

/* ---------------------------------------------------------------- */
/* Self-test                                                          */
/* ---------------------------------------------------------------- */

#define _CHECK(cond, msg) do {                      \
    if (!(cond)) { fprintf(stderr, "FAIL %s\n", (msg)); ++_fail; } \
    else ++_pass;                                   \
} while (0)

int cos_v114_self_test(void)
{
    int _pass = 0, _fail = 0;

    /* 1. TOML parse */
    const char *toml =
        "[server]\nport = 8080\n\n"
        "[swarm]\n"
        "specialists = [\n"
        "  { name = \"reasoning\", gguf = \"bitnet-2b.gguf\", role = \"math, logic\" },\n"
        "  { name = \"code\",      gguf = \"qwen-7b.gguf\",   role = \"programming\" },\n"
        "  { name = \"general\",   gguf = \"llama-8b.gguf\",  role = \"knowledge, chat\" }\n"
        "]\n"
        "routing = \"sigma_product_min\"\n"
        "consensus_threshold = 2\n"
        "consensus_epsilon = 0.10\n";
    cos_v114_config_t cfg;
    int n = cos_v114_parse_config(toml, &cfg);
    _CHECK(n == 3, "parse: 3 specialists");
    _CHECK(strcmp(cfg.specialists[0].name, "reasoning") == 0, "sp[0].name");
    _CHECK(strcmp(cfg.specialists[1].name, "code")      == 0, "sp[1].name");
    _CHECK(strcmp(cfg.specialists[2].name, "general")   == 0, "sp[2].name");
    _CHECK(strcmp(cfg.routing, "sigma_product_min")     == 0, "routing");
    _CHECK(cfg.consensus_threshold == 2, "threshold");
    _CHECK(fabsf(cfg.consensus_epsilon - 0.10f) < 1e-4f, "epsilon");

    /* 2. Stub run + arbitrate (code prompt → code specialist wins). */
    cos_v114_candidate_t cands[COS_V114_MAX_SPECIALISTS];
    int nc = cos_v114_stub_run(&cfg, "write a python program to compute primes",
                               cands, COS_V114_MAX_SPECIALISTS);
    _CHECK(nc == 3, "stub: 3 candidates");
    cos_v114_decision_t dec;
    int rc = cos_v114_arbitrate(cands, (size_t)nc, &cfg, &dec);
    _CHECK(rc == 0, "arbitrate ok");
    _CHECK(dec.winner_index >= 0, "winner chosen");
    _CHECK(strcmp(cands[dec.winner_index].specialist_name, "code") == 0,
           "code specialist wins the python prompt");
    _CHECK(dec.all_abstained == 0, "not all abstained");

    /* 3. Arbitrate — all-abstained corner case. */
    for (int i = 0; i < nc; ++i) cands[i].abstained = 1;
    rc = cos_v114_arbitrate(cands, (size_t)nc, &cfg, &dec);
    _CHECK(rc == 0, "abstain arbitrate ok");
    _CHECK(dec.all_abstained == 1, "all abstained flagged");
    _CHECK(dec.winner_index == -1, "no winner");

    /* 4. Resonance: two identical contents at low σ. */
    for (int i = 0; i < nc; ++i) {
        cands[i].abstained     = 0;
        cands[i].sigma_product = 0.20f + (float)i * 0.05f;
        snprintf(cands[i].content, sizeof cands[i].content,
                 "%s", (i == 2) ? "different" : "same_answer");
    }
    cfg.consensus_threshold = 2;
    cfg.consensus_epsilon   = 0.10f;
    rc = cos_v114_arbitrate(cands, (size_t)nc, &cfg, &dec);
    _CHECK(rc == 0, "resonance arbitrate ok");
    _CHECK(dec.resonance == 1, "resonance detected");

    /* 5. Response builder */
    char hdr[1024], bdy[8192];
    int  bn = cos_v114_build_response(cands, (size_t)nc, &dec,
                                      "test-swarm", 1700000000,
                                      hdr, sizeof hdr, bdy, sizeof bdy);
    _CHECK(bn > 0, "response built");
    _CHECK(strstr(hdr, "X-COS-Specialist:") != NULL, "header X-COS-Specialist");
    _CHECK(strstr(hdr, "X-COS-Consensus: true") != NULL, "header consensus");
    _CHECK(strstr(bdy, "\"resonance\":true") != NULL, "json resonance");
    _CHECK(strstr(bdy, "\"candidates\"") != NULL, "json candidates");

    if (_fail > 0) {
        fprintf(stderr, "v114 self-test: %d PASS / %d FAIL\n", _pass, _fail);
        return 1;
    }
    printf("v114 self-test: %d PASS / 0 FAIL\n", _pass);
    return 0;
}
