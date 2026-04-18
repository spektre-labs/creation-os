/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v111.2 — σ-Reasoning Endpoint implementation.
 *
 * See reason.h for the contract.  This file is intentionally small —
 * the heavy lifting is delegated to v101 (generation + σ-channel math).
 * v62 / v64 / v45 are referenced here only as *method names*, not as
 * separate binary dependencies; the actual integer kernels in
 * `src/v62`, `src/v64`, `src/v45` remain the authoritative proofs of
 * their respective semantics and continue to ship their own
 * `--self-test` targets.
 */
#include "reason.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------- prompt stems ---------------------------- */

/* Eight pre-registered stems.  Index 0 is the identity.  These are
 * deterministic prompt perturbations that make the k candidates
 * differentiable without temperature sampling (which the v101 greedy
 * API does not expose today).  Each stem is an English-only cue; no
 * content leaks through to the model response because the stem is
 * prepended *to the prompt*, not echoed back.
 *
 * Stems are pre-registered here so the σ statistics per stem are
 * interpretable across runs — a specific stem id means the same thing
 * today and after a rebuild. */
static const char *const k_stems[COS_V111_REASON_MAX_CANDIDATES] = {
    "",                                                  /* 0 identity */
    "Think step by step, then give the final answer.\n\n",
    "Be concise. Answer directly.\n\n",
    "Consider the inverse: what if the claim were false? Then answer.\n\n",
    "First restate the question in your own words, then answer.\n\n",
    "List the assumptions you are making, then answer.\n\n",
    "Name two plausible alternative answers, then pick one.\n\n",
    "Answer, then rate your own confidence from 0 to 1.\n\n",
};

const char *cos_v111_reason_stem(int stem_id)
{
    if (stem_id < 0 || stem_id >= COS_V111_REASON_MAX_CANDIDATES) return "";
    return k_stems[stem_id];
}

/* ---------------------------- defaults ------------------------------- */

void cos_v111_reason_opts_defaults(cos_v111_reason_opts_t *o)
{
    if (!o) return;
    o->k_candidates   = 3;
    o->max_tokens     = 128;
    o->tau_abstain    = 0.70f;
    o->waypoint_delta = 0.08f;
    o->min_margin     = 0.005f;
}

/* ----------------------- σ aggregation helper ------------------------ */

static float _sigma_product_from_profile(const float *p)
{
    const float EPS = 1e-6f;
    double logsum = 0.0;
    for (int i = 0; i < COS_V101_SIGMA_CHANNELS; i++) {
        float v = p[i];
        if (v < EPS) v = EPS;
        logsum += log((double)v);
    }
    return (float)exp(logsum / (double)COS_V101_SIGMA_CHANNELS);
}

static float _sigma_mean_from_profile(const float *p)
{
    double s = 0.0;
    for (int i = 0; i < COS_V101_SIGMA_CHANNELS; i++) s += (double)p[i];
    return (float)(s / (double)COS_V101_SIGMA_CHANNELS);
}

/* ------------------------------ driver ------------------------------- */

int cos_v111_reason_run(cos_v101_bridge_t *bridge,
                        const char *prompt,
                        const cos_v111_reason_opts_t *opts_in,
                        cos_v111_reason_report_t *out)
{
    if (!bridge || !prompt || !out) return -1;
    cos_v111_reason_opts_t opts;
    if (opts_in) opts = *opts_in;
    else         cos_v111_reason_opts_defaults(&opts);

    if (opts.k_candidates < 1) opts.k_candidates = 1;
    if (opts.k_candidates > COS_V111_REASON_MAX_CANDIDATES)
        opts.k_candidates = COS_V111_REASON_MAX_CANDIDATES;
    if (opts.max_tokens < 1) opts.max_tokens = 1;

    memset(out, 0, sizeof *out);
    out->n_candidates = opts.k_candidates;
    out->chosen_index = -1;
    out->abstained    = 0;
    out->waypoint_match = 1;

    char expanded[5120];
    double sum_prod = 0.0;
    int best_i = -1;
    float best_prod = 1.0f + 1e-6f;
    int all_failed = 1;

    for (int i = 0; i < opts.k_candidates; i++) {
        const char *stem = cos_v111_reason_stem(i);
        int w = snprintf(expanded, sizeof expanded, "%s%s", stem, prompt);
        if (w < 0 || w >= (int)sizeof expanded) continue;

        cos_v111_reason_candidate_t *c = &out->candidates[i];
        c->stem_id = i;

        int abstained = 0;
        int n = cos_v101_bridge_generate(bridge,
                                         expanded,
                                         /*until=*/NULL, /*n_until=*/0,
                                         opts.max_tokens,
                                         /*sigma_threshold=*/0.0f,
                                         c->text, (int)sizeof c->text - 1,
                                         c->sigma_profile,
                                         &abstained);
        if (n < 0) { c->n_text = 0; continue; }
        c->n_text = n;
        if (n < (int)sizeof c->text) c->text[n] = 0;
        c->abstained     = abstained;
        c->sigma_mean    = _sigma_mean_from_profile(c->sigma_profile);
        c->sigma_product = _sigma_product_from_profile(c->sigma_profile);
        sum_prod += (double)c->sigma_product;
        all_failed = 0;

        if (c->sigma_product < best_prod) {
            best_prod = c->sigma_product;
            best_i    = i;
        }
    }

    if (all_failed) {
        out->abstained = 1;
        out->abstain_reason = "all candidates failed to generate";
        return 0;
    }

    out->chosen_index         = best_i;
    out->chosen_sigma_product = best_prod;
    out->mean_sigma_product   = (float)(sum_prod / (double)opts.k_candidates);
    out->margin               = out->mean_sigma_product - best_prod; /* ≥ 0 if healthy */

    /* v62 / v64 — EBM path-consistency gate and MCTS-style ranking are
     * expressed here simply as:  "do the candidates disagree enough to
     * carry rank signal?"  If not (margin < min_margin), abstain. */
    if (out->margin < opts.min_margin) {
        out->abstained      = 1;
        out->abstain_reason = "candidates undifferentiated (margin < min_margin)";
    }

    /* τ abstain gate. */
    if (best_prod > opts.tau_abstain) {
        out->abstained      = 1;
        out->abstain_reason = "sigma_product exceeded tau_abstain";
    }

    /* v45 introspection waypoint check.  The predicted σ is
     * best_prod; the observed σ is a short re-generation of the
     * chosen candidate's own prompt with a new stem 0.  If they
     * disagree by more than waypoint_delta the model's confidence is
     * not self-consistent on this input and we flag the answer but do
     * not force an abstain (operator decides downstream). */
    out->waypoint_sigma_predicted = best_prod;
    out->waypoint_sigma_observed  = best_prod;
    out->waypoint_match           = 1;
    if (best_i >= 0 && !out->abstained) {
        const char *base_stem = cos_v111_reason_stem(0);
        int w = snprintf(expanded, sizeof expanded, "%s%s", base_stem, prompt);
        if (w > 0 && w < (int)sizeof expanded) {
            float probe_profile[COS_V101_SIGMA_CHANNELS] = {0};
            char probe_buf[512];
            int probe_ab = 0;
            int n = cos_v101_bridge_generate(bridge, expanded,
                                             NULL, 0,
                                             /*max_tokens=*/32,
                                             /*sigma_threshold=*/0.0f,
                                             probe_buf, (int)sizeof probe_buf - 1,
                                             probe_profile,
                                             &probe_ab);
            if (n >= 0) {
                float probe_prod = _sigma_product_from_profile(probe_profile);
                out->waypoint_sigma_observed = probe_prod;
                float d = probe_prod - best_prod;
                if (d < 0.0f) d = -d;
                out->waypoint_match = (d <= opts.waypoint_delta) ? 1 : 0;
            }
        }
    }

    return 0;
}

/* --------------------------- JSON writer ----------------------------- */

/* Minimal JSON string escape into `out` capacity `cap`.  Returns bytes
 * written (not NUL-terminated) or -1 on truncation. */
static int _json_escape(const char *in, int n_in, char *out, int cap)
{
    int w = 0;
    for (int i = 0; i < n_in; i++) {
        unsigned char c = (unsigned char)in[i];
        const char *esc = NULL;
        char hex[8];
        switch (c) {
            case '"':  esc = "\\\""; break;
            case '\\': esc = "\\\\"; break;
            case '\n': esc = "\\n"; break;
            case '\r': esc = "\\r"; break;
            case '\t': esc = "\\t"; break;
            case '\b': esc = "\\b"; break;
            case '\f': esc = "\\f"; break;
            default:
                if (c < 0x20) {
                    snprintf(hex, sizeof hex, "\\u%04x", c);
                    esc = hex;
                }
                break;
        }
        if (esc) {
            int l = (int)strlen(esc);
            if (w + l >= cap) return -1;
            memcpy(out + w, esc, l); w += l;
        } else {
            if (w + 1 >= cap) return -1;
            out[w++] = (char)c;
        }
    }
    if (w >= cap) return -1;
    out[w] = 0;
    return w;
}

int cos_v111_reason_report_to_json(const cos_v111_reason_report_t *r,
                                   const char *model_id,
                                   char *buf, int cap)
{
    if (!r || !buf || cap <= 0) return -1;
    int w = 0;
    int n;
    const char *mid = model_id ? model_id : "creation-os";
    n = snprintf(buf + w, cap - w,
                 "{\"object\":\"reasoning\",\"model\":\"%s\","
                 "\"k_candidates\":%d,"
                 "\"chosen_index\":%d,"
                 "\"chosen_sigma_product\":%.6f,"
                 "\"mean_sigma_product\":%.6f,"
                 "\"margin\":%.6f,"
                 "\"abstained\":%s,"
                 "\"abstain_reason\":%s%s%s,"
                 "\"waypoint_match\":%s,"
                 "\"waypoint_sigma_predicted\":%.6f,"
                 "\"waypoint_sigma_observed\":%.6f,"
                 "\"chosen\":",
                 mid,
                 r->n_candidates,
                 r->chosen_index,
                 (double)r->chosen_sigma_product,
                 (double)r->mean_sigma_product,
                 (double)r->margin,
                 r->abstained ? "true" : "false",
                 r->abstain_reason ? "\"" : "",
                 r->abstain_reason ? r->abstain_reason : "null",
                 r->abstain_reason ? "\"" : "",
                 r->waypoint_match ? "true" : "false",
                 (double)r->waypoint_sigma_predicted,
                 (double)r->waypoint_sigma_observed);
    if (n < 0 || n >= cap - w) return -1;
    w += n;

    /* chosen block */
    if (r->chosen_index >= 0 && r->chosen_index < r->n_candidates) {
        const cos_v111_reason_candidate_t *c = &r->candidates[r->chosen_index];
        n = snprintf(buf + w, cap - w,
                     "{\"text\":\"");
        if (n < 0 || n >= cap - w) return -1;
        w += n;
        n = _json_escape(c->text, c->n_text, buf + w, cap - w);
        if (n < 0) return -1;
        w += n;
        n = snprintf(buf + w, cap - w,
                     "\",\"sigma_product\":%.6f,\"sigma_mean\":%.6f,"
                     "\"sigma_profile\":[%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f],"
                     "\"stem_id\":%d}",
                     (double)c->sigma_product, (double)c->sigma_mean,
                     (double)c->sigma_profile[0], (double)c->sigma_profile[1],
                     (double)c->sigma_profile[2], (double)c->sigma_profile[3],
                     (double)c->sigma_profile[4], (double)c->sigma_profile[5],
                     (double)c->sigma_profile[6], (double)c->sigma_profile[7],
                     c->stem_id);
    } else {
        n = snprintf(buf + w, cap - w, "null");
    }
    if (n < 0 || n >= cap - w) return -1;
    w += n;

    /* candidates array */
    n = snprintf(buf + w, cap - w, ",\"candidates\":[");
    if (n < 0 || n >= cap - w) return -1;
    w += n;
    for (int i = 0; i < r->n_candidates; i++) {
        const cos_v111_reason_candidate_t *c = &r->candidates[i];
        if (i > 0) { if (w + 1 >= cap) return -1; buf[w++] = ','; }
        n = snprintf(buf + w, cap - w, "{\"text\":\"");
        if (n < 0 || n >= cap - w) return -1;
        w += n;
        n = _json_escape(c->text, c->n_text, buf + w, cap - w);
        if (n < 0) return -1;
        w += n;
        n = snprintf(buf + w, cap - w,
                     "\",\"sigma_product\":%.6f,\"sigma_mean\":%.6f,"
                     "\"stem_id\":%d}",
                     (double)c->sigma_product, (double)c->sigma_mean,
                     c->stem_id);
        if (n < 0 || n >= cap - w) return -1;
        w += n;
    }
    if (w + 2 >= cap) return -1;
    buf[w++] = ']';
    buf[w++] = '}';
    if (w >= cap) return -1;
    buf[w] = 0;
    return w;
}

/* ---------------------------- self-test ------------------------------ */

int cos_v111_reason_self_test(void)
{
    int pass = 0, fail = 0;
    #define CHECK(cond) do { \
        if (cond) pass++; \
        else { fail++; fprintf(stderr, "v111 reason FAIL: %s\n", #cond); } \
    } while (0)

    /* Stem registry */
    CHECK(strcmp(cos_v111_reason_stem(0), "") == 0);
    CHECK(cos_v111_reason_stem(-1)[0] == 0);
    CHECK(cos_v111_reason_stem(8)[0] == 0);
    CHECK(strlen(cos_v111_reason_stem(1)) > 0);
    CHECK(strlen(cos_v111_reason_stem(7)) > 0);

    /* Defaults */
    cos_v111_reason_opts_t o;
    cos_v111_reason_opts_defaults(&o);
    CHECK(o.k_candidates == 3);
    CHECK(o.max_tokens == 128);
    CHECK(o.tau_abstain > 0.0f && o.tau_abstain < 1.0f);
    CHECK(o.waypoint_delta > 0.0f);
    CHECK(o.min_margin >= 0.0f);

    /* σ helpers arithmetic */
    float flat[COS_V101_SIGMA_CHANNELS] = {0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f};
    float p = _sigma_product_from_profile(flat);
    float m = _sigma_mean_from_profile(flat);
    CHECK(fabsf(p - 0.5f) < 1e-5f);
    CHECK(fabsf(m - 0.5f) < 1e-5f);
    float peaked[COS_V101_SIGMA_CHANNELS] = {0.9f,0.9f,0.9f,0.9f,0.9f,0.9f,0.9f,0.01f};
    float pp = _sigma_product_from_profile(peaked);
    float mm = _sigma_mean_from_profile(peaked);
    CHECK(pp < mm);  /* geometric mean punishes the one low channel */

    /* JSON serialiser round-trip on a synthetic report */
    cos_v111_reason_report_t r;
    memset(&r, 0, sizeof r);
    r.n_candidates = 2;
    r.chosen_index = 0;
    r.chosen_sigma_product = 0.234f;
    r.mean_sigma_product   = 0.300f;
    r.margin               = 0.066f;
    r.abstained = 0;
    r.abstain_reason = NULL;
    r.waypoint_match = 1;
    r.waypoint_sigma_predicted = 0.234f;
    r.waypoint_sigma_observed  = 0.241f;
    for (int k = 0; k < 2; k++) {
        cos_v111_reason_candidate_t *c = &r.candidates[k];
        snprintf(c->text, sizeof c->text, "answer %d \"with\" \\escape\n", k);
        c->n_text = (int)strlen(c->text);
        c->sigma_mean = 0.4f + 0.01f * k;
        c->sigma_product = 0.3f + 0.01f * k;
        for (int j = 0; j < COS_V101_SIGMA_CHANNELS; j++)
            c->sigma_profile[j] = 0.25f + 0.01f * j;
        c->stem_id = k;
    }
    char json[8192];
    int n = cos_v111_reason_report_to_json(&r, "bitnet-b1.58-2B-test", json, sizeof json);
    CHECK(n > 0);
    CHECK(strstr(json, "\"object\":\"reasoning\"") != NULL);
    CHECK(strstr(json, "\"chosen_index\":0") != NULL);
    CHECK(strstr(json, "\"abstained\":false") != NULL);
    CHECK(strstr(json, "\"waypoint_match\":true") != NULL);
    CHECK(strstr(json, "\\\"with\\\"") != NULL);  /* escape applied */
    CHECK(strstr(json, "\"candidates\":[") != NULL);
    CHECK(strstr(json, "\"stem_id\":1") != NULL);

    /* Abstain-reason string round-trip */
    r.abstained = 1;
    r.abstain_reason = "sigma_product exceeded tau_abstain";
    n = cos_v111_reason_report_to_json(&r, NULL, json, sizeof json);
    CHECK(n > 0);
    CHECK(strstr(json, "\"abstained\":true") != NULL);
    CHECK(strstr(json, "\"abstain_reason\":\"sigma_product exceeded tau_abstain\"") != NULL);

    /* Truncation resistance: very tight buffer must return -1 */
    char tiny[8];
    int nt = cos_v111_reason_report_to_json(&r, NULL, tiny, sizeof tiny);
    CHECK(nt < 0);

    printf("v111 σ-Reason: %d PASS / %d FAIL\n", pass, fail);
    return fail == 0 ? 0 : 1;
    #undef CHECK
}
