/*
 * v207 σ-Design — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "design.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v207_init(cos_v207_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed  = seed ? seed : 0x207DE516ULL;
    s->c_max = 100.0f;
}

typedef struct { float perf, cplx; float cost;
                 int law, ethics; } fx_t;
static const fx_t FX[COS_V207_N_CANDIDATES] = {
    /* perf  cplx  cost    law ethics */
    { 0.85f, 0.35f,  85.0f,  1, 1 },   /*  0 pareto */
    { 0.78f, 0.25f,  70.0f,  1, 1 },   /*  1 pareto */
    { 0.60f, 0.10f,  40.0f,  1, 1 },   /*  2 pareto */
    { 0.90f, 0.55f,  95.0f,  1, 1 },   /*  3 pareto (high perf, high cplx) */
    { 0.50f, 0.40f,  60.0f,  1, 1 },   /*  4 dominated */
    { 0.72f, 0.45f,  80.0f,  1, 1 },   /*  5 dominated */
    { 0.65f, 0.50f,  72.0f,  1, 1 },   /*  6 dominated */
    { 0.95f, 0.90f, 120.0f,  1, 1 },   /*  7 INFEASIBLE (cost) */
    { 0.80f, 0.30f,  78.0f,  0, 1 },   /*  8 INFEASIBLE (law) */
    { 0.88f, 0.70f, 115.0f,  1, 0 },   /*  9 INFEASIBLE (ethics) */
    { 0.40f, 0.20f,  35.0f,  1, 1 },   /* 10 dominated */
    { 0.55f, 0.18f,  50.0f,  1, 1 }    /* 11 dominated by 2? let's see */
};

void cos_v207_build(cos_v207_state_t *s) {
    s->n = COS_V207_N_CANDIDATES;
    for (int i = 0; i < s->n; ++i) {
        cos_v207_candidate_t *c = &s->cands[i];
        memset(c, 0, sizeof(*c));
        c->id          = i;
        c->performance = FX[i].perf;
        c->complexity  = FX[i].cplx;
        c->cost        = FX[i].cost;
        c->law_ok      = FX[i].law    != 0;
        c->ethics_ok   = FX[i].ethics != 0;
    }
}

/* Dominance on the feasible set: a dominates b if
 *   perf_a ≥ perf_b, cplx_a ≤ cplx_b, cost_a ≤ cost_b,
 *   with at least one strict inequality.
 */
static bool dominates(const cos_v207_candidate_t *a,
                      const cos_v207_candidate_t *b) {
    bool ge = a->performance >= b->performance - 1e-6f;
    bool le_cplx = a->complexity <= b->complexity + 1e-6f;
    bool le_cost = a->cost       <= b->cost       + 1e-6f;
    if (!(ge && le_cplx && le_cost)) return false;
    bool strict =
        a->performance > b->performance + 1e-6f ||
        a->complexity  < b->complexity  - 1e-6f ||
        a->cost        < b->cost        - 1e-6f;
    return strict;
}

void cos_v207_run(cos_v207_state_t *s) {
    s->n_feasible = s->n_infeasible = s->n_pareto = 0;

    for (int i = 0; i < s->n; ++i) {
        cos_v207_candidate_t *c = &s->cands[i];
        c->cost_ok = c->cost <= s->c_max + 1e-6f;
        c->feasible = c->law_ok && c->ethics_ok && c->cost_ok;
        float penalty =
            (c->law_ok    ? 0.0f : 0.35f) +
            (c->ethics_ok ? 0.0f : 0.35f) +
            (c->cost_ok   ? 0.0f : 0.20f);
        /* base σ_feasibility from headroom. */
        float headroom = (s->c_max - c->cost) / s->c_max;
        if (headroom < 0.0f) headroom = 0.0f;
        float s_f = 0.10f + 0.20f * (1.0f - headroom) + penalty;
        if (s_f > 1.0f) s_f = 1.0f;
        c->sigma_feasibility = s_f;

        if (c->feasible) s->n_feasible++;
        else             s->n_infeasible++;
    }

    for (int i = 0; i < s->n; ++i) {
        cos_v207_candidate_t *c = &s->cands[i];
        if (!c->feasible) { c->pareto = false; continue; }
        bool on_front = true;
        for (int j = 0; j < s->n; ++j) {
            if (i == j) continue;
            const cos_v207_candidate_t *o = &s->cands[j];
            if (!o->feasible) continue;
            if (dominates(o, c)) { on_front = false; break; }
        }
        c->pareto = on_front;
        if (on_front) s->n_pareto++;
    }

    uint64_t prev = 0x207E5167ULL;
    for (int i = 0; i < s->n; ++i) {
        cos_v207_candidate_t *c = &s->cands[i];
        c->hash_prev = prev;
        struct { int id, law, ethics, cost_ok, feasible, pareto;
                 float perf, cplx, cost, s_f;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id       = c->id;
        rec.law      = c->law_ok    ? 1 : 0;
        rec.ethics   = c->ethics_ok ? 1 : 0;
        rec.cost_ok  = c->cost_ok   ? 1 : 0;
        rec.feasible = c->feasible  ? 1 : 0;
        rec.pareto   = c->pareto    ? 1 : 0;
        rec.perf     = c->performance;
        rec.cplx     = c->complexity;
        rec.cost     = c->cost;
        rec.s_f      = c->sigma_feasibility;
        rec.prev     = prev;
        c->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = c->hash_curr;
    }
    s->terminal_hash = prev;

    uint64_t v = 0x207E5167ULL;
    s->chain_valid = true;
    for (int i = 0; i < s->n; ++i) {
        const cos_v207_candidate_t *c = &s->cands[i];
        if (c->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, law, ethics, cost_ok, feasible, pareto;
                 float perf, cplx, cost, s_f;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id       = c->id;
        rec.law      = c->law_ok    ? 1 : 0;
        rec.ethics   = c->ethics_ok ? 1 : 0;
        rec.cost_ok  = c->cost_ok   ? 1 : 0;
        rec.feasible = c->feasible  ? 1 : 0;
        rec.pareto   = c->pareto    ? 1 : 0;
        rec.perf     = c->performance;
        rec.cplx     = c->complexity;
        rec.cost     = c->cost;
        rec.s_f      = c->sigma_feasibility;
        rec.prev     = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != c->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v207_to_json(const cos_v207_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v207\","
        "\"n\":%d,\"c_max\":%.3f,"
        "\"n_feasible\":%d,\"n_infeasible\":%d,\"n_pareto\":%d,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"candidates\":[",
        s->n, s->c_max,
        s->n_feasible, s->n_infeasible, s->n_pareto,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n; ++i) {
        const cos_v207_candidate_t *c = &s->cands[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"perf\":%.3f,\"cplx\":%.3f,"
            "\"cost\":%.3f,\"law\":%s,\"ethics\":%s,"
            "\"cost_ok\":%s,\"feasible\":%s,\"pareto\":%s,"
            "\"s_f\":%.3f}",
            i == 0 ? "" : ",", c->id, c->performance,
            c->complexity, c->cost,
            c->law_ok    ? "true" : "false",
            c->ethics_ok ? "true" : "false",
            c->cost_ok   ? "true" : "false",
            c->feasible  ? "true" : "false",
            c->pareto    ? "true" : "false",
            c->sigma_feasibility);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v207_self_test(void) {
    cos_v207_state_t s;
    cos_v207_init(&s, 0x207DE516ULL);
    cos_v207_build(&s);
    cos_v207_run(&s);

    if (s.n != COS_V207_N_CANDIDATES) return 1;
    if (s.n_pareto < 3)               return 2;
    if (s.n_infeasible < 1)           return 3;
    if (!s.chain_valid)                return 4;

    for (int i = 0; i < s.n; ++i) {
        const cos_v207_candidate_t *c = &s.cands[i];
        if (c->sigma_feasibility < 0.0f || c->sigma_feasibility > 1.0f)
            return 5;
        /* pareto ⇒ feasible */
        if (c->pareto && !c->feasible) return 6;
        /* pareto front must not be dominated by any feasible */
        if (c->pareto) {
            for (int j = 0; j < s.n; ++j) {
                if (i == j) continue;
                const cos_v207_candidate_t *o = &s.cands[j];
                if (!o->feasible) continue;
                bool ge = o->performance >= c->performance - 1e-6f;
                bool le_cplx = o->complexity <= c->complexity + 1e-6f;
                bool le_cost = o->cost       <= c->cost       + 1e-6f;
                bool strict =
                    o->performance > c->performance + 1e-6f ||
                    o->complexity  < c->complexity  - 1e-6f ||
                    o->cost        < c->cost        - 1e-6f;
                if (ge && le_cplx && le_cost && strict) return 7;
            }
        }
    }
    return 0;
}
