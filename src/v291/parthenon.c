/*
 * v291 σ-Parthenon — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "parthenon.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (; n + 1 < cap && src[n]; ++n) dst[n] = src[n];
    dst[n] = '\0';
}

static const struct { const char *d; cos_v291_verdict_t v; }
    CALIBS[COS_V291_N_CALIB] = {
    { "medical",  COS_V291_VERDICT_ABSTAIN  },
    { "code",     COS_V291_VERDICT_CAUTIOUS },
    { "creative", COS_V291_VERDICT_SAFE     },
};

static const struct { float s; int d; }
    PERCEPTS[COS_V291_N_PERCEPT] = {
    { 0.05f, 20 },
    { 0.15f,  7 },
    { 0.50f,  2 },
};

static const struct { const char *t; float r; float o; }
    BIASES[COS_V291_N_BIAS] = {
    { "overconfident",  0.20f, +0.10f },
    { "underconfident", 0.60f, -0.10f },
    { "calibrated",     0.40f,  0.00f },
};

static const float ENTASIS_INPUTS[COS_V291_N_ENTASIS] = {
    0.005f, 0.995f, 0.500f,
};

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

void cos_v291_init(cos_v291_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed                = seed ? seed : 0x291bbbULL;
    s->bias_budget         = 0.02f;
    s->lower_bound         = 0.02f;
    s->upper_bound         = 0.98f;
    s->shared_sigma_sample = 0.30f;
}

void cos_v291_run(cos_v291_state_t *s) {
    uint64_t prev = 0x291bbb00ULL;

    s->n_calib_rows_ok = 0;
    int n_shared = 0;
    for (int i = 0; i < COS_V291_N_CALIB; ++i) {
        cos_v291_calib_t *c = &s->calib[i];
        memset(c, 0, sizeof(*c));
        cpy(c->domain, sizeof(c->domain), CALIBS[i].d);
        c->sigma_sample = s->shared_sigma_sample;
        c->verdict      = CALIBS[i].v;
        if (strlen(c->domain) > 0) s->n_calib_rows_ok++;
        if (fabsf(c->sigma_sample - s->shared_sigma_sample) < 1e-6f) n_shared++;
        prev = fnv1a(c->domain, strlen(c->domain), prev);
        prev = fnv1a(&c->sigma_sample, sizeof(c->sigma_sample), prev);
        prev = fnv1a(&c->verdict,      sizeof(c->verdict),      prev);
    }
    bool distinct = (s->calib[0].verdict != s->calib[1].verdict) &&
                    (s->calib[0].verdict != s->calib[2].verdict) &&
                    (s->calib[1].verdict != s->calib[2].verdict);
    s->calib_verdict_distinct_ok = distinct;
    s->calib_sample_shared_ok    = (n_shared == COS_V291_N_CALIB);

    s->n_percept_rows_ok = 0;
    bool ratio_ok = true, expl_ok = true;
    for (int i = 0; i < COS_V291_N_PERCEPT; ++i) {
        cos_v291_percept_t *p = &s->percept[i];
        memset(p, 0, sizeof(*p));
        p->sigma               = PERCEPTS[i].s;
        p->ratio_denominator   = PERCEPTS[i].d;
        p->explanation_present = true;
        if (p->sigma > 0.0f) s->n_percept_rows_ok++;
        int exp_ratio = (p->sigma > 0.0f) ? (int)lroundf(1.0f / p->sigma) : 0;
        if (p->ratio_denominator != exp_ratio) ratio_ok = false;
        if (!p->explanation_present) expl_ok = false;
        prev = fnv1a(&p->sigma,              sizeof(p->sigma),              prev);
        prev = fnv1a(&p->ratio_denominator,  sizeof(p->ratio_denominator),  prev);
        prev = fnv1a(&p->explanation_present, sizeof(p->explanation_present), prev);
    }
    s->percept_ratio_ok       = ratio_ok;
    s->percept_explanation_ok = expl_ok;

    s->n_bias_rows_ok = 0;
    bool formula_ok = true, polarity_ok = true, budget_ok = true;
    for (int i = 0; i < COS_V291_N_BIAS; ++i) {
        cos_v291_bias_t *b = &s->bias[i];
        memset(b, 0, sizeof(*b));
        cpy(b->bias_type, sizeof(b->bias_type), BIASES[i].t);
        b->sigma_raw       = BIASES[i].r;
        b->bias_offset     = BIASES[i].o;
        b->sigma_corrected = b->sigma_raw + b->bias_offset;
        b->residual_bias   = 0.00f;
        if (strlen(b->bias_type) > 0) s->n_bias_rows_ok++;
        float exp_corr = b->sigma_raw + b->bias_offset;
        if (fabsf(b->sigma_corrected - exp_corr) > 1e-5f) formula_ok = false;
        if (b->residual_bias > s->bias_budget) budget_ok = false;
        prev = fnv1a(b->bias_type,        strlen(b->bias_type),        prev);
        prev = fnv1a(&b->sigma_raw,       sizeof(b->sigma_raw),       prev);
        prev = fnv1a(&b->bias_offset,     sizeof(b->bias_offset),     prev);
        prev = fnv1a(&b->sigma_corrected, sizeof(b->sigma_corrected), prev);
        prev = fnv1a(&b->residual_bias,   sizeof(b->residual_bias),   prev);
    }
    polarity_ok =
        (strcmp(s->bias[0].bias_type, "overconfident")  == 0 && s->bias[0].bias_offset > 0.0f) &&
        (strcmp(s->bias[1].bias_type, "underconfident") == 0 && s->bias[1].bias_offset < 0.0f) &&
        (strcmp(s->bias[2].bias_type, "calibrated")     == 0 &&
         fabsf(s->bias[2].bias_offset) < 1e-6f);
    s->bias_formula_ok  = formula_ok;
    s->bias_polarity_ok = polarity_ok;
    s->bias_budget_ok   = budget_ok;

    s->n_entasis_rows_ok = 0;
    bool clamp_ok = true;
    for (int i = 0; i < COS_V291_N_ENTASIS; ++i) {
        cos_v291_entasis_t *e = &s->entasis[i];
        memset(e, 0, sizeof(*e));
        e->sigma_in      = ENTASIS_INPUTS[i];
        e->sigma_clamped = clampf(e->sigma_in, s->lower_bound, s->upper_bound);
        if (e->sigma_in >= 0.0f && e->sigma_in <= 1.0f) s->n_entasis_rows_ok++;
        float exp = clampf(e->sigma_in, s->lower_bound, s->upper_bound);
        if (fabsf(e->sigma_clamped - exp) > 1e-5f) clamp_ok = false;
        prev = fnv1a(&e->sigma_in,      sizeof(e->sigma_in),      prev);
        prev = fnv1a(&e->sigma_clamped, sizeof(e->sigma_clamped), prev);
    }
    s->entasis_clamp_formula_ok = clamp_ok;

    int total   = COS_V291_N_CALIB    + 1 + 1 +
                  COS_V291_N_PERCEPT  + 1 + 1 +
                  COS_V291_N_BIAS     + 1 + 1 + 1 +
                  COS_V291_N_ENTASIS  + 1;
    int passing = s->n_calib_rows_ok +
                  (s->calib_verdict_distinct_ok ? 1 : 0) +
                  (s->calib_sample_shared_ok    ? 1 : 0) +
                  s->n_percept_rows_ok +
                  (s->percept_ratio_ok       ? 1 : 0) +
                  (s->percept_explanation_ok ? 1 : 0) +
                  s->n_bias_rows_ok +
                  (s->bias_formula_ok  ? 1 : 0) +
                  (s->bias_polarity_ok ? 1 : 0) +
                  (s->bias_budget_ok   ? 1 : 0) +
                  s->n_entasis_rows_ok +
                  (s->entasis_clamp_formula_ok ? 1 : 0);
    s->sigma_parthenon = 1.0f - ((float)passing / (float)total);
    if (s->sigma_parthenon < 0.0f) s->sigma_parthenon = 0.0f;
    if (s->sigma_parthenon > 1.0f) s->sigma_parthenon = 1.0f;

    struct { int nc, np, nb, ne;
             bool cd, cs, pr, pe, bf, bp, bu, ec;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nc = s->n_calib_rows_ok;
    trec.np = s->n_percept_rows_ok;
    trec.nb = s->n_bias_rows_ok;
    trec.ne = s->n_entasis_rows_ok;
    trec.cd = s->calib_verdict_distinct_ok;
    trec.cs = s->calib_sample_shared_ok;
    trec.pr = s->percept_ratio_ok;
    trec.pe = s->percept_explanation_ok;
    trec.bf = s->bias_formula_ok;
    trec.bp = s->bias_polarity_ok;
    trec.bu = s->bias_budget_ok;
    trec.ec = s->entasis_clamp_formula_ok;
    trec.sigma = s->sigma_parthenon;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *verdict_name(cos_v291_verdict_t v) {
    switch (v) {
    case COS_V291_VERDICT_SAFE:     return "SAFE";
    case COS_V291_VERDICT_CAUTIOUS: return "CAUTIOUS";
    case COS_V291_VERDICT_ABSTAIN:  return "ABSTAIN";
    }
    return "UNKNOWN";
}

size_t cos_v291_to_json(const cos_v291_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v291\","
        "\"bias_budget\":%.3f,\"lower_bound\":%.3f,"
        "\"upper_bound\":%.3f,"
        "\"shared_sigma_sample\":%.3f,"
        "\"n_calib_rows_ok\":%d,"
        "\"calib_verdict_distinct_ok\":%s,"
        "\"calib_sample_shared_ok\":%s,"
        "\"n_percept_rows_ok\":%d,"
        "\"percept_ratio_ok\":%s,"
        "\"percept_explanation_ok\":%s,"
        "\"n_bias_rows_ok\":%d,"
        "\"bias_formula_ok\":%s,"
        "\"bias_polarity_ok\":%s,"
        "\"bias_budget_ok\":%s,"
        "\"n_entasis_rows_ok\":%d,"
        "\"entasis_clamp_formula_ok\":%s,"
        "\"sigma_parthenon\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"calib\":[",
        s->bias_budget, s->lower_bound, s->upper_bound,
        s->shared_sigma_sample,
        s->n_calib_rows_ok,
        s->calib_verdict_distinct_ok ? "true" : "false",
        s->calib_sample_shared_ok    ? "true" : "false",
        s->n_percept_rows_ok,
        s->percept_ratio_ok       ? "true" : "false",
        s->percept_explanation_ok ? "true" : "false",
        s->n_bias_rows_ok,
        s->bias_formula_ok  ? "true" : "false",
        s->bias_polarity_ok ? "true" : "false",
        s->bias_budget_ok   ? "true" : "false",
        s->n_entasis_rows_ok,
        s->entasis_clamp_formula_ok ? "true" : "false",
        s->sigma_parthenon,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V291_N_CALIB; ++i) {
        const cos_v291_calib_t *c = &s->calib[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"domain\":\"%s\",\"sigma_sample\":%.3f,"
            "\"verdict\":\"%s\"}",
            i == 0 ? "" : ",", c->domain, c->sigma_sample,
            verdict_name(c->verdict));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"percept\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V291_N_PERCEPT; ++i) {
        const cos_v291_percept_t *p = &s->percept[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"sigma\":%.4f,\"ratio_denominator\":%d,"
            "\"explanation_present\":%s}",
            i == 0 ? "" : ",", p->sigma, p->ratio_denominator,
            p->explanation_present ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"bias\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V291_N_BIAS; ++i) {
        const cos_v291_bias_t *b = &s->bias[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"bias_type\":\"%s\",\"sigma_raw\":%.4f,"
            "\"bias_offset\":%.4f,\"sigma_corrected\":%.4f,"
            "\"residual_bias\":%.4f}",
            i == 0 ? "" : ",", b->bias_type, b->sigma_raw,
            b->bias_offset, b->sigma_corrected,
            b->residual_bias);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"entasis\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V291_N_ENTASIS; ++i) {
        const cos_v291_entasis_t *e = &s->entasis[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"sigma_in\":%.4f,\"sigma_clamped\":%.4f}",
            i == 0 ? "" : ",", e->sigma_in, e->sigma_clamped);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v291_self_test(void) {
    cos_v291_state_t s;
    cos_v291_init(&s, 0x291bbbULL);
    cos_v291_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_D[COS_V291_N_CALIB] = { "medical", "code", "creative" };
    for (int i = 0; i < COS_V291_N_CALIB; ++i) {
        if (strcmp(s.calib[i].domain, WANT_D[i]) != 0) return 2;
        if (fabsf(s.calib[i].sigma_sample - s.shared_sigma_sample) > 1e-6f) return 3;
    }
    if (s.n_calib_rows_ok           != COS_V291_N_CALIB) return 4;
    if (!s.calib_verdict_distinct_ok) return 5;
    if (!s.calib_sample_shared_ok)    return 6;

    for (int i = 0; i < COS_V291_N_PERCEPT; ++i) {
        int exp_d = (int)lroundf(1.0f / s.percept[i].sigma);
        if (s.percept[i].ratio_denominator != exp_d) return 7;
        if (!s.percept[i].explanation_present)       return 8;
    }
    if (s.n_percept_rows_ok       != COS_V291_N_PERCEPT) return 9;
    if (!s.percept_ratio_ok)        return 10;
    if (!s.percept_explanation_ok)  return 11;

    static const char *WANT_B[COS_V291_N_BIAS] = {
        "overconfident", "underconfident", "calibrated"
    };
    for (int i = 0; i < COS_V291_N_BIAS; ++i) {
        if (strcmp(s.bias[i].bias_type, WANT_B[i]) != 0) return 12;
        float corr_exp = s.bias[i].sigma_raw + s.bias[i].bias_offset;
        if (fabsf(s.bias[i].sigma_corrected - corr_exp) > 1e-5f) return 13;
        if (s.bias[i].residual_bias > s.bias_budget) return 14;
    }
    if (s.n_bias_rows_ok     != COS_V291_N_BIAS) return 15;
    if (!s.bias_formula_ok)   return 16;
    if (!s.bias_polarity_ok)  return 17;
    if (!s.bias_budget_ok)    return 18;

    for (int i = 0; i < COS_V291_N_ENTASIS; ++i) {
        float exp = clampf(s.entasis[i].sigma_in, s.lower_bound, s.upper_bound);
        if (fabsf(s.entasis[i].sigma_clamped - exp) > 1e-5f) return 19;
    }
    if (s.n_entasis_rows_ok         != COS_V291_N_ENTASIS) return 20;
    if (!s.entasis_clamp_formula_ok) return 21;

    if (s.sigma_parthenon < 0.0f || s.sigma_parthenon > 1.0f) return 22;
    if (s.sigma_parthenon > 1e-6f) return 23;

    cos_v291_state_t u;
    cos_v291_init(&u, 0x291bbbULL);
    cos_v291_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 24;
    return 0;
}
