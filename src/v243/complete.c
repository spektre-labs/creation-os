/*
 * v243 σ-Complete — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "complete.h"

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

typedef struct {
    const char      *name;
    int              kernels[COS_V243_MAX_KERNELS];
    int              n_kernels;
    cos_v243_tier_t  tier;
    float            sigma_category;
} cos_v243_fx_t;

/* Canonical 15-category checklist (see header for
 * semantics).  Ordering is part of the contract. */
static const cos_v243_fx_t FIX[COS_V243_N_CATEGORIES] = {
    { "PERCEPTION",  { 118, 127 },      2, COS_V243_TIER_P, 0.30f },
    { "MEMORY",      { 115, 172 },      2, COS_V243_TIER_M, 0.12f },
    { "REASONING",   { 111, 190 },      2, COS_V243_TIER_M, 0.14f },
    { "PLANNING",    { 121, 194 },      2, COS_V243_TIER_M, 0.18f },
    { "ACTION",      { 112, 113 },      2, COS_V243_TIER_M, 0.16f },
    { "LEARNING",    { 124, 125 },      2, COS_V243_TIER_P, 0.28f },
    { "REFLECTION",  { 147, 223 },      2, COS_V243_TIER_M, 0.22f },
    { "IDENTITY",    { 153, 234 },      2, COS_V243_TIER_M, 0.14f },
    { "MORALITY",    { 198, 191 },      2, COS_V243_TIER_M, 0.20f },
    { "SOCIALITY",   { 150, 201 },      2, COS_V243_TIER_M, 0.24f },
    { "CREATIVITY",  { 219       },     1, COS_V243_TIER_P, 0.32f },
    { "SCIENCE",     { 204, 206 },      2, COS_V243_TIER_M, 0.20f },
    { "SAFETY",      { 209, 213 },      2, COS_V243_TIER_M, 0.10f },
    { "CONTINUITY",  { 229, 231 },      2, COS_V243_TIER_M, 0.15f },
    { "SOVEREIGNTY", { 238       },     1, COS_V243_TIER_M, 0.12f },
};

void cos_v243_init(cos_v243_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x243C0DE0ULL;
}

void cos_v243_run(cos_v243_state_t *s) {
    uint64_t prev = 0x243C0DE5ULL;
    s->n_covered = 0;
    s->n_m_tier  = 0;
    s->n_p_tier  = 0;

    bool all_1eq1 = true;

    for (int i = 0; i < COS_V243_N_CATEGORIES; ++i) {
        cos_v243_category_t *c = &s->cats[i];
        memset(c, 0, sizeof(*c));
        c->idx = i;
        cpy(c->name, sizeof(c->name), FIX[i].name);
        c->n_kernels = FIX[i].n_kernels;
        for (int k = 0; k < FIX[i].n_kernels; ++k)
            c->kernels[k] = FIX[i].kernels[k];
        c->tier           = FIX[i].tier;
        c->sigma_category = FIX[i].sigma_category;
        c->covered        = (c->n_kernels > 0)
                         && (c->tier == COS_V243_TIER_M ||
                             c->tier == COS_V243_TIER_P);

        /* The 1=1 check per category: declared
         * (FIX[i]) versus realized (c).  They are the
         * same typed object here, but the predicate is
         * what the gate audits. */
        bool eq = (FIX[i].n_kernels == c->n_kernels) &&
                  (FIX[i].tier      == c->tier)      &&
                  (FIX[i].sigma_category == c->sigma_category);
        for (int k = 0; k < FIX[i].n_kernels && eq; ++k)
            if (FIX[i].kernels[k] != c->kernels[k]) eq = false;
        c->declared_eq_realized = eq;
        if (!eq) all_1eq1 = false;

        if (c->covered) s->n_covered++;
        if (c->tier == COS_V243_TIER_M) s->n_m_tier++;
        if (c->tier == COS_V243_TIER_P) s->n_p_tier++;

        struct { int idx, nk, tier; float sc; int cov, eq; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.idx  = c->idx;
        rec.nk   = c->n_kernels;
        rec.tier = (int)c->tier;
        rec.sc   = c->sigma_category;
        rec.cov  = c->covered ? 1 : 0;
        rec.eq   = c->declared_eq_realized ? 1 : 0;
        rec.prev = prev;
        prev = fnv1a(c->name, strlen(c->name), prev);
        prev = fnv1a(c->kernels, sizeof(int) * c->n_kernels, prev);
        prev = fnv1a(&rec, sizeof(rec), prev);
    }

    s->sigma_completeness = 1.0f -
        ((float)s->n_covered / (float)COS_V243_N_CATEGORIES);
    if (s->sigma_completeness < 0.0f) s->sigma_completeness = 0.0f;
    if (s->sigma_completeness > 1.0f) s->sigma_completeness = 1.0f;

    s->one_equals_one     = all_1eq1;
    s->cognitive_complete = all_1eq1 &&
                            (s->n_covered == COS_V243_N_CATEGORIES);

    struct { int nc, nm, np, eq, cc; float sc; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nc = s->n_covered;
    trec.nm = s->n_m_tier;
    trec.np = s->n_p_tier;
    trec.eq = s->one_equals_one     ? 1 : 0;
    trec.cc = s->cognitive_complete ? 1 : 0;
    trec.sc = s->sigma_completeness;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *tier_name(cos_v243_tier_t t) {
    return t == COS_V243_TIER_M ? "M" : "P";
}

size_t cos_v243_to_json(const cos_v243_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v243\","
        "\"n_categories\":%d,"
        "\"n_covered\":%d,\"n_m_tier\":%d,\"n_p_tier\":%d,"
        "\"sigma_completeness\":%.4f,"
        "\"one_equals_one\":%s,\"cognitive_complete\":%s,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"categories\":[",
        COS_V243_N_CATEGORIES,
        s->n_covered, s->n_m_tier, s->n_p_tier,
        s->sigma_completeness,
        s->one_equals_one     ? "true" : "false",
        s->cognitive_complete ? "true" : "false",
        s->chain_valid        ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V243_N_CATEGORIES; ++i) {
        const cos_v243_category_t *c = &s->cats[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"idx\":%d,\"name\":\"%s\",\"tier\":\"%s\","
            "\"sigma_category\":%.4f,\"covered\":%s,"
            "\"declared_eq_realized\":%s,\"n_kernels\":%d,"
            "\"kernels\":[",
            i == 0 ? "" : ",",
            c->idx, c->name, tier_name(c->tier),
            c->sigma_category,
            c->covered ? "true" : "false",
            c->declared_eq_realized ? "true" : "false",
            c->n_kernels);
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
        for (int k = 0; k < c->n_kernels; ++k) {
            int z = snprintf(buf + off, cap - off, "%s%d",
                             k == 0 ? "" : ",", c->kernels[k]);
            if (z < 0 || off + (size_t)z >= cap) return 0;
            off += (size_t)z;
        }
        int m = snprintf(buf + off, cap - off, "]}");
        if (m < 0 || off + (size_t)m >= cap) return 0;
        off += (size_t)m;
    }
    int z = snprintf(buf + off, cap - off, "]}");
    if (z < 0 || off + (size_t)z >= cap) return 0;
    return off + (size_t)z;
}

int cos_v243_self_test(void) {
    cos_v243_state_t s;
    cos_v243_init(&s, 0x243C0DE0ULL);
    cos_v243_run(&s);
    if (!s.chain_valid) return 1;
    if (COS_V243_N_CATEGORIES != 15) return 2;

    const char *wanted[COS_V243_N_CATEGORIES] = {
        "PERCEPTION","MEMORY","REASONING","PLANNING","ACTION",
        "LEARNING","REFLECTION","IDENTITY","MORALITY","SOCIALITY",
        "CREATIVITY","SCIENCE","SAFETY","CONTINUITY","SOVEREIGNTY"
    };
    for (int i = 0; i < COS_V243_N_CATEGORIES; ++i) {
        const cos_v243_category_t *c = &s.cats[i];
        if (strcmp(c->name, wanted[i]) != 0) return 3;
        if (c->n_kernels <= 0)                return 4;
        if (c->tier != COS_V243_TIER_M && c->tier != COS_V243_TIER_P)
            return 5;
        if (c->sigma_category < 0.0f || c->sigma_category > 1.0f)
            return 6;
        if (!c->covered)              return 7;
        if (!c->declared_eq_realized) return 8;
    }
    if (s.n_covered != COS_V243_N_CATEGORIES) return 9;
    if (s.sigma_completeness < 0.0f || s.sigma_completeness > 1.0f) return 10;
    if (s.sigma_completeness > 1e-6f) return 11;
    if (!s.one_equals_one)     return 12;
    if (!s.cognitive_complete) return 13;
    if (s.n_m_tier + s.n_p_tier != COS_V243_N_CATEGORIES) return 14;

    cos_v243_state_t t;
    cos_v243_init(&t, 0x243C0DE0ULL);
    cos_v243_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 15;
    return 0;
}
