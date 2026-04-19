/*
 * v278 σ-Recursive-Self-Improve — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "rsi.h"

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

static const struct { int e; float err; }
    CALIB[COS_V278_N_EPOCHS] = {
    { 0, 0.22f },
    { 1, 0.15f },
    { 2, 0.09f },
    { 3, 0.04f },   /* anchor ≤ 0.05 */
};

static const struct { int ch; float aurcc; }
    ARCH[COS_V278_N_ARCH] = {
    {  6, 0.78f },
    {  8, 0.91f },   /* winner */
    { 12, 0.84f },
};

static const struct { const char *dn; float tau; }
    THRESH[COS_V278_N_DOMAINS] = {
    { "code",     0.20f },
    { "creative", 0.50f },
    { "medical",  0.15f },
};

static const struct { int id; float s; }
    GOEDEL[COS_V278_N_GOEDEL] = {
    { 0, 0.12f },   /* SELF_CONFIDENT      */
    { 1, 0.37f },   /* SELF_CONFIDENT      */
    { 2, 0.64f },   /* CALL_PROCONDUCTOR   */
};

void cos_v278_init(cos_v278_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x278515ULL;
    s->tau_goedel = 0.40f;
}

void cos_v278_run(cos_v278_state_t *s) {
    uint64_t prev = 0x27851500ULL;

    s->n_calibration_rows_ok  = 0;
    s->calibration_monotone_ok = true;
    for (int i = 0; i < COS_V278_N_EPOCHS; ++i) {
        cos_v278_calib_t *c = &s->calibration[i];
        memset(c, 0, sizeof(*c));
        c->epoch = CALIB[i].e;
        c->sigma_calibration_err = CALIB[i].err;
        if (c->sigma_calibration_err >= 0.0f &&
            c->sigma_calibration_err <= 1.0f &&
            c->epoch == i)
            s->n_calibration_rows_ok++;
        if (i > 0 &&
            !(c->sigma_calibration_err <
              s->calibration[i-1].sigma_calibration_err))
            s->calibration_monotone_ok = false;
        prev = fnv1a(&c->epoch,                 sizeof(c->epoch),                 prev);
        prev = fnv1a(&c->sigma_calibration_err, sizeof(c->sigma_calibration_err), prev);
    }
    s->calibration_anchor_ok =
        (s->calibration[COS_V278_N_EPOCHS - 1].sigma_calibration_err <= 0.05f);

    s->n_arch_rows_ok = 0;
    int best = 0;
    float best_aurcc = -1.0f;
    for (int i = 0; i < COS_V278_N_ARCH; ++i) {
        cos_v278_arch_t *a = &s->arch[i];
        memset(a, 0, sizeof(*a));
        a->n_channels = ARCH[i].ch;
        a->aurcc      = ARCH[i].aurcc;
        a->chosen     = false;
        if (a->n_channels > 0 && a->aurcc >= 0.0f && a->aurcc <= 1.0f)
            s->n_arch_rows_ok++;
        if (a->aurcc > best_aurcc) { best_aurcc = a->aurcc; best = i; }
    }
    s->arch[best].chosen = true;
    s->arch_chosen_idx   = best;
    for (int i = 0; i < COS_V278_N_ARCH; ++i) {
        cos_v278_arch_t *a = &s->arch[i];
        prev = fnv1a(&a->n_channels, sizeof(a->n_channels), prev);
        prev = fnv1a(&a->aurcc,      sizeof(a->aurcc),      prev);
        prev = fnv1a(&a->chosen,     sizeof(a->chosen),     prev);
    }
    int chosen_count = 0;
    for (int i = 0; i < COS_V278_N_ARCH; ++i)
        if (s->arch[i].chosen) chosen_count++;
    s->arch_chosen_ok = (chosen_count == 1);
    int distinct = 0;
    for (int i = 0; i < COS_V278_N_ARCH; ++i) {
        bool new_v = true;
        for (int j = 0; j < i; ++j)
            if (fabsf(s->arch[i].aurcc - s->arch[j].aurcc) < 1e-6f) {
                new_v = false; break;
            }
        if (new_v) distinct++;
    }
    s->n_distinct_aurcc   = distinct;
    s->arch_distinct_ok   = (distinct >= 2);

    s->n_threshold_rows_ok = 0;
    for (int i = 0; i < COS_V278_N_DOMAINS; ++i) {
        cos_v278_thresh_t *t = &s->thresholds[i];
        memset(t, 0, sizeof(*t));
        cpy(t->domain, sizeof(t->domain), THRESH[i].dn);
        t->tau = THRESH[i].tau;
        if (t->tau > 0.0f && t->tau < 1.0f)
            s->n_threshold_rows_ok++;
        prev = fnv1a(t->domain, strlen(t->domain), prev);
        prev = fnv1a(&t->tau,   sizeof(t->tau),    prev);
    }
    s->threshold_canonical_ok =
        (strcmp(s->thresholds[0].domain, "code")     == 0) &&
        (strcmp(s->thresholds[1].domain, "creative") == 0) &&
        (strcmp(s->thresholds[2].domain, "medical")  == 0) &&
        (fabsf(s->thresholds[0].tau - 0.20f) < 1e-6f) &&
        (fabsf(s->thresholds[1].tau - 0.50f) < 1e-6f) &&
        (fabsf(s->thresholds[2].tau - 0.15f) < 1e-6f);
    int distinct_tau = 0;
    for (int i = 0; i < COS_V278_N_DOMAINS; ++i) {
        bool new_v = true;
        for (int j = 0; j < i; ++j)
            if (fabsf(s->thresholds[i].tau - s->thresholds[j].tau) < 1e-6f) {
                new_v = false; break;
            }
        if (new_v) distinct_tau++;
    }
    s->n_distinct_tau       = distinct_tau;
    s->threshold_distinct_ok = (distinct_tau >= 2);

    s->n_goedel_rows_ok = 0;
    s->n_goedel_self = s->n_goedel_procon = 0;
    for (int i = 0; i < COS_V278_N_GOEDEL; ++i) {
        cos_v278_goedel_t *g = &s->goedel[i];
        memset(g, 0, sizeof(*g));
        g->claim_id     = GOEDEL[i].id;
        g->sigma_goedel = GOEDEL[i].s;
        g->action = (g->sigma_goedel <= s->tau_goedel)
                        ? COS_V278_ACT_SELF_CONFIDENT
                        : COS_V278_ACT_CALL_PROCONDUCTOR;
        if (g->sigma_goedel >= 0.0f && g->sigma_goedel <= 1.0f)
            s->n_goedel_rows_ok++;
        if (g->action == COS_V278_ACT_SELF_CONFIDENT)    s->n_goedel_self++;
        if (g->action == COS_V278_ACT_CALL_PROCONDUCTOR) s->n_goedel_procon++;
        prev = fnv1a(&g->claim_id,     sizeof(g->claim_id),     prev);
        prev = fnv1a(&g->sigma_goedel, sizeof(g->sigma_goedel), prev);
        prev = fnv1a(&g->action,       sizeof(g->action),       prev);
    }
    bool goedel_both = (s->n_goedel_self >= 1) && (s->n_goedel_procon >= 1);

    int total   = COS_V278_N_EPOCHS  + 1 + 1 +
                  COS_V278_N_ARCH    + 1 + 1 +
                  COS_V278_N_DOMAINS + 1 + 1 +
                  COS_V278_N_GOEDEL  + 1;
    int passing = s->n_calibration_rows_ok +
                  (s->calibration_monotone_ok ? 1 : 0) +
                  (s->calibration_anchor_ok   ? 1 : 0) +
                  s->n_arch_rows_ok +
                  (s->arch_chosen_ok   ? 1 : 0) +
                  (s->arch_distinct_ok ? 1 : 0) +
                  s->n_threshold_rows_ok +
                  (s->threshold_canonical_ok ? 1 : 0) +
                  (s->threshold_distinct_ok  ? 1 : 0) +
                  s->n_goedel_rows_ok +
                  (goedel_both ? 1 : 0);
    s->sigma_rsi = 1.0f - ((float)passing / (float)total);
    if (s->sigma_rsi < 0.0f) s->sigma_rsi = 0.0f;
    if (s->sigma_rsi > 1.0f) s->sigma_rsi = 1.0f;

    struct { int nc, na, nti, nt, nax, ngs, ngp, ndt;
             int aci;
             bool cm, ca, axc, ad, tc, td;
             float sigma, tg; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nc  = s->n_calibration_rows_ok;
    trec.na  = s->n_arch_rows_ok;
    trec.nti = s->n_threshold_rows_ok;
    trec.nt  = s->n_goedel_rows_ok;
    trec.nax = s->n_distinct_aurcc;
    trec.ngs = s->n_goedel_self;
    trec.ngp = s->n_goedel_procon;
    trec.ndt = s->n_distinct_tau;
    trec.aci = s->arch_chosen_idx;
    trec.cm  = s->calibration_monotone_ok;
    trec.ca  = s->calibration_anchor_ok;
    trec.axc = s->arch_chosen_ok;
    trec.ad  = s->arch_distinct_ok;
    trec.tc  = s->threshold_canonical_ok;
    trec.td  = s->threshold_distinct_ok;
    trec.sigma = s->sigma_rsi;
    trec.tg  = s->tau_goedel;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *act_name(cos_v278_act_t a) {
    switch (a) {
    case COS_V278_ACT_SELF_CONFIDENT:    return "SELF_CONFIDENT";
    case COS_V278_ACT_CALL_PROCONDUCTOR: return "CALL_PROCONDUCTOR";
    }
    return "UNKNOWN";
}

size_t cos_v278_to_json(const cos_v278_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v278\",\"tau_goedel\":%.3f,"
        "\"n_calibration_rows_ok\":%d,"
        "\"calibration_monotone_ok\":%s,"
        "\"calibration_anchor_ok\":%s,"
        "\"n_arch_rows_ok\":%d,\"arch_chosen_idx\":%d,"
        "\"arch_chosen_ok\":%s,\"n_distinct_aurcc\":%d,"
        "\"arch_distinct_ok\":%s,"
        "\"n_threshold_rows_ok\":%d,"
        "\"threshold_canonical_ok\":%s,"
        "\"n_distinct_tau\":%d,\"threshold_distinct_ok\":%s,"
        "\"n_goedel_rows_ok\":%d,"
        "\"n_goedel_self\":%d,\"n_goedel_procon\":%d,"
        "\"sigma_rsi\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"calibration\":[",
        s->tau_goedel,
        s->n_calibration_rows_ok,
        s->calibration_monotone_ok ? "true" : "false",
        s->calibration_anchor_ok   ? "true" : "false",
        s->n_arch_rows_ok, s->arch_chosen_idx,
        s->arch_chosen_ok ? "true" : "false",
        s->n_distinct_aurcc,
        s->arch_distinct_ok ? "true" : "false",
        s->n_threshold_rows_ok,
        s->threshold_canonical_ok ? "true" : "false",
        s->n_distinct_tau,
        s->threshold_distinct_ok ? "true" : "false",
        s->n_goedel_rows_ok,
        s->n_goedel_self, s->n_goedel_procon,
        s->sigma_rsi,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V278_N_EPOCHS; ++i) {
        const cos_v278_calib_t *c = &s->calibration[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"epoch\":%d,\"sigma_calibration_err\":%.4f}",
            i == 0 ? "" : ",", c->epoch, c->sigma_calibration_err);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"arch\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V278_N_ARCH; ++i) {
        const cos_v278_arch_t *a = &s->arch[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"n_channels\":%d,\"aurcc\":%.4f,\"chosen\":%s}",
            i == 0 ? "" : ",", a->n_channels, a->aurcc,
            a->chosen ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"thresholds\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V278_N_DOMAINS; ++i) {
        const cos_v278_thresh_t *t = &s->thresholds[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"domain\":\"%s\",\"tau\":%.4f}",
            i == 0 ? "" : ",", t->domain, t->tau);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"goedel\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V278_N_GOEDEL; ++i) {
        const cos_v278_goedel_t *g = &s->goedel[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"claim_id\":%d,\"sigma_goedel\":%.4f,\"action\":\"%s\"}",
            i == 0 ? "" : ",", g->claim_id, g->sigma_goedel,
            act_name(g->action));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v278_self_test(void) {
    cos_v278_state_t s;
    cos_v278_init(&s, 0x278515ULL);
    cos_v278_run(&s);
    if (!s.chain_valid) return 1;

    if (s.n_calibration_rows_ok != COS_V278_N_EPOCHS) return 2;
    if (!s.calibration_monotone_ok) return 3;
    if (!s.calibration_anchor_ok)   return 4;
    for (int i = 1; i < COS_V278_N_EPOCHS; ++i)
        if (!(s.calibration[i].sigma_calibration_err <
              s.calibration[i-1].sigma_calibration_err)) return 5;

    if (s.n_arch_rows_ok != COS_V278_N_ARCH) return 6;
    if (!s.arch_chosen_ok) return 7;
    int exp_best = 0; float best = -1.0f;
    for (int i = 0; i < COS_V278_N_ARCH; ++i)
        if (s.arch[i].aurcc > best) { best = s.arch[i].aurcc; exp_best = i; }
    if (s.arch_chosen_idx != exp_best) return 8;
    if (!s.arch[exp_best].chosen) return 9;
    if (!s.arch_distinct_ok) return 10;

    if (s.n_threshold_rows_ok != COS_V278_N_DOMAINS) return 11;
    if (!s.threshold_canonical_ok) return 12;
    if (!s.threshold_distinct_ok)  return 13;

    for (int i = 0; i < COS_V278_N_GOEDEL; ++i) {
        cos_v278_act_t exp =
            (s.goedel[i].sigma_goedel <= s.tau_goedel)
                ? COS_V278_ACT_SELF_CONFIDENT
                : COS_V278_ACT_CALL_PROCONDUCTOR;
        if (s.goedel[i].action != exp) return 14;
    }
    if (s.n_goedel_rows_ok != COS_V278_N_GOEDEL) return 15;
    if (s.n_goedel_self   < 1) return 16;
    if (s.n_goedel_procon < 1) return 17;

    if (s.sigma_rsi < 0.0f || s.sigma_rsi > 1.0f) return 18;
    if (s.sigma_rsi > 1e-6f) return 19;

    cos_v278_state_t u;
    cos_v278_init(&u, 0x278515ULL);
    cos_v278_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 20;
    return 0;
}
