/*
 * v288 σ-Oculus — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "oculus.h"

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

static const struct { const char *d; float t; cos_v288_width_t w; }
    CASCADES[COS_V288_N_CASCADE] = {
    { "medical",  0.10f, COS_V288_WIDTH_TIGHT  },
    { "code",     0.30f, COS_V288_WIDTH_NORMAL },
    { "creative", 0.60f, COS_V288_WIDTH_OPEN   },
};

static const struct { const char *l; float t; bool u; bool d; }
    EXTREMES[COS_V288_N_EXTREME] = {
    { "closed",  0.00f, true,  false },
    { "open",    1.00f, false, true  },
    { "optimal", 0.30f, false, false },
};

static const struct { int s; float t; float e; }
    ADAPTIVES[COS_V288_N_ADAPTIVE] = {
    { 0, 0.30f, 0.15f },
    { 1, 0.20f, 0.08f },
    { 2, 0.15f, 0.03f },
};

static const char *TRANSPARENTS[COS_V288_N_TRANSPARENT] = {
    "tau_declared", "sigma_measured", "decision_visible",
};

void cos_v288_init(cos_v288_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed            = seed ? seed : 0x288888ULL;
    s->threshold_error = 0.05f;
}

void cos_v288_run(cos_v288_state_t *s) {
    uint64_t prev = 0x28888800ULL;

    s->n_cascade_rows_ok = 0;
    for (int i = 0; i < COS_V288_N_CASCADE; ++i) {
        cos_v288_cascade_t *c = &s->cascade[i];
        memset(c, 0, sizeof(*c));
        cpy(c->domain, sizeof(c->domain), CASCADES[i].d);
        c->tau   = CASCADES[i].t;
        c->width = CASCADES[i].w;
        if (strlen(c->domain) > 0 && c->tau >= 0.0f && c->tau <= 1.0f)
            s->n_cascade_rows_ok++;
        prev = fnv1a(c->domain, strlen(c->domain), prev);
        prev = fnv1a(&c->tau,   sizeof(c->tau),   prev);
        prev = fnv1a(&c->width, sizeof(c->width), prev);
    }
    s->cascade_width_distinct_ok =
        (s->cascade[0].width != s->cascade[1].width) &&
        (s->cascade[0].width != s->cascade[2].width) &&
        (s->cascade[1].width != s->cascade[2].width);
    s->cascade_tau_monotone_ok =
        (s->cascade[0].tau < s->cascade[1].tau) &&
        (s->cascade[1].tau < s->cascade[2].tau);

    s->n_extreme_rows_ok = 0;
    for (int i = 0; i < COS_V288_N_EXTREME; ++i) {
        cos_v288_extreme_t *e = &s->extreme[i];
        memset(e, 0, sizeof(*e));
        cpy(e->label, sizeof(e->label), EXTREMES[i].l);
        e->tau       = EXTREMES[i].t;
        e->useless   = EXTREMES[i].u;
        e->dangerous = EXTREMES[i].d;
        if (strlen(e->label) > 0) s->n_extreme_rows_ok++;
        prev = fnv1a(e->label, strlen(e->label), prev);
        prev = fnv1a(&e->tau,       sizeof(e->tau),       prev);
        prev = fnv1a(&e->useless,   sizeof(e->useless),   prev);
        prev = fnv1a(&e->dangerous, sizeof(e->dangerous), prev);
    }
    s->extreme_polarity_ok =
        (s->extreme[0].useless   == true  && s->extreme[0].dangerous == false) &&
        (s->extreme[1].useless   == false && s->extreme[1].dangerous == true ) &&
        (s->extreme[2].useless   == false && s->extreme[2].dangerous == false);

    s->n_adaptive_rows_ok = 0;
    s->n_action_tighten = s->n_action_stable = 0;
    for (int i = 0; i < COS_V288_N_ADAPTIVE; ++i) {
        cos_v288_adaptive_t *a = &s->adaptive[i];
        memset(a, 0, sizeof(*a));
        a->step       = ADAPTIVES[i].s;
        a->tau        = ADAPTIVES[i].t;
        a->error_rate = ADAPTIVES[i].e;
        a->action     = (a->error_rate > s->threshold_error)
                            ? COS_V288_ACTION_TIGHTEN
                            : COS_V288_ACTION_STABLE;
        if (a->error_rate >= 0.0f && a->error_rate <= 1.0f)
            s->n_adaptive_rows_ok++;
        if (a->action == COS_V288_ACTION_TIGHTEN) s->n_action_tighten++;
        if (a->action == COS_V288_ACTION_STABLE)  s->n_action_stable++;
        prev = fnv1a(&a->step,       sizeof(a->step),       prev);
        prev = fnv1a(&a->tau,        sizeof(a->tau),        prev);
        prev = fnv1a(&a->error_rate, sizeof(a->error_rate), prev);
        prev = fnv1a(&a->action,     sizeof(a->action),     prev);
    }
    bool err_dec = true;
    for (int i = 1; i < COS_V288_N_ADAPTIVE; ++i) {
        if (s->adaptive[i].error_rate >= s->adaptive[i - 1].error_rate)
            err_dec = false;
    }
    s->adaptive_error_monotone_ok = err_dec;
    bool action_ok = true;
    for (int i = 0; i < COS_V288_N_ADAPTIVE; ++i) {
        cos_v288_action_t exp = (s->adaptive[i].error_rate > s->threshold_error)
                                    ? COS_V288_ACTION_TIGHTEN
                                    : COS_V288_ACTION_STABLE;
        if (s->adaptive[i].action != exp) action_ok = false;
        if (exp == COS_V288_ACTION_TIGHTEN && i + 1 < COS_V288_N_ADAPTIVE) {
            if (!(s->adaptive[i + 1].tau < s->adaptive[i].tau)) action_ok = false;
        }
    }
    s->adaptive_action_rule_ok = action_ok;

    s->n_transparent_rows_ok = 0;
    bool all_rep = true;
    for (int i = 0; i < COS_V288_N_TRANSPARENT; ++i) {
        cos_v288_transparent_t *t = &s->transparent[i];
        memset(t, 0, sizeof(*t));
        cpy(t->field, sizeof(t->field), TRANSPARENTS[i]);
        t->reported = true;
        if (strlen(t->field) > 0) s->n_transparent_rows_ok++;
        if (!t->reported) all_rep = false;
        prev = fnv1a(t->field,     strlen(t->field),     prev);
        prev = fnv1a(&t->reported, sizeof(t->reported),  prev);
    }
    s->transparency_all_reported_ok = all_rep;

    bool adaptive_both = (s->n_action_tighten >= 1) && (s->n_action_stable >= 1);

    int total   = COS_V288_N_CASCADE     + 1 + 1 +
                  COS_V288_N_EXTREME     + 1 +
                  COS_V288_N_ADAPTIVE    + 1 + 1 + 1 +
                  COS_V288_N_TRANSPARENT + 1;
    int passing = s->n_cascade_rows_ok +
                  (s->cascade_width_distinct_ok ? 1 : 0) +
                  (s->cascade_tau_monotone_ok   ? 1 : 0) +
                  s->n_extreme_rows_ok +
                  (s->extreme_polarity_ok ? 1 : 0) +
                  s->n_adaptive_rows_ok +
                  (s->adaptive_error_monotone_ok ? 1 : 0) +
                  (s->adaptive_action_rule_ok    ? 1 : 0) +
                  (adaptive_both ? 1 : 0) +
                  s->n_transparent_rows_ok +
                  (s->transparency_all_reported_ok ? 1 : 0);
    s->sigma_oculus = 1.0f - ((float)passing / (float)total);
    if (s->sigma_oculus < 0.0f) s->sigma_oculus = 0.0f;
    if (s->sigma_oculus > 1.0f) s->sigma_oculus = 1.0f;

    struct { int nc, ne, na, nat, nas, nt;
             bool wd, tm, ep, em, ar, tr;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nc  = s->n_cascade_rows_ok;
    trec.ne  = s->n_extreme_rows_ok;
    trec.na  = s->n_adaptive_rows_ok;
    trec.nat = s->n_action_tighten;
    trec.nas = s->n_action_stable;
    trec.nt  = s->n_transparent_rows_ok;
    trec.wd  = s->cascade_width_distinct_ok;
    trec.tm  = s->cascade_tau_monotone_ok;
    trec.ep  = s->extreme_polarity_ok;
    trec.em  = s->adaptive_error_monotone_ok;
    trec.ar  = s->adaptive_action_rule_ok;
    trec.tr  = s->transparency_all_reported_ok;
    trec.sigma = s->sigma_oculus;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *width_name(cos_v288_width_t w) {
    switch (w) {
    case COS_V288_WIDTH_TIGHT:  return "TIGHT";
    case COS_V288_WIDTH_NORMAL: return "NORMAL";
    case COS_V288_WIDTH_OPEN:   return "OPEN";
    }
    return "UNKNOWN";
}

static const char *action_name(cos_v288_action_t a) {
    switch (a) {
    case COS_V288_ACTION_TIGHTEN: return "TIGHTEN";
    case COS_V288_ACTION_STABLE:  return "STABLE";
    }
    return "UNKNOWN";
}

size_t cos_v288_to_json(const cos_v288_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v288\","
        "\"threshold_error\":%.3f,"
        "\"n_cascade_rows_ok\":%d,"
        "\"cascade_width_distinct_ok\":%s,"
        "\"cascade_tau_monotone_ok\":%s,"
        "\"n_extreme_rows_ok\":%d,\"extreme_polarity_ok\":%s,"
        "\"n_adaptive_rows_ok\":%d,"
        "\"adaptive_error_monotone_ok\":%s,"
        "\"adaptive_action_rule_ok\":%s,"
        "\"n_action_tighten\":%d,\"n_action_stable\":%d,"
        "\"n_transparent_rows_ok\":%d,"
        "\"transparency_all_reported_ok\":%s,"
        "\"sigma_oculus\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"cascade\":[",
        s->threshold_error,
        s->n_cascade_rows_ok,
        s->cascade_width_distinct_ok ? "true" : "false",
        s->cascade_tau_monotone_ok   ? "true" : "false",
        s->n_extreme_rows_ok,
        s->extreme_polarity_ok ? "true" : "false",
        s->n_adaptive_rows_ok,
        s->adaptive_error_monotone_ok ? "true" : "false",
        s->adaptive_action_rule_ok    ? "true" : "false",
        s->n_action_tighten, s->n_action_stable,
        s->n_transparent_rows_ok,
        s->transparency_all_reported_ok ? "true" : "false",
        s->sigma_oculus,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V288_N_CASCADE; ++i) {
        const cos_v288_cascade_t *c = &s->cascade[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"domain\":\"%s\",\"tau\":%.4f,\"width\":\"%s\"}",
            i == 0 ? "" : ",", c->domain, c->tau, width_name(c->width));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"extreme\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V288_N_EXTREME; ++i) {
        const cos_v288_extreme_t *e = &s->extreme[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"tau\":%.4f,"
            "\"useless\":%s,\"dangerous\":%s}",
            i == 0 ? "" : ",", e->label, e->tau,
            e->useless   ? "true" : "false",
            e->dangerous ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"adaptive\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V288_N_ADAPTIVE; ++i) {
        const cos_v288_adaptive_t *a = &s->adaptive[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"step\":%d,\"tau\":%.4f,"
            "\"error_rate\":%.4f,\"action\":\"%s\"}",
            i == 0 ? "" : ",", a->step, a->tau,
            a->error_rate, action_name(a->action));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"transparent\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V288_N_TRANSPARENT; ++i) {
        const cos_v288_transparent_t *t = &s->transparent[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"field\":\"%s\",\"reported\":%s}",
            i == 0 ? "" : ",", t->field,
            t->reported ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v288_self_test(void) {
    cos_v288_state_t s;
    cos_v288_init(&s, 0x288888ULL);
    cos_v288_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_D[COS_V288_N_CASCADE] = { "medical", "code", "creative" };
    for (int i = 0; i < COS_V288_N_CASCADE; ++i) {
        if (strcmp(s.cascade[i].domain, WANT_D[i]) != 0) return 2;
    }
    if (s.n_cascade_rows_ok          != COS_V288_N_CASCADE) return 3;
    if (!s.cascade_width_distinct_ok) return 4;
    if (!s.cascade_tau_monotone_ok)   return 5;

    static const char *WANT_E[COS_V288_N_EXTREME] = { "closed", "open", "optimal" };
    for (int i = 0; i < COS_V288_N_EXTREME; ++i) {
        if (strcmp(s.extreme[i].label, WANT_E[i]) != 0) return 6;
    }
    if (s.n_extreme_rows_ok     != COS_V288_N_EXTREME) return 7;
    if (!s.extreme_polarity_ok) return 8;

    if (s.n_adaptive_rows_ok != COS_V288_N_ADAPTIVE) return 9;
    if (!s.adaptive_error_monotone_ok) return 10;
    if (!s.adaptive_action_rule_ok)    return 11;
    if (s.n_action_tighten < 1) return 12;
    if (s.n_action_stable  < 1) return 13;

    static const char *WANT_T[COS_V288_N_TRANSPARENT] = {
        "tau_declared", "sigma_measured", "decision_visible"
    };
    for (int i = 0; i < COS_V288_N_TRANSPARENT; ++i) {
        if (strcmp(s.transparent[i].field, WANT_T[i]) != 0) return 14;
        if (!s.transparent[i].reported) return 15;
    }
    if (s.n_transparent_rows_ok       != COS_V288_N_TRANSPARENT) return 16;
    if (!s.transparency_all_reported_ok) return 17;

    if (s.sigma_oculus < 0.0f || s.sigma_oculus > 1.0f) return 18;
    if (s.sigma_oculus > 1e-6f) return 19;

    cos_v288_state_t u;
    cos_v288_init(&u, 0x288888ULL);
    cos_v288_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 20;
    return 0;
}
