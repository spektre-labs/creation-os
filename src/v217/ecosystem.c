/*
 * v217 σ-Ecosystem — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "ecosystem.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v217_init(cos_v217_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed         = seed ? seed : 0x217EC051ULL;
    s->tau_dom      = 0.40f;
    s->tau_healthy  = 0.55f;
    s->pop_total    = 100;
}

/* Balanced ecosystem: 32 producers, 28 consumers,
 * 22 decomposers, 18 predators (no level > 40 %). */
typedef struct { const char *name; int pop; float compute; } lvl_fx_t;
static const lvl_fx_t LFX[COS_V217_N_LEVELS] = {
    { "producer_v174_flywheel",   32, 0.30f },
    { "consumer_v101_inference",  28, 0.38f },
    { "decomposer_v177_v159",     22, 0.18f },
    { "predator_v122_v210",       18, 0.14f }
};

typedef struct { int kind; const char *a; const char *b; float s; } pair_fx_t;
static const pair_fx_t PFX[COS_V217_N_PAIRS] = {
    { COS_V217_SYM_MUTUAL,    "v120_big_teacher",    "v120_small_student",    0.10f },
    { COS_V217_SYM_COMPETE,   "v150_debate_a",       "v150_debate_b",         0.25f },
    { COS_V217_SYM_COMMENSAL, "v215_stigmergy_a",    "v215_stigmergy_b",      0.15f },
    { COS_V217_SYM_MUTUAL,    "v121_planner",        "v151_code_agent",       0.12f },
    { COS_V217_SYM_COMPETE,   "v210_guardian",       "v122_redteam",          0.22f }
};

void cos_v217_build(cos_v217_state_t *s) {
    for (int i = 0; i < COS_V217_N_LEVELS; ++i) {
        cos_v217_trophic_t *t = &s->levels[i];
        memset(t, 0, sizeof(*t));
        t->id = i;
        strncpy(t->name, LFX[i].name, COS_V217_STR_MAX - 1);
        t->population    = LFX[i].pop;
        t->compute_share = LFX[i].compute;
        t->share         = (float)t->population / (float)s->pop_total;
    }
    for (int i = 0; i < COS_V217_N_PAIRS; ++i) {
        cos_v217_symbiosis_t *p = &s->pairs[i];
        memset(p, 0, sizeof(*p));
        p->id   = i;
        p->kind = PFX[i].kind;
        strncpy(p->a, PFX[i].a, COS_V217_STR_MAX - 1);
        strncpy(p->b, PFX[i].b, COS_V217_STR_MAX - 1);
        p->sigma_pair = PFX[i].s;
    }
}

void cos_v217_run(cos_v217_state_t *s) {
    /* σ_dominance = max share. */
    float max_share = 0.0f, min_share = 1.0f;
    for (int i = 0; i < COS_V217_N_LEVELS; ++i) {
        if (s->levels[i].share > max_share) max_share = s->levels[i].share;
        if (s->levels[i].share < min_share) min_share = s->levels[i].share;
    }
    s->sigma_dominance = max_share;
    s->sigma_balance   = (max_share > 0.0f) ? (1.0f - min_share / max_share) : 1.0f;

    float sym_sum = 0.0f;
    for (int i = 0; i < COS_V217_N_PAIRS; ++i)
        sym_sum += s->pairs[i].sigma_pair;
    s->sigma_symbiosis = sym_sum / (float)COS_V217_N_PAIRS;

    float se = 0.4f * s->sigma_dominance
              + 0.4f * s->sigma_balance
              + 0.2f * s->sigma_symbiosis;
    if (se < 0.0f) se = 0.0f;
    if (se > 1.0f) se = 1.0f;
    s->sigma_ecosystem = se;
    s->k_eff = 1.0f - se;

    uint64_t prev = 0x217EC111ULL;
    for (int i = 0; i < COS_V217_N_LEVELS; ++i) {
        struct { int id, pop; float share, compute; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id      = s->levels[i].id;
        rec.pop     = s->levels[i].population;
        rec.share   = s->levels[i].share;
        rec.compute = s->levels[i].compute_share;
        rec.prev    = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    for (int i = 0; i < COS_V217_N_PAIRS; ++i) {
        struct { int id, kind; float s; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id   = s->pairs[i].id;
        rec.kind = s->pairs[i].kind;
        rec.s    = s->pairs[i].sigma_pair;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    {
        struct { float dom, bal, sym, eco, keff; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.dom = s->sigma_dominance;
        rec.bal = s->sigma_balance;
        rec.sym = s->sigma_symbiosis;
        rec.eco = s->sigma_ecosystem;
        rec.keff = s->k_eff;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    s->terminal_hash = prev;
    s->chain_valid = true;

    /* Replay. */
    uint64_t v = 0x217EC111ULL;
    for (int i = 0; i < COS_V217_N_LEVELS; ++i) {
        struct { int id, pop; float share, compute; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id      = s->levels[i].id;
        rec.pop     = s->levels[i].population;
        rec.share   = s->levels[i].share;
        rec.compute = s->levels[i].compute_share;
        rec.prev    = v;
        v = fnv1a(&rec, sizeof(rec), v);
    }
    for (int i = 0; i < COS_V217_N_PAIRS; ++i) {
        struct { int id, kind; float s; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id   = s->pairs[i].id;
        rec.kind = s->pairs[i].kind;
        rec.s    = s->pairs[i].sigma_pair;
        rec.prev = v;
        v = fnv1a(&rec, sizeof(rec), v);
    }
    {
        struct { float dom, bal, sym, eco, keff; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.dom = s->sigma_dominance;
        rec.bal = s->sigma_balance;
        rec.sym = s->sigma_symbiosis;
        rec.eco = s->sigma_ecosystem;
        rec.keff = s->k_eff;
        rec.prev = v;
        v = fnv1a(&rec, sizeof(rec), v);
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v217_to_json(const cos_v217_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v217\","
        "\"pop_total\":%d,\"tau_dom\":%.3f,\"tau_healthy\":%.3f,"
        "\"sigma_dominance\":%.4f,\"sigma_balance\":%.4f,"
        "\"sigma_symbiosis\":%.4f,\"sigma_ecosystem\":%.4f,"
        "\"k_eff\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"levels\":[",
        s->pop_total, s->tau_dom, s->tau_healthy,
        s->sigma_dominance, s->sigma_balance,
        s->sigma_symbiosis, s->sigma_ecosystem, s->k_eff,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V217_N_LEVELS; ++i) {
        const cos_v217_trophic_t *t = &s->levels[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"name\":\"%s\",\"pop\":%d,"
            "\"share\":%.4f,\"compute\":%.3f}",
            i == 0 ? "" : ",", t->id, t->name,
            t->population, t->share, t->compute_share);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m1 = snprintf(buf + off, cap - off, "],\"pairs\":[");
    if (m1 < 0 || off + (size_t)m1 >= cap) return 0;
    off += (size_t)m1;
    for (int i = 0; i < COS_V217_N_PAIRS; ++i) {
        const cos_v217_symbiosis_t *pp = &s->pairs[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"kind\":%d,\"s\":%.3f}",
            i == 0 ? "" : ",", pp->id, pp->kind, pp->sigma_pair);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m2 = snprintf(buf + off, cap - off, "]}");
    if (m2 < 0 || off + (size_t)m2 >= cap) return 0;
    return off + (size_t)m2;
}

int cos_v217_self_test(void) {
    cos_v217_state_t s;
    cos_v217_init(&s, 0x217EC051ULL);
    cos_v217_build(&s);
    cos_v217_run(&s);

    int sum = 0;
    for (int i = 0; i < COS_V217_N_LEVELS; ++i)
        sum += s.levels[i].population;
    if (sum != s.pop_total)                return 1;

    for (int i = 0; i < COS_V217_N_LEVELS; ++i)
        if (s.levels[i].share > s.tau_dom) return 2;

    if (COS_V217_N_PAIRS < 3)              return 3;
    for (int i = 0; i < COS_V217_N_PAIRS; ++i) {
        if (s.pairs[i].sigma_pair < 0.0f ||
            s.pairs[i].sigma_pair > 1.0f)  return 4;
    }
    if (s.sigma_ecosystem < 0.0f || s.sigma_ecosystem > 1.0f) return 5;
    if (s.k_eff < 0.0f || s.k_eff > 1.0f)  return 5;
    if (!(s.k_eff > s.tau_healthy))         return 6;
    if (!s.chain_valid)                     return 7;
    return 0;
}
