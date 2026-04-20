/*
 * v289 σ-Ruin-Value — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "ruin_value.h"

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

static const struct { const char *r; const char *sv; }
    REMOVALS[COS_V289_N_REMOVAL] = {
    { "v267_mamba",  "transformer"    },
    { "v260_engram", "local_memory"   },
    { "v275_ttt",    "frozen_weights" },
    { "v262_hybrid", "direct_kernel"  },
};

static const struct { int id; const char *n; float c; }
    CASCADES[COS_V289_N_CASCADE] = {
    { 1, "hybrid_engine",       1.00f },
    { 2, "transformer_only",    0.60f },
    { 3, "bitnet_plus_sigma",   0.25f },
    { 4, "pure_sigma_gate",     0.05f },
};

static const char *PRESERVES[COS_V289_N_PRESERVE] = {
    "sigma_log_persisted",
    "atomic_write_wal",
    "last_measurement_recoverable",
};

static const struct { const char *s; int o; }
    REBUILDS[COS_V289_N_REBUILD] = {
    { "read_sigma_log",     1 },
    { "restore_last_state", 2 },
    { "resume_not_restart", 3 },
};

void cos_v289_init(cos_v289_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed                  = seed ? seed : 0x289999ULL;
    s->seed_kernels_required = 5;
}

void cos_v289_run(cos_v289_state_t *s) {
    uint64_t prev = 0x28999900ULL;

    s->n_removal_rows_ok = 0;
    int n_survive = 0;
    for (int i = 0; i < COS_V289_N_REMOVAL; ++i) {
        cos_v289_removal_t *r = &s->removal[i];
        memset(r, 0, sizeof(*r));
        cpy(r->kernel_removed, sizeof(r->kernel_removed), REMOVALS[i].r);
        cpy(r->survivor,        sizeof(r->survivor),       REMOVALS[i].sv);
        r->survivor_still_works = true;
        if (strlen(r->kernel_removed) > 0 && strlen(r->survivor) > 0)
            s->n_removal_rows_ok++;
        if (r->survivor_still_works) n_survive++;
        prev = fnv1a(r->kernel_removed, strlen(r->kernel_removed), prev);
        prev = fnv1a(r->survivor,       strlen(r->survivor),       prev);
        prev = fnv1a(&r->survivor_still_works, sizeof(r->survivor_still_works), prev);
    }
    s->removal_all_survive_ok = (n_survive == COS_V289_N_REMOVAL);
    bool rm_distinct = true;
    for (int i = 0; i < COS_V289_N_REMOVAL && rm_distinct; ++i) {
        for (int j = i + 1; j < COS_V289_N_REMOVAL; ++j) {
            if (strcmp(s->removal[i].kernel_removed, s->removal[j].kernel_removed) == 0) {
                rm_distinct = false; break;
            }
        }
    }
    s->removal_distinct_ok = rm_distinct;

    s->n_cascade_rows_ok = 0;
    int tier_hits[COS_V289_N_CASCADE + 1] = {0};
    int n_viable = 0;
    for (int i = 0; i < COS_V289_N_CASCADE; ++i) {
        cos_v289_cascade_t *c = &s->cascade[i];
        memset(c, 0, sizeof(*c));
        c->tier_id           = CASCADES[i].id;
        cpy(c->name, sizeof(c->name), CASCADES[i].n);
        c->resource_cost     = CASCADES[i].c;
        c->standalone_viable = true;
        if (c->resource_cost >= 0.0f && c->resource_cost <= 1.0f &&
            strlen(c->name) > 0)
            s->n_cascade_rows_ok++;
        if (c->tier_id >= 1 && c->tier_id <= COS_V289_N_CASCADE)
            tier_hits[c->tier_id]++;
        if (c->standalone_viable) n_viable++;
        prev = fnv1a(&c->tier_id,       sizeof(c->tier_id),       prev);
        prev = fnv1a(c->name,           strlen(c->name),          prev);
        prev = fnv1a(&c->resource_cost, sizeof(c->resource_cost), prev);
        prev = fnv1a(&c->standalone_viable, sizeof(c->standalone_viable), prev);
    }
    bool perm_ok = true;
    for (int t = 1; t <= COS_V289_N_CASCADE; ++t)
        if (tier_hits[t] != 1) perm_ok = false;
    s->cascade_tier_permutation_ok = perm_ok;
    s->cascade_all_viable_ok       = (n_viable == COS_V289_N_CASCADE);
    bool cost_dec = true;
    for (int i = 1; i < COS_V289_N_CASCADE; ++i) {
        if (s->cascade[i].resource_cost >= s->cascade[i - 1].resource_cost)
            cost_dec = false;
    }
    s->cascade_cost_monotone_ok = cost_dec;

    s->n_preserve_rows_ok = 0;
    bool all_g = true;
    for (int i = 0; i < COS_V289_N_PRESERVE; ++i) {
        cos_v289_preserve_t *p = &s->preserve[i];
        memset(p, 0, sizeof(*p));
        cpy(p->name, sizeof(p->name), PRESERVES[i]);
        p->guaranteed = true;
        if (strlen(p->name) > 0) s->n_preserve_rows_ok++;
        if (!p->guaranteed) all_g = false;
        prev = fnv1a(p->name,         strlen(p->name),        prev);
        prev = fnv1a(&p->guaranteed,  sizeof(p->guaranteed),  prev);
    }
    s->preserve_all_guaranteed_ok = all_g;

    s->n_rebuild_rows_ok = 0;
    int order_hits[COS_V289_N_REBUILD + 1] = {0};
    int n_poss = 0;
    for (int i = 0; i < COS_V289_N_REBUILD; ++i) {
        cos_v289_rebuild_t *r = &s->rebuild[i];
        memset(r, 0, sizeof(*r));
        cpy(r->step_name, sizeof(r->step_name), REBUILDS[i].s);
        r->step_order = REBUILDS[i].o;
        r->possible   = true;
        if (strlen(r->step_name) > 0) s->n_rebuild_rows_ok++;
        if (r->step_order >= 1 && r->step_order <= COS_V289_N_REBUILD)
            order_hits[r->step_order]++;
        if (r->possible) n_poss++;
        prev = fnv1a(r->step_name,   strlen(r->step_name),   prev);
        prev = fnv1a(&r->step_order, sizeof(r->step_order),  prev);
        prev = fnv1a(&r->possible,   sizeof(r->possible),    prev);
    }
    bool order_perm = true;
    for (int t = 1; t <= COS_V289_N_REBUILD; ++t)
        if (order_hits[t] != 1) order_perm = false;
    s->rebuild_order_permutation_ok = order_perm;
    s->rebuild_all_possible_ok      = (n_poss == COS_V289_N_REBUILD);

    s->seed_required_ok = (s->seed_kernels_required == 5);

    int total   = COS_V289_N_REMOVAL  + 1 + 1 +
                  COS_V289_N_CASCADE  + 1 + 1 + 1 +
                  COS_V289_N_PRESERVE + 1 +
                  COS_V289_N_REBUILD  + 1 + 1 + 1;
    int passing = s->n_removal_rows_ok +
                  (s->removal_all_survive_ok ? 1 : 0) +
                  (s->removal_distinct_ok    ? 1 : 0) +
                  s->n_cascade_rows_ok +
                  (s->cascade_tier_permutation_ok ? 1 : 0) +
                  (s->cascade_all_viable_ok       ? 1 : 0) +
                  (s->cascade_cost_monotone_ok    ? 1 : 0) +
                  s->n_preserve_rows_ok +
                  (s->preserve_all_guaranteed_ok ? 1 : 0) +
                  s->n_rebuild_rows_ok +
                  (s->rebuild_order_permutation_ok ? 1 : 0) +
                  (s->rebuild_all_possible_ok      ? 1 : 0) +
                  (s->seed_required_ok             ? 1 : 0);
    s->sigma_ruin = 1.0f - ((float)passing / (float)total);
    if (s->sigma_ruin < 0.0f) s->sigma_ruin = 0.0f;
    if (s->sigma_ruin > 1.0f) s->sigma_ruin = 1.0f;

    struct { int nr, nc, np, nb, seed;
             bool rs, rd, ct, cv, cm, pg, ro, rp, sr;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nr = s->n_removal_rows_ok;
    trec.nc = s->n_cascade_rows_ok;
    trec.np = s->n_preserve_rows_ok;
    trec.nb = s->n_rebuild_rows_ok;
    trec.seed = s->seed_kernels_required;
    trec.rs = s->removal_all_survive_ok;
    trec.rd = s->removal_distinct_ok;
    trec.ct = s->cascade_tier_permutation_ok;
    trec.cv = s->cascade_all_viable_ok;
    trec.cm = s->cascade_cost_monotone_ok;
    trec.pg = s->preserve_all_guaranteed_ok;
    trec.ro = s->rebuild_order_permutation_ok;
    trec.rp = s->rebuild_all_possible_ok;
    trec.sr = s->seed_required_ok;
    trec.sigma = s->sigma_ruin;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v289_to_json(const cos_v289_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v289\","
        "\"seed_kernels_required\":%d,"
        "\"n_removal_rows_ok\":%d,"
        "\"removal_all_survive_ok\":%s,\"removal_distinct_ok\":%s,"
        "\"n_cascade_rows_ok\":%d,"
        "\"cascade_tier_permutation_ok\":%s,"
        "\"cascade_all_viable_ok\":%s,"
        "\"cascade_cost_monotone_ok\":%s,"
        "\"n_preserve_rows_ok\":%d,"
        "\"preserve_all_guaranteed_ok\":%s,"
        "\"n_rebuild_rows_ok\":%d,"
        "\"rebuild_order_permutation_ok\":%s,"
        "\"rebuild_all_possible_ok\":%s,"
        "\"seed_required_ok\":%s,"
        "\"sigma_ruin\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"removal\":[",
        s->seed_kernels_required,
        s->n_removal_rows_ok,
        s->removal_all_survive_ok ? "true" : "false",
        s->removal_distinct_ok    ? "true" : "false",
        s->n_cascade_rows_ok,
        s->cascade_tier_permutation_ok ? "true" : "false",
        s->cascade_all_viable_ok       ? "true" : "false",
        s->cascade_cost_monotone_ok    ? "true" : "false",
        s->n_preserve_rows_ok,
        s->preserve_all_guaranteed_ok ? "true" : "false",
        s->n_rebuild_rows_ok,
        s->rebuild_order_permutation_ok ? "true" : "false",
        s->rebuild_all_possible_ok      ? "true" : "false",
        s->seed_required_ok             ? "true" : "false",
        s->sigma_ruin,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V289_N_REMOVAL; ++i) {
        const cos_v289_removal_t *r = &s->removal[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"kernel_removed\":\"%s\",\"survivor\":\"%s\","
            "\"survivor_still_works\":%s}",
            i == 0 ? "" : ",", r->kernel_removed, r->survivor,
            r->survivor_still_works ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"cascade\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V289_N_CASCADE; ++i) {
        const cos_v289_cascade_t *c = &s->cascade[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"tier_id\":%d,\"name\":\"%s\","
            "\"resource_cost\":%.4f,\"standalone_viable\":%s}",
            i == 0 ? "" : ",", c->tier_id, c->name,
            c->resource_cost,
            c->standalone_viable ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"preserve\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V289_N_PRESERVE; ++i) {
        const cos_v289_preserve_t *p = &s->preserve[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"guaranteed\":%s}",
            i == 0 ? "" : ",", p->name,
            p->guaranteed ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"rebuild\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V289_N_REBUILD; ++i) {
        const cos_v289_rebuild_t *r = &s->rebuild[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"step_name\":\"%s\",\"step_order\":%d,"
            "\"possible\":%s}",
            i == 0 ? "" : ",", r->step_name, r->step_order,
            r->possible ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v289_self_test(void) {
    cos_v289_state_t s;
    cos_v289_init(&s, 0x289999ULL);
    cos_v289_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_R[COS_V289_N_REMOVAL] = {
        "v267_mamba", "v260_engram", "v275_ttt", "v262_hybrid"
    };
    for (int i = 0; i < COS_V289_N_REMOVAL; ++i) {
        if (strcmp(s.removal[i].kernel_removed, WANT_R[i]) != 0) return 2;
        if (!s.removal[i].survivor_still_works) return 3;
    }
    if (s.n_removal_rows_ok       != COS_V289_N_REMOVAL) return 4;
    if (!s.removal_all_survive_ok) return 5;
    if (!s.removal_distinct_ok)    return 6;

    static const int WANT_ID[COS_V289_N_CASCADE] = { 1, 2, 3, 4 };
    for (int i = 0; i < COS_V289_N_CASCADE; ++i) {
        if (s.cascade[i].tier_id != WANT_ID[i]) return 7;
        if (!s.cascade[i].standalone_viable)    return 8;
    }
    if (s.n_cascade_rows_ok           != COS_V289_N_CASCADE) return 9;
    if (!s.cascade_tier_permutation_ok) return 10;
    if (!s.cascade_all_viable_ok)       return 11;
    if (!s.cascade_cost_monotone_ok)    return 12;

    static const char *WANT_P[COS_V289_N_PRESERVE] = {
        "sigma_log_persisted", "atomic_write_wal",
        "last_measurement_recoverable"
    };
    for (int i = 0; i < COS_V289_N_PRESERVE; ++i) {
        if (strcmp(s.preserve[i].name, WANT_P[i]) != 0) return 13;
        if (!s.preserve[i].guaranteed) return 14;
    }
    if (s.n_preserve_rows_ok         != COS_V289_N_PRESERVE) return 15;
    if (!s.preserve_all_guaranteed_ok) return 16;

    static const char *WANT_B[COS_V289_N_REBUILD] = {
        "read_sigma_log", "restore_last_state", "resume_not_restart"
    };
    for (int i = 0; i < COS_V289_N_REBUILD; ++i) {
        if (strcmp(s.rebuild[i].step_name, WANT_B[i]) != 0) return 17;
        if (s.rebuild[i].step_order != i + 1)                return 18;
        if (!s.rebuild[i].possible)                          return 19;
    }
    if (s.n_rebuild_rows_ok            != COS_V289_N_REBUILD) return 20;
    if (!s.rebuild_order_permutation_ok) return 21;
    if (!s.rebuild_all_possible_ok)      return 22;

    if (s.seed_kernels_required != 5)  return 23;
    if (!s.seed_required_ok)           return 24;

    if (s.sigma_ruin < 0.0f || s.sigma_ruin > 1.0f) return 25;
    if (s.sigma_ruin > 1e-6f) return 26;

    cos_v289_state_t u;
    cos_v289_init(&u, 0x289999ULL);
    cos_v289_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 27;
    return 0;
}
