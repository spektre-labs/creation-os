/*
 * v286 σ-Interpretability — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "interpretability.h"

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

static const struct {
    const char *id; const char *ch; const char *cs;
} DECOMPS[COS_V286_N_DECOMP] = {
    { "low_confidence", "entropy",
      "model does not know the answer"             },
    { "repetitive",     "repetition",
      "model is repeating itself"                  },
    { "overconfident",  "calibration",
      "model is overconfident"                     },
    { "distracted",     "attention",
      "model could not focus attention"            },
};

static const struct { const char *n; float s; }
    HEADS[COS_V286_N_ATTN] = {
    { "head_0", 0.12f },   /* CONFIDENT */
    { "head_1", 0.35f },   /* CONFIDENT */
    { "head_2", 0.78f },   /* UNCERTAIN */
};

static const struct { int id; float w; float wo; }
    CFS[COS_V286_N_CF] = {
    { 0, 0.20f, 0.50f },   /* delta=0.30 → CRITICAL   */
    { 1, 0.20f, 0.25f },   /* delta=0.05 → IRRELEVANT */
    { 2, 0.20f, 0.42f },   /* delta=0.22 → CRITICAL   */
};

static const struct { const char *id; float trust; }
    REPORTS[COS_V286_N_REPORT] = {
    { "response_0", 92.0f },
    { "response_1", 67.0f },
    { "response_2", 41.0f },
};

void cos_v286_init(cos_v286_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed           = seed ? seed : 0x286666ULL;
    s->tau_attn       = 0.40f;
    s->delta_critical = 0.10f;
}

void cos_v286_run(cos_v286_state_t *s) {
    uint64_t prev = 0x28666600ULL;

    s->n_decomp_rows_ok = 0;
    for (int i = 0; i < COS_V286_N_DECOMP; ++i) {
        cos_v286_decomp_t *d = &s->decomp[i];
        memset(d, 0, sizeof(*d));
        cpy(d->scenario_id, sizeof(d->scenario_id), DECOMPS[i].id);
        cpy(d->top_channel, sizeof(d->top_channel), DECOMPS[i].ch);
        cpy(d->cause,       sizeof(d->cause),       DECOMPS[i].cs);
        d->cause_nonempty = (strlen(d->cause) > 0);
        if (strlen(d->scenario_id) > 0 &&
            strlen(d->top_channel) > 0 &&
            d->cause_nonempty)
            s->n_decomp_rows_ok++;
        prev = fnv1a(d->scenario_id,   strlen(d->scenario_id),   prev);
        prev = fnv1a(d->top_channel,   strlen(d->top_channel),   prev);
        prev = fnv1a(d->cause,         strlen(d->cause),         prev);
        prev = fnv1a(&d->cause_nonempty, sizeof(d->cause_nonempty), prev);
    }
    bool ch_distinct = true;
    for (int i = 0; i < COS_V286_N_DECOMP && ch_distinct; ++i) {
        for (int j = i + 1; j < COS_V286_N_DECOMP; ++j) {
            if (strcmp(s->decomp[i].top_channel,
                       s->decomp[j].top_channel) == 0) {
                ch_distinct = false; break;
            }
        }
    }
    s->decomp_channels_distinct_ok = ch_distinct;
    bool all_cause = true;
    for (int i = 0; i < COS_V286_N_DECOMP; ++i)
        if (!s->decomp[i].cause_nonempty) all_cause = false;
    s->decomp_all_causes_ok = all_cause;

    s->n_attn_rows_ok = 0;
    s->n_attn_confident = s->n_attn_uncertain = 0;
    for (int i = 0; i < COS_V286_N_ATTN; ++i) {
        cos_v286_head_t *h = &s->head[i];
        memset(h, 0, sizeof(*h));
        cpy(h->name, sizeof(h->name), HEADS[i].n);
        h->sigma_head = HEADS[i].s;
        h->status     = (h->sigma_head <= s->tau_attn)
                            ? COS_V286_ATTN_CONFIDENT
                            : COS_V286_ATTN_UNCERTAIN;
        if (h->sigma_head >= 0.0f && h->sigma_head <= 1.0f)
            s->n_attn_rows_ok++;
        if (h->status == COS_V286_ATTN_CONFIDENT) s->n_attn_confident++;
        if (h->status == COS_V286_ATTN_UNCERTAIN) s->n_attn_uncertain++;
        prev = fnv1a(h->name, strlen(h->name), prev);
        prev = fnv1a(&h->sigma_head, sizeof(h->sigma_head), prev);
        prev = fnv1a(&h->status,     sizeof(h->status),     prev);
    }

    s->n_cf_rows_ok = 0;
    s->n_cf_critical = s->n_cf_irrelevant = 0;
    bool delta_ok = true;
    for (int i = 0; i < COS_V286_N_CF; ++i) {
        cos_v286_cf_row_t *c = &s->cf[i];
        memset(c, 0, sizeof(*c));
        c->token_id      = CFS[i].id;
        c->sigma_with    = CFS[i].w;
        c->sigma_without = CFS[i].wo;
        float delta = c->sigma_without - c->sigma_with;
        if (delta < 0.0f) delta = -delta;
        c->delta_sigma   = delta;
        c->classification = (c->delta_sigma > s->delta_critical)
                                ? COS_V286_CF_CRITICAL
                                : COS_V286_CF_IRRELEVANT;
        if (c->sigma_with    >= 0.0f && c->sigma_with    <= 1.0f &&
            c->sigma_without >= 0.0f && c->sigma_without <= 1.0f)
            s->n_cf_rows_ok++;
        if (c->classification == COS_V286_CF_CRITICAL)   s->n_cf_critical++;
        if (c->classification == COS_V286_CF_IRRELEVANT) s->n_cf_irrelevant++;
        float want = c->sigma_without - c->sigma_with;
        if (want < 0.0f) want = -want;
        float err = c->delta_sigma - want;
        if (err < 0.0f) err = -err;
        if (err > 1e-5f) delta_ok = false;
        prev = fnv1a(&c->token_id,      sizeof(c->token_id),      prev);
        prev = fnv1a(&c->sigma_with,    sizeof(c->sigma_with),    prev);
        prev = fnv1a(&c->sigma_without, sizeof(c->sigma_without), prev);
        prev = fnv1a(&c->delta_sigma,   sizeof(c->delta_sigma),   prev);
        prev = fnv1a(&c->classification, sizeof(c->classification), prev);
    }
    s->cf_delta_formula_ok = delta_ok;

    s->n_report_rows_ok = 0;
    bool range_ok = true, compliant_ok = true;
    for (int i = 0; i < COS_V286_N_REPORT; ++i) {
        cos_v286_report_t *r = &s->report[i];
        memset(r, 0, sizeof(*r));
        cpy(r->response_id, sizeof(r->response_id), REPORTS[i].id);
        r->trust_percent           = REPORTS[i].trust;
        r->explanation_present     = true;
        r->recommendation_present  = true;
        r->eu_article_13_compliant = true;
        if (r->trust_percent >= 0.0f && r->trust_percent <= 100.0f)
            s->n_report_rows_ok++;
        if (r->trust_percent < 0.0f || r->trust_percent > 100.0f)
            range_ok = false;
        if (!r->explanation_present || !r->recommendation_present ||
            !r->eu_article_13_compliant)
            compliant_ok = false;
        prev = fnv1a(r->response_id, strlen(r->response_id), prev);
        prev = fnv1a(&r->trust_percent,           sizeof(r->trust_percent),           prev);
        prev = fnv1a(&r->explanation_present,     sizeof(r->explanation_present),     prev);
        prev = fnv1a(&r->recommendation_present,  sizeof(r->recommendation_present),  prev);
        prev = fnv1a(&r->eu_article_13_compliant, sizeof(r->eu_article_13_compliant), prev);
    }
    s->report_trust_range_ok   = range_ok;
    s->report_all_compliant_ok = compliant_ok;

    bool attn_both = (s->n_attn_confident >= 1) && (s->n_attn_uncertain >= 1);
    bool cf_both   = (s->n_cf_critical    >= 1) && (s->n_cf_irrelevant  >= 1);

    int total   = COS_V286_N_DECOMP + 1 + 1 +
                  COS_V286_N_ATTN   + 1 +
                  COS_V286_N_CF     + 1 + 1 +
                  COS_V286_N_REPORT + 1 + 1;
    int passing = s->n_decomp_rows_ok +
                  (s->decomp_channels_distinct_ok ? 1 : 0) +
                  (s->decomp_all_causes_ok        ? 1 : 0) +
                  s->n_attn_rows_ok +
                  (attn_both ? 1 : 0) +
                  s->n_cf_rows_ok +
                  (cf_both ? 1 : 0) +
                  (s->cf_delta_formula_ok ? 1 : 0) +
                  s->n_report_rows_ok +
                  (s->report_all_compliant_ok ? 1 : 0) +
                  (s->report_trust_range_ok   ? 1 : 0);
    s->sigma_interpret = 1.0f - ((float)passing / (float)total);
    if (s->sigma_interpret < 0.0f) s->sigma_interpret = 0.0f;
    if (s->sigma_interpret > 1.0f) s->sigma_interpret = 1.0f;

    struct { int nd, na, nac, nau, nc, ncc, nci, nr;
             bool cdist, cau, cfd, rcp, rrn;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nd  = s->n_decomp_rows_ok;
    trec.na  = s->n_attn_rows_ok;
    trec.nac = s->n_attn_confident;
    trec.nau = s->n_attn_uncertain;
    trec.nc  = s->n_cf_rows_ok;
    trec.ncc = s->n_cf_critical;
    trec.nci = s->n_cf_irrelevant;
    trec.nr  = s->n_report_rows_ok;
    trec.cdist = s->decomp_channels_distinct_ok;
    trec.cau   = s->decomp_all_causes_ok;
    trec.cfd   = s->cf_delta_formula_ok;
    trec.rcp   = s->report_all_compliant_ok;
    trec.rrn   = s->report_trust_range_ok;
    trec.sigma = s->sigma_interpret;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *attn_name(cos_v286_attn_t a) {
    switch (a) {
    case COS_V286_ATTN_CONFIDENT: return "CONFIDENT";
    case COS_V286_ATTN_UNCERTAIN: return "UNCERTAIN";
    }
    return "UNKNOWN";
}

static const char *cf_name(cos_v286_cf_t c) {
    switch (c) {
    case COS_V286_CF_CRITICAL:   return "CRITICAL";
    case COS_V286_CF_IRRELEVANT: return "IRRELEVANT";
    }
    return "UNKNOWN";
}

size_t cos_v286_to_json(const cos_v286_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v286\","
        "\"tau_attn\":%.3f,\"delta_critical\":%.3f,"
        "\"n_decomp_rows_ok\":%d,"
        "\"decomp_channels_distinct_ok\":%s,"
        "\"decomp_all_causes_ok\":%s,"
        "\"n_attn_rows_ok\":%d,"
        "\"n_attn_confident\":%d,\"n_attn_uncertain\":%d,"
        "\"n_cf_rows_ok\":%d,"
        "\"n_cf_critical\":%d,\"n_cf_irrelevant\":%d,"
        "\"cf_delta_formula_ok\":%s,"
        "\"n_report_rows_ok\":%d,"
        "\"report_all_compliant_ok\":%s,"
        "\"report_trust_range_ok\":%s,"
        "\"sigma_interpret\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"decomp\":[",
        s->tau_attn, s->delta_critical,
        s->n_decomp_rows_ok,
        s->decomp_channels_distinct_ok ? "true" : "false",
        s->decomp_all_causes_ok        ? "true" : "false",
        s->n_attn_rows_ok,
        s->n_attn_confident, s->n_attn_uncertain,
        s->n_cf_rows_ok,
        s->n_cf_critical, s->n_cf_irrelevant,
        s->cf_delta_formula_ok ? "true" : "false",
        s->n_report_rows_ok,
        s->report_all_compliant_ok ? "true" : "false",
        s->report_trust_range_ok   ? "true" : "false",
        s->sigma_interpret,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V286_N_DECOMP; ++i) {
        const cos_v286_decomp_t *d = &s->decomp[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"scenario_id\":\"%s\",\"top_channel\":\"%s\","
            "\"cause\":\"%s\",\"cause_nonempty\":%s}",
            i == 0 ? "" : ",", d->scenario_id, d->top_channel,
            d->cause, d->cause_nonempty ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"head\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V286_N_ATTN; ++i) {
        const cos_v286_head_t *h = &s->head[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"sigma_head\":%.4f,\"status\":\"%s\"}",
            i == 0 ? "" : ",", h->name, h->sigma_head, attn_name(h->status));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"cf\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V286_N_CF; ++i) {
        const cos_v286_cf_row_t *c = &s->cf[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"token_id\":%d,\"sigma_with\":%.4f,"
            "\"sigma_without\":%.4f,\"delta_sigma\":%.4f,"
            "\"classification\":\"%s\"}",
            i == 0 ? "" : ",", c->token_id,
            c->sigma_with, c->sigma_without, c->delta_sigma,
            cf_name(c->classification));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"report\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V286_N_REPORT; ++i) {
        const cos_v286_report_t *r = &s->report[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"response_id\":\"%s\",\"trust_percent\":%.2f,"
            "\"explanation_present\":%s,\"recommendation_present\":%s,"
            "\"eu_article_13_compliant\":%s}",
            i == 0 ? "" : ",", r->response_id, r->trust_percent,
            r->explanation_present     ? "true" : "false",
            r->recommendation_present  ? "true" : "false",
            r->eu_article_13_compliant ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v286_self_test(void) {
    cos_v286_state_t s;
    cos_v286_init(&s, 0x286666ULL);
    cos_v286_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_DEC[COS_V286_N_DECOMP] = {
        "low_confidence", "repetitive", "overconfident", "distracted"
    };
    static const char *WANT_CH[COS_V286_N_DECOMP] = {
        "entropy", "repetition", "calibration", "attention"
    };
    for (int i = 0; i < COS_V286_N_DECOMP; ++i) {
        if (strcmp(s.decomp[i].scenario_id, WANT_DEC[i]) != 0) return 2;
        if (strcmp(s.decomp[i].top_channel, WANT_CH[i])  != 0) return 3;
        if (!s.decomp[i].cause_nonempty) return 4;
    }
    if (s.n_decomp_rows_ok != COS_V286_N_DECOMP) return 5;
    if (!s.decomp_channels_distinct_ok) return 6;
    if (!s.decomp_all_causes_ok)        return 7;

    for (int i = 0; i < COS_V286_N_ATTN; ++i) {
        cos_v286_attn_t exp = (s.head[i].sigma_head <= s.tau_attn)
                                  ? COS_V286_ATTN_CONFIDENT
                                  : COS_V286_ATTN_UNCERTAIN;
        if (s.head[i].status != exp) return 8;
    }
    if (s.n_attn_rows_ok    != COS_V286_N_ATTN) return 9;
    if (s.n_attn_confident  < 1) return 10;
    if (s.n_attn_uncertain  < 1) return 11;

    for (int i = 0; i < COS_V286_N_CF; ++i) {
        float want = s.cf[i].sigma_without - s.cf[i].sigma_with;
        if (want < 0.0f) want = -want;
        float err = s.cf[i].delta_sigma - want;
        if (err < 0.0f) err = -err;
        if (err > 1e-5f) return 12;
        cos_v286_cf_t exp = (s.cf[i].delta_sigma > s.delta_critical)
                                ? COS_V286_CF_CRITICAL
                                : COS_V286_CF_IRRELEVANT;
        if (s.cf[i].classification != exp) return 13;
    }
    if (s.n_cf_rows_ok      != COS_V286_N_CF) return 14;
    if (s.n_cf_critical     < 1) return 15;
    if (s.n_cf_irrelevant   < 1) return 16;
    if (!s.cf_delta_formula_ok) return 17;

    for (int i = 0; i < COS_V286_N_REPORT; ++i) {
        if (s.report[i].trust_percent < 0.0f ||
            s.report[i].trust_percent > 100.0f) return 18;
        if (!s.report[i].explanation_present)     return 19;
        if (!s.report[i].recommendation_present)  return 20;
        if (!s.report[i].eu_article_13_compliant) return 21;
    }
    if (s.n_report_rows_ok        != COS_V286_N_REPORT) return 22;
    if (!s.report_all_compliant_ok) return 23;
    if (!s.report_trust_range_ok)   return 24;

    if (s.sigma_interpret < 0.0f || s.sigma_interpret > 1.0f) return 25;
    if (s.sigma_interpret > 1e-6f) return 26;

    cos_v286_state_t u;
    cos_v286_init(&u, 0x286666ULL);
    cos_v286_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 27;
    return 0;
}
