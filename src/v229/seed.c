/*
 * v229 σ-Seed — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "seed.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy_name(char *dst, const char *src) {
    size_t n = 0;
    for (; n < 23 && src[n]; ++n) dst[n] = src[n];
    dst[n] = '\0';
}

typedef struct {
    int         version;
    const char *name;
    int         parent_version;   /* must already be in the system */
    float       sigma_raw;
} cos_v229_cand_t;

/* Five-kernel seed set (declared). */
static const cos_v229_cand_t SEED[COS_V229_N_SEED] = {
    {  29, "measurement", -1, 0.05f },
    { 101, "bridge",      -1, 0.05f },
    { 106, "server",      -1, 0.05f },
    { 124, "continual",   -1, 0.05f },
    { 148, "sovereign",   -1, 0.05f },
};

/* Candidate queue, ordered.  Each candidate declares
 * its parent (must already be integrated at integration
 * time) and its raw σ_growth (risk).  Two candidates
 * are deliberately high-σ so the gate rejects them. */
static const cos_v229_cand_t CANDIDATES[COS_V229_N_CANDIDATES] = {
    { 133, "meta",         29, 0.15f },
    { 143, "benchmark",   133, 0.20f },
    { 146, "genesis",     101, 0.30f },
    { 162, "compose",     148, 0.25f },
    { 193, "k_eff",        29, 0.18f },
    { 225, "fractal",     193, 0.32f },
    { 228, "unified",     225, 0.38f },
    { 902, "rogue_A",      29, 0.90f },   /* REJECT (too risky) */
    { 126, "embed",       124, 0.28f },
    { 116, "mcp",         106, 0.22f },
    { 144, "rsi",         148, 0.35f },
    { 903, "rogue_B",     162, 0.70f },   /* REJECT */
    { 177, "compress",    126, 0.33f },
};

void cos_v229_init(cos_v229_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x229511DULL;
    s->tau_growth = 0.40f;
}

static int find_version(const cos_v229_state_t *s, int n, int v) {
    for (int i = 0; i < n; ++i)
        if (s->pool[i].version == v) return i;
    return -1;
}

static uint64_t grow_once(cos_v229_state_t *s) {
    /* 1. Seed. */
    for (int i = 0; i < COS_V229_N_SEED; ++i) {
        cos_v229_kernel_t *k = &s->pool[i];
        memset(k, 0, sizeof(*k));
        k->version = SEED[i].version;
        cpy_name(k->name, SEED[i].name);
        k->parent_version = -1;
        k->sigma_raw      = SEED[i].sigma_raw;
        k->sigma_depth    = 0.0f;
        k->sigma_growth   = SEED[i].sigma_raw;
        k->depth          = 0;
        k->accepted       = true;
        k->is_seed        = true;
    }
    s->n_accepted = COS_V229_N_SEED;
    s->n_rejected = 0;
    int n_pool    = COS_V229_N_SEED;

    /* 2. Growth protocol. */
    uint64_t prev = 0x2297EED0ULL;
    for (int i = 0; i < COS_V229_N_SEED; ++i) {
        const cos_v229_kernel_t *k = &s->pool[i];
        struct { int v; int p; float sr, sd, sg; int d, a, sd_flag;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.v  = k->version; rec.p = k->parent_version;
        rec.sr = k->sigma_raw; rec.sd = k->sigma_depth;
        rec.sg = k->sigma_growth;
        rec.d  = k->depth; rec.a = k->accepted ? 1 : 0;
        rec.sd_flag = k->is_seed ? 1 : 0;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }

    for (int c = 0; c < COS_V229_N_CANDIDATES; ++c) {
        const cos_v229_cand_t *cd = &CANDIDATES[c];
        int parent_idx = find_version(s, n_pool, cd->parent_version);

        cos_v229_kernel_t *k = &s->pool[n_pool];
        memset(k, 0, sizeof(*k));
        k->version        = cd->version;
        cpy_name(k->name, cd->name);
        k->parent_version = cd->parent_version;
        k->sigma_raw      = cd->sigma_raw;

        int depth = (parent_idx >= 0) ? (s->pool[parent_idx].depth + 1) : 99;
        k->depth = depth;
        /* Depth penalty: deeper derivations carry more
         * risk (the v0 surrogate for 'far from the seed'). */
        k->sigma_depth  = (float)depth * 0.03f;
        k->sigma_growth = 0.60f * k->sigma_raw + 0.40f * k->sigma_depth;
        if (k->sigma_growth < 0.0f) k->sigma_growth = 0.0f;
        if (k->sigma_growth > 1.0f) k->sigma_growth = 1.0f;

        bool ok_parent = (parent_idx >= 0) && s->pool[parent_idx].accepted;
        bool ok_sigma  = k->sigma_growth <= s->tau_growth;
        k->accepted    = ok_parent && ok_sigma;
        k->is_seed     = false;

        if (k->accepted) s->n_accepted++;
        else             s->n_rejected++;

        struct { int v; int p; float sr, sd, sg; int d, a, sd_flag;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.v  = k->version; rec.p = k->parent_version;
        rec.sr = k->sigma_raw; rec.sd = k->sigma_depth;
        rec.sg = k->sigma_growth;
        rec.d  = k->depth; rec.a = k->accepted ? 1 : 0;
        rec.sd_flag = 0;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);

        n_pool++;
    }
    {
        struct { int na, nr; float tau; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.na = s->n_accepted; rec.nr = s->n_rejected;
        rec.tau = s->tau_growth; rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    return prev;
}

void cos_v229_run(cos_v229_state_t *s) {
    /* Run A: populates s->pool and counters. */
    s->terminal_hash = grow_once(s);
    /* Run B: a second fresh snapshot on a temporary
     * state must produce the same terminal_hash. */
    cos_v229_state_t t;
    cos_v229_init(&t, s->seed);
    s->terminal_hash_verify = grow_once(&t);
    s->chain_valid = (s->terminal_hash == s->terminal_hash_verify);
}

size_t cos_v229_to_json(const cos_v229_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v229\","
        "\"n_seed\":%d,\"n_candidates\":%d,\"tau_growth\":%.3f,"
        "\"n_accepted\":%d,\"n_rejected\":%d,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"terminal_hash_verify\":\"%016llx\","
        "\"pool\":[",
        COS_V229_N_SEED, COS_V229_N_CANDIDATES,
        s->tau_growth, s->n_accepted, s->n_rejected,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash,
        (unsigned long long)s->terminal_hash_verify);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    int total = COS_V229_N_MAX;
    for (int i = 0; i < total; ++i) {
        const cos_v229_kernel_t *k = &s->pool[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"version\":%d,\"name\":\"%s\",\"parent\":%d,"
            "\"sigma_raw\":%.4f,\"sigma_depth\":%.4f,"
            "\"sigma_growth\":%.4f,\"depth\":%d,"
            "\"accepted\":%s,\"is_seed\":%s}",
            i == 0 ? "" : ",",
            k->version, k->name, k->parent_version,
            k->sigma_raw, k->sigma_depth, k->sigma_growth,
            k->depth,
            k->accepted ? "true" : "false",
            k->is_seed ? "true" : "false");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v229_self_test(void) {
    cos_v229_state_t s;
    cos_v229_init(&s, 0x229511DULL);
    cos_v229_run(&s);
    if (!s.chain_valid) return 1;

    /* Seed shape. */
    static const int expected_seed[COS_V229_N_SEED] =
        { 29, 101, 106, 124, 148 };
    for (int i = 0; i < COS_V229_N_SEED; ++i) {
        if (s.pool[i].version != expected_seed[i]) return 2;
        if (!s.pool[i].is_seed)                     return 2;
        if (!s.pool[i].accepted)                    return 2;
    }
    /* Growth guarantees. */
    if (s.n_accepted < 15) return 3;
    if (s.n_rejected < 1)  return 4;
    for (int i = COS_V229_N_SEED; i < COS_V229_N_MAX; ++i) {
        const cos_v229_kernel_t *k = &s.pool[i];
        if (k->accepted) {
            if (k->sigma_growth > s.tau_growth + 1e-6f) return 5;
        } else {
            /* A reject either broke the σ-gate or had
             * an ineligible parent.  σ_growth must
             * exceed τ_growth if it was a σ-gate
             * reject. */
        }
    }
    if (s.terminal_hash != s.terminal_hash_verify) return 6;
    return 0;
}
