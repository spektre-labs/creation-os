/*
 * v232 σ-Lineage — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "lineage.h"

#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static int popcount64(uint64_t x) {
    int c = 0;
    while (x) { c += (int)(x & 1ULL); x >>= 1; }
    return c;
}

typedef struct {
    int       parent;            /* -1 for root */
    int       gen;
    uint64_t  edge_xor_mask;     /* applied to parent skills */
    int       uptime_steps;
} cos_v232_fixture_t;

/* Six-node tree; edge masks tuned so three nodes
 * merge cleanly (popcount ≤ 0.40·64 = 25) and two
 * are blocked (popcount ≥ 30). */
static const cos_v232_fixture_t FIX[COS_V232_N_NODES] = {
    { -1, 0, 0x0000000000000000ULL, 1000 },  /* 0 root */
    {  0, 1, 0x00000000000000F0ULL,  800 },  /* 1 low div */
    {  0, 1, 0x00000000FFFFFFFFULL,  700 },  /* 2 high div (32b) */
    {  1, 2, 0x0000000000000F00ULL,  400 },  /* 3 low div */
    {  1, 2, 0x000000003FFFFFFFULL,  350 },  /* 4 high div (30b) */
    {  2, 2, 0x0000000000F00000ULL,  300 },  /* 5 low div */
};

static const uint64_t ROOT_SKILLS = 0xA5A5A5A5A5A5A5A5ULL;

void cos_v232_init(cos_v232_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed      = seed ? seed : 0x232AFCED5ULL;
    s->tau_merge = 0.40f;
}

void cos_v232_run(cos_v232_state_t *s) {
    uint64_t prev = 0x232B100DULL;
    s->n_mergeable = s->n_blocked = 0;

    for (int i = 0; i < COS_V232_N_NODES; ++i) {
        cos_v232_node_t *n = &s->nodes[i];
        memset(n, 0, sizeof(*n));
        n->id         = i;
        n->parent_idx = FIX[i].parent;
        n->gen        = FIX[i].gen;
        n->uptime_steps = FIX[i].uptime_steps;

        if (n->parent_idx < 0) {
            n->skills = ROOT_SKILLS;
            n->sigma_divergence_from_parent = 0.0f;
        } else {
            const cos_v232_node_t *p = &s->nodes[n->parent_idx];
            n->skills = p->skills ^ FIX[i].edge_xor_mask;
            int d = popcount64(FIX[i].edge_xor_mask);
            n->sigma_divergence_from_parent =
                (float)d / (float)COS_V232_N_BITS;
        }

        n->n_skills     = popcount64(n->skills);
        n->sigma_profile = (float)n->n_skills
                            / (float)COS_V232_N_BITS;

        /* Ancestor path. */
        int depth = n->gen;
        n->ancestor_depth = depth;
        int cur = i;
        for (int k = depth; k >= 0; --k) {
            n->ancestor_path[k] = cur;
            if (s->nodes[cur].parent_idx >= 0)
                cur = s->nodes[cur].parent_idx;
        }

        /* Merge-back verdict. */
        if (n->parent_idx < 0) {
            n->sigma_merge = 0.0f;
            n->mergeable   = false;     /* root has nothing to merge */
        } else {
            n->sigma_merge = n->sigma_divergence_from_parent;
            n->mergeable   = n->sigma_merge <= s->tau_merge;
            if (n->mergeable) s->n_mergeable++;
            else              s->n_blocked++;
        }

        struct { int id, p, g; uint64_t sk; int ns; float sp, sd, sm;
                 int up, merge; int path[COS_V232_MAX_GEN + 1];
                 int ad; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id = n->id; rec.p = n->parent_idx; rec.g = n->gen;
        rec.sk = n->skills; rec.ns = n->n_skills;
        rec.sp = n->sigma_profile;
        rec.sd = n->sigma_divergence_from_parent;
        rec.sm = n->sigma_merge;
        rec.up = n->uptime_steps;
        rec.merge = n->mergeable ? 1 : 0;
        for (int k = 0; k <= COS_V232_MAX_GEN; ++k)
            rec.path[k] = n->ancestor_path[k];
        rec.ad = n->ancestor_depth;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    {
        struct { int ng, nb; float tau; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.ng = s->n_mergeable;
        rec.nb = s->n_blocked;
        rec.tau = s->tau_merge;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v232_to_json(const cos_v232_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v232\","
        "\"n_nodes\":%d,\"max_gen\":%d,\"tau_merge\":%.3f,"
        "\"n_mergeable\":%d,\"n_blocked\":%d,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"nodes\":[",
        COS_V232_N_NODES, COS_V232_MAX_GEN,
        s->tau_merge, s->n_mergeable, s->n_blocked,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;

    for (int i = 0; i < COS_V232_N_NODES; ++i) {
        const cos_v232_node_t *nd = &s->nodes[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"parent\":%d,\"gen\":%d,"
            "\"skills\":\"%016llx\",\"n_skills\":%d,"
            "\"sigma_profile\":%.4f,"
            "\"sigma_divergence\":%.4f,"
            "\"sigma_merge\":%.4f,\"uptime_steps\":%d,"
            "\"mergeable\":%s,\"ancestor_depth\":%d,"
            "\"ancestor_path\":[",
            i == 0 ? "" : ",",
            nd->id, nd->parent_idx, nd->gen,
            (unsigned long long)nd->skills, nd->n_skills,
            nd->sigma_profile,
            nd->sigma_divergence_from_parent,
            nd->sigma_merge, nd->uptime_steps,
            nd->mergeable ? "true" : "false",
            nd->ancestor_depth);
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
        for (int k = 0; k <= nd->ancestor_depth; ++k) {
            int p = snprintf(buf + off, cap - off,
                "%s%d", k == 0 ? "" : ",",
                nd->ancestor_path[k]);
            if (p < 0 || off + (size_t)p >= cap) return 0;
            off += (size_t)p;
        }
        int r = snprintf(buf + off, cap - off, "]}");
        if (r < 0 || off + (size_t)r >= cap) return 0;
        off += (size_t)r;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v232_self_test(void) {
    cos_v232_state_t s;
    cos_v232_init(&s, 0x232AFCED5ULL);
    cos_v232_run(&s);
    if (!s.chain_valid) return 1;

    int n_root = 0, max_g = 0;
    for (int i = 0; i < COS_V232_N_NODES; ++i) {
        const cos_v232_node_t *n = &s.nodes[i];
        if (n->parent_idx < 0) n_root++;
        else {
            if (n->parent_idx >= i)                 return 2;
            if (s.nodes[n->parent_idx].gen >= n->gen) return 3;
        }
        if (n->gen > max_g) max_g = n->gen;
        if (n->sigma_divergence_from_parent < 0.0f ||
            n->sigma_divergence_from_parent > 1.0f) return 4;
        /* Ancestor path end hits a root. */
        int top = n->ancestor_path[0];
        if (s.nodes[top].parent_idx != -1)          return 5;
        if (n->ancestor_path[n->ancestor_depth] != n->id) return 6;
    }
    if (n_root != 1)             return 7;
    if (max_g  != COS_V232_MAX_GEN) return 8;
    if (s.n_mergeable < 1)       return 9;
    if (s.n_blocked   < 1)       return 10;
    if (s.n_mergeable + s.n_blocked != COS_V232_N_NODES - 1) return 11;
    return 0;
}
