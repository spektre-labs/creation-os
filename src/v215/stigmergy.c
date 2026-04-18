/*
 * v215 σ-Stigmergy — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "stigmergy.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v215_init(cos_v215_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed         = seed ? seed : 0x215F7EA1ULL;
    s->tau_trail    = 0.40f;
    s->lambda_decay = 0.08f;
    s->t_final      = COS_V215_N_STEPS - 1;
}

/*  Fixture:
 *    Trails 0..3  are TRUE (low-σ reinforcements)
 *    Trails 4..5  are FALSE (high-σ reinforcements)
 *  Each TRUE trail collects 4 early marks and 2 follow-up
 *  marks from different mesh nodes.  Each FALSE trail
 *  collects a mix that drives strength below τ_trail. */
typedef struct { int t; int node; float sigma; } mk_t;

static const mk_t T0[] = {
    {0,0,0.10f},{1,1,0.12f},{2,2,0.15f},{3,3,0.20f},{8,0,0.18f},{12,2,0.22f}
};
static const mk_t T1[] = {
    {0,1,0.08f},{1,2,0.14f},{2,3,0.18f},{3,0,0.20f},{10,1,0.16f},{15,3,0.25f}
};
static const mk_t T2[] = {
    {1,2,0.12f},{2,3,0.18f},{3,0,0.22f},{4,1,0.20f},{11,2,0.20f},{14,0,0.28f}
};
static const mk_t T3[] = {
    {2,3,0.15f},{3,0,0.18f},{4,1,0.20f},{5,2,0.22f},{13,3,0.24f},{17,1,0.26f}
};
/* FALSE: only early weak marks; no follow-up. */
static const mk_t T4[] = {
    {0,0,0.82f},{1,1,0.88f},{2,2,0.90f}
};
static const mk_t T5[] = {
    {0,1,0.78f},{2,2,0.92f}
};

static const mk_t * const TR[COS_V215_N_TRAILS] = { T0, T1, T2, T3, T4, T5 };
static const int          NTR[COS_V215_N_TRAILS] = {
    sizeof(T0)/sizeof(T0[0]), sizeof(T1)/sizeof(T1[0]),
    sizeof(T2)/sizeof(T2[0]), sizeof(T3)/sizeof(T3[0]),
    sizeof(T4)/sizeof(T4[0]), sizeof(T5)/sizeof(T5[0])
};
static const int          ISTRUE[COS_V215_N_TRAILS] = { 1,1,1,1,0,0 };

void cos_v215_build(cos_v215_state_t *s) {
    for (int i = 0; i < COS_V215_N_TRAILS; ++i) {
        cos_v215_trail_t *tr = &s->trails[i];
        memset(tr, 0, sizeof(*tr));
        tr->id            = i;
        tr->is_true_trail = ISTRUE[i] != 0;
        tr->n_marks       = NTR[i];
        for (int k = 0; k < NTR[i]; ++k) {
            tr->marks[k].t                   = TR[i][k].t;
            tr->marks[k].author_node         = TR[i][k].node;
            tr->marks[k].sigma_product_write = TR[i][k].sigma;
        }
    }
}

void cos_v215_run(cos_v215_state_t *s) {
    s->n_true_alive  = 0;
    s->n_false_alive = 0;

    uint64_t prev = 0x215BA4E5ULL;
    for (int i = 0; i < COS_V215_N_TRAILS; ++i) {
        cos_v215_trail_t *tr = &s->trails[i];

        /* Closed-form strength at t=t_final:
         *   sum_k max(0, 1 - σ_k) · e^{-λ·(t_final - t_k)} */
        float strength = 0.0f;
        int   nodes_mask = 0;
        int   distinct = 0;
        for (int k = 0; k < tr->n_marks; ++k) {
            float s_strong = 1.0f - tr->marks[k].sigma_product_write;
            if (s_strong < 0.0f) s_strong = 0.0f;
            float dt = (float)(s->t_final - tr->marks[k].t);
            float w  = expf(-s->lambda_decay * dt);
            strength += s_strong * w;
            int node = tr->marks[k].author_node & 0x1f;
            int bit  = 1 << node;
            if (!(nodes_mask & bit)) { distinct++; nodes_mask |= bit; }
        }
        /* Normalise into [0, 1] — denominator is the max
         * possible from this many marks at σ=0 with t=t_final. */
        float max_possible = 0.0f;
        for (int k = 0; k < tr->n_marks; ++k) {
            float dt = (float)(s->t_final - tr->marks[k].t);
            max_possible += expf(-s->lambda_decay * dt);
        }
        if (max_possible < 1e-6f) max_possible = 1.0f;
        strength /= max_possible;
        if (strength < 0.0f) strength = 0.0f;
        if (strength > 1.0f) strength = 1.0f;

        tr->strength_final = strength;
        tr->n_reinforcers  = distinct;
        tr->nodes_mask     = nodes_mask;
        tr->alive_at_end   = strength >= s->tau_trail;

        if (tr->is_true_trail && tr->alive_at_end) s->n_true_alive++;
        if (!tr->is_true_trail && tr->alive_at_end) s->n_false_alive++;

        tr->hash_prev = prev;
        struct { int id, is_true, n_marks, n_reinf, nodes;
                 float strength; int alive; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id       = tr->id;
        rec.is_true  = tr->is_true_trail ? 1 : 0;
        rec.n_marks  = tr->n_marks;
        rec.n_reinf  = tr->n_reinforcers;
        rec.nodes    = tr->nodes_mask;
        rec.strength = tr->strength_final;
        rec.alive    = tr->alive_at_end ? 1 : 0;
        rec.prev     = prev;
        tr->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = tr->hash_curr;
    }
    s->terminal_hash = prev;

    uint64_t v = 0x215BA4E5ULL;
    s->chain_valid = true;
    for (int i = 0; i < COS_V215_N_TRAILS; ++i) {
        const cos_v215_trail_t *tr = &s->trails[i];
        if (tr->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, is_true, n_marks, n_reinf, nodes;
                 float strength; int alive; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id       = tr->id;
        rec.is_true  = tr->is_true_trail ? 1 : 0;
        rec.n_marks  = tr->n_marks;
        rec.n_reinf  = tr->n_reinforcers;
        rec.nodes    = tr->nodes_mask;
        rec.strength = tr->strength_final;
        rec.alive    = tr->alive_at_end ? 1 : 0;
        rec.prev     = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != tr->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v215_to_json(const cos_v215_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v215\","
        "\"n_trails\":%d,\"tau_trail\":%.3f,\"lambda\":%.3f,"
        "\"t_final\":%d,"
        "\"n_true_alive\":%d,\"n_false_alive\":%d,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"trails\":[",
        COS_V215_N_TRAILS, s->tau_trail, s->lambda_decay,
        s->t_final, s->n_true_alive, s->n_false_alive,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V215_N_TRAILS; ++i) {
        const cos_v215_trail_t *tr = &s->trails[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"is_true\":%s,\"n_marks\":%d,"
            "\"n_reinf\":%d,\"nodes\":%d,"
            "\"strength\":%.4f,\"alive\":%s}",
            i == 0 ? "" : ",", tr->id,
            tr->is_true_trail ? "true" : "false",
            tr->n_marks, tr->n_reinforcers, tr->nodes_mask,
            tr->strength_final,
            tr->alive_at_end ? "true" : "false");
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v215_self_test(void) {
    cos_v215_state_t s;
    cos_v215_init(&s, 0x215F7EA1ULL);
    cos_v215_build(&s);
    cos_v215_run(&s);

    if (!s.chain_valid)            return 1;
    if (s.n_true_alive != 4)       return 2;
    if (s.n_false_alive != 0)       return 3;

    for (int i = 0; i < COS_V215_N_TRAILS; ++i) {
        const cos_v215_trail_t *tr = &s.trails[i];
        if (tr->strength_final < 0.0f ||
            tr->strength_final > 1.0f) return 4;
        if (tr->is_true_trail) {
            if (!tr->alive_at_end)        return 5;
            if (tr->n_reinforcers < 3)    return 6;
        } else {
            if (tr->alive_at_end)         return 7;
        }
    }
    return 0;
}
