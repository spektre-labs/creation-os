/*
 * v277 σ-Distill-Runtime — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "distill_runtime.h"

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

static const struct { int id; float s; }
    FILTER[COS_V277_N_FILTER] = {
    { 0, 0.09f },   /* LEARN  */
    { 1, 0.20f },   /* LEARN  */
    { 2, 0.36f },   /* SKIP   */
    { 3, 0.62f },   /* SKIP   */
};

static const struct { const char *n; float s; }
    DOMAINS[COS_V277_N_DOMAINS] = {
    { "law",     0.18f },   /* LOCAL_ONLY */
    { "code",    0.27f },   /* LOCAL_ONLY */
    { "medical", 0.44f },   /* API        */
};

static const struct { const char *l; float api; float local; float cost; }
    TRAJ[COS_V277_N_CHECKPTS] = {
    { "month_0",  0.80f, 0.20f, 200.00f },
    { "month_1",  0.60f, 0.40f, 140.00f },
    { "month_3",  0.30f, 0.70f,  60.00f },
    { "month_12", 0.05f, 0.95f,  20.00f },
};

void cos_v277_init(cos_v277_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x277D15ULL;
    s->tau_learn  = 0.25f;
    s->tau_domain = 0.30f;
}

void cos_v277_run(cos_v277_state_t *s) {
    uint64_t prev = 0x277D1500ULL;

    cpy(s->pair.teacher, sizeof(s->pair.teacher), "api-claude");
    cpy(s->pair.student, sizeof(s->pair.student), "bitnet-3B-local");
    s->pair_ok =
        (strlen(s->pair.teacher) > 0) &&
        (strlen(s->pair.student) > 0) &&
        (strcmp(s->pair.teacher, s->pair.student) != 0);
    prev = fnv1a(s->pair.teacher, strlen(s->pair.teacher), prev);
    prev = fnv1a(s->pair.student, strlen(s->pair.student), prev);
    prev = fnv1a(&s->pair_ok, sizeof(s->pair_ok), prev);

    s->n_filter_rows_ok = 0;
    s->n_filter_learn = s->n_filter_skip = 0;
    for (int i = 0; i < COS_V277_N_FILTER; ++i) {
        cos_v277_filter_t *f = &s->filter[i];
        memset(f, 0, sizeof(*f));
        f->query_id      = FILTER[i].id;
        f->sigma_teacher = FILTER[i].s;
        f->decision      = (f->sigma_teacher <= s->tau_learn)
                               ? COS_V277_FILT_LEARN
                               : COS_V277_FILT_SKIP;
        if (f->sigma_teacher >= 0.0f && f->sigma_teacher <= 1.0f)
            s->n_filter_rows_ok++;
        if (f->decision == COS_V277_FILT_LEARN) s->n_filter_learn++;
        else                                    s->n_filter_skip++;
        prev = fnv1a(&f->query_id,      sizeof(f->query_id),      prev);
        prev = fnv1a(&f->sigma_teacher, sizeof(f->sigma_teacher), prev);
        prev = fnv1a(&f->decision,      sizeof(f->decision),      prev);
    }

    s->n_domain_rows_ok = 0;
    s->n_domain_local = s->n_domain_api = 0;
    for (int i = 0; i < COS_V277_N_DOMAINS; ++i) {
        cos_v277_domain_t *d = &s->domains[i];
        memset(d, 0, sizeof(*d));
        cpy(d->name, sizeof(d->name), DOMAINS[i].n);
        d->sigma_domain = DOMAINS[i].s;
        d->route = (d->sigma_domain <= s->tau_domain)
                       ? COS_V277_ROUTE_LOCAL_ONLY
                       : COS_V277_ROUTE_API;
        if (d->sigma_domain >= 0.0f && d->sigma_domain <= 1.0f)
            s->n_domain_rows_ok++;
        if (d->route == COS_V277_ROUTE_LOCAL_ONLY) s->n_domain_local++;
        else                                       s->n_domain_api++;
        prev = fnv1a(d->name, strlen(d->name), prev);
        prev = fnv1a(&d->sigma_domain, sizeof(d->sigma_domain), prev);
        prev = fnv1a(&d->route,        sizeof(d->route),        prev);
    }

    s->n_trajectory_rows_ok = 0;
    s->traj_shares_ok         = true;
    s->traj_monotone_api_ok   = true;
    s->traj_monotone_local_ok = true;
    s->traj_monotone_cost_ok  = true;
    for (int i = 0; i < COS_V277_N_CHECKPTS; ++i) {
        cos_v277_checkpt_t *c = &s->traj[i];
        memset(c, 0, sizeof(*c));
        cpy(c->label, sizeof(c->label), TRAJ[i].l);
        c->api_share          = TRAJ[i].api;
        c->local_share        = TRAJ[i].local;
        c->cost_eur_per_month = TRAJ[i].cost;
        if (c->api_share  >= 0.0f && c->api_share  <= 1.0f &&
            c->local_share >= 0.0f && c->local_share <= 1.0f &&
            c->cost_eur_per_month >= 0.0f)
            s->n_trajectory_rows_ok++;
        if (fabsf(c->api_share + c->local_share - 1.0f) > 1e-4f)
            s->traj_shares_ok = false;
        if (i > 0) {
            if (!(c->api_share   < s->traj[i-1].api_share))   s->traj_monotone_api_ok   = false;
            if (!(c->local_share > s->traj[i-1].local_share)) s->traj_monotone_local_ok = false;
            if (!(c->cost_eur_per_month <
                  s->traj[i-1].cost_eur_per_month))
                s->traj_monotone_cost_ok = false;
        }
        prev = fnv1a(c->label, strlen(c->label), prev);
        prev = fnv1a(&c->api_share,          sizeof(c->api_share),          prev);
        prev = fnv1a(&c->local_share,        sizeof(c->local_share),        prev);
        prev = fnv1a(&c->cost_eur_per_month, sizeof(c->cost_eur_per_month), prev);
    }
    s->traj_anchors_ok =
        (s->traj[0].api_share >= 0.75f) &&
        (s->traj[COS_V277_N_CHECKPTS - 1].api_share <= 0.10f);

    bool filter_both = (s->n_filter_learn >= 1) && (s->n_filter_skip >= 1);
    bool domain_both = (s->n_domain_local >= 1) && (s->n_domain_api >= 1);

    int total   = 1 +
                  COS_V277_N_FILTER  + 1 +
                  COS_V277_N_DOMAINS + 1 +
                  COS_V277_N_CHECKPTS + 1 + 1 + 1 + 1 + 1;
    int passing = (s->pair_ok ? 1 : 0) +
                  s->n_filter_rows_ok +
                  (filter_both ? 1 : 0) +
                  s->n_domain_rows_ok +
                  (domain_both ? 1 : 0) +
                  s->n_trajectory_rows_ok +
                  (s->traj_shares_ok         ? 1 : 0) +
                  (s->traj_monotone_api_ok   ? 1 : 0) +
                  (s->traj_monotone_local_ok ? 1 : 0) +
                  (s->traj_monotone_cost_ok  ? 1 : 0) +
                  (s->traj_anchors_ok        ? 1 : 0);
    s->sigma_distill = 1.0f - ((float)passing / (float)total);
    if (s->sigma_distill < 0.0f) s->sigma_distill = 0.0f;
    if (s->sigma_distill > 1.0f) s->sigma_distill = 1.0f;

    struct { int nf, nfl, nfs, nd, ndl, nda, nt;
             bool po, ts, ma, ml, mc, ta;
             float sigma, tl, td; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nf  = s->n_filter_rows_ok;
    trec.nfl = s->n_filter_learn;
    trec.nfs = s->n_filter_skip;
    trec.nd  = s->n_domain_rows_ok;
    trec.ndl = s->n_domain_local;
    trec.nda = s->n_domain_api;
    trec.nt  = s->n_trajectory_rows_ok;
    trec.po  = s->pair_ok;
    trec.ts  = s->traj_shares_ok;
    trec.ma  = s->traj_monotone_api_ok;
    trec.ml  = s->traj_monotone_local_ok;
    trec.mc  = s->traj_monotone_cost_ok;
    trec.ta  = s->traj_anchors_ok;
    trec.sigma = s->sigma_distill;
    trec.tl  = s->tau_learn;
    trec.td  = s->tau_domain;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *filt_name(cos_v277_filt_t f) {
    switch (f) {
    case COS_V277_FILT_LEARN: return "LEARN";
    case COS_V277_FILT_SKIP:  return "SKIP";
    }
    return "UNKNOWN";
}

static const char *route_name(cos_v277_route_t r) {
    switch (r) {
    case COS_V277_ROUTE_LOCAL_ONLY: return "LOCAL_ONLY";
    case COS_V277_ROUTE_API:        return "API";
    }
    return "UNKNOWN";
}

size_t cos_v277_to_json(const cos_v277_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v277\","
        "\"tau_learn\":%.3f,\"tau_domain\":%.3f,"
        "\"pair\":{\"teacher\":\"%s\",\"student\":\"%s\"},"
        "\"pair_ok\":%s,"
        "\"n_filter_rows_ok\":%d,"
        "\"n_filter_learn\":%d,\"n_filter_skip\":%d,"
        "\"n_domain_rows_ok\":%d,"
        "\"n_domain_local\":%d,\"n_domain_api\":%d,"
        "\"n_trajectory_rows_ok\":%d,"
        "\"traj_shares_ok\":%s,"
        "\"traj_monotone_api_ok\":%s,"
        "\"traj_monotone_local_ok\":%s,"
        "\"traj_monotone_cost_ok\":%s,"
        "\"traj_anchors_ok\":%s,"
        "\"sigma_distill\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"filter\":[",
        s->tau_learn, s->tau_domain,
        s->pair.teacher, s->pair.student,
        s->pair_ok ? "true" : "false",
        s->n_filter_rows_ok,
        s->n_filter_learn, s->n_filter_skip,
        s->n_domain_rows_ok,
        s->n_domain_local, s->n_domain_api,
        s->n_trajectory_rows_ok,
        s->traj_shares_ok         ? "true" : "false",
        s->traj_monotone_api_ok   ? "true" : "false",
        s->traj_monotone_local_ok ? "true" : "false",
        s->traj_monotone_cost_ok  ? "true" : "false",
        s->traj_anchors_ok        ? "true" : "false",
        s->sigma_distill,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V277_N_FILTER; ++i) {
        const cos_v277_filter_t *f = &s->filter[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"query_id\":%d,\"sigma_teacher\":%.4f,\"decision\":\"%s\"}",
            i == 0 ? "" : ",", f->query_id, f->sigma_teacher,
            filt_name(f->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"domains\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V277_N_DOMAINS; ++i) {
        const cos_v277_domain_t *d = &s->domains[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"sigma_domain\":%.4f,\"route\":\"%s\"}",
            i == 0 ? "" : ",", d->name, d->sigma_domain,
            route_name(d->route));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"trajectory\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V277_N_CHECKPTS; ++i) {
        const cos_v277_checkpt_t *c = &s->traj[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"api_share\":%.4f,"
            "\"local_share\":%.4f,\"cost_eur_per_month\":%.2f}",
            i == 0 ? "" : ",", c->label,
            c->api_share, c->local_share, c->cost_eur_per_month);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v277_self_test(void) {
    cos_v277_state_t s;
    cos_v277_init(&s, 0x277D15ULL);
    cos_v277_run(&s);
    if (!s.chain_valid) return 1;
    if (!s.pair_ok) return 2;
    if (strcmp(s.pair.teacher, "api-claude")      != 0) return 3;
    if (strcmp(s.pair.student, "bitnet-3B-local") != 0) return 4;

    for (int i = 0; i < COS_V277_N_FILTER; ++i) {
        cos_v277_filt_t exp =
            (s.filter[i].sigma_teacher <= s.tau_learn)
                ? COS_V277_FILT_LEARN : COS_V277_FILT_SKIP;
        if (s.filter[i].decision != exp) return 5;
    }
    if (s.n_filter_rows_ok != COS_V277_N_FILTER) return 6;
    if (s.n_filter_learn < 1) return 7;
    if (s.n_filter_skip  < 1) return 8;

    const char *dn[COS_V277_N_DOMAINS] = { "law","code","medical" };
    for (int i = 0; i < COS_V277_N_DOMAINS; ++i) {
        if (strcmp(s.domains[i].name, dn[i]) != 0) return 9;
        cos_v277_route_t exp =
            (s.domains[i].sigma_domain <= s.tau_domain)
                ? COS_V277_ROUTE_LOCAL_ONLY : COS_V277_ROUTE_API;
        if (s.domains[i].route != exp) return 10;
    }
    if (s.n_domain_rows_ok != COS_V277_N_DOMAINS) return 11;
    if (s.n_domain_local < 1) return 12;
    if (s.n_domain_api   < 1) return 13;

    if (s.n_trajectory_rows_ok != COS_V277_N_CHECKPTS) return 14;
    if (!s.traj_shares_ok)         return 15;
    if (!s.traj_monotone_api_ok)   return 16;
    if (!s.traj_monotone_local_ok) return 17;
    if (!s.traj_monotone_cost_ok)  return 18;
    if (!s.traj_anchors_ok)        return 19;

    if (s.sigma_distill < 0.0f || s.sigma_distill > 1.0f) return 20;
    if (s.sigma_distill > 1e-6f) return 21;

    cos_v277_state_t u;
    cos_v277_init(&u, 0x277D15ULL);
    cos_v277_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 22;
    return 0;
}
