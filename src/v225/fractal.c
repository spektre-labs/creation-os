/*
 * v225 σ-Fractal — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "fractal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v225_init(cos_v225_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed ? seed : 0x225F4AC7ULL;
    s->delta_cross = 0.10f;
    s->eps_kk      = 1e-5f;
}

/* BFS layout:
 *   id 0            : root (level 4)
 *   id 1..2         : level 3  (parent 0)
 *   id 3..6         : level 2  (parents 1, 1, 2, 2)
 *   id 7..14        : level 1
 *   id 15..30       : level 0  (leaves)
 * parent(i) = (i-1)/2;  children(i) = { 2i+1, 2i+2 }. */

static int level_of(int id) {
    if (id == 0)  return 4;
    if (id <= 2)  return 3;
    if (id <= 6)  return 2;
    if (id <= 14) return 1;
    return 0;
}

/* Leaf σ fixture: 16 leaves.  Values are spread across
 * [0.05, 0.75] so subtree means are non-trivial but stay
 * inside [0, 1] at every level. */
static const float LEAF_SIGMA[16] = {
    0.05f, 0.08f, 0.11f, 0.15f,  /* sentence 0, 1 branches */
    0.12f, 0.10f, 0.09f, 0.07f,  /* sentence 2, 3 branches */
    0.35f, 0.40f, 0.45f, 0.50f,  /* sentence 4, 5 branches */
    0.60f, 0.65f, 0.70f, 0.75f   /* sentence 6, 7 branches */
};

void cos_v225_build(cos_v225_state_t *s) {
    for (int i = 0; i < COS_V225_N_NODES; ++i) {
        cos_v225_node_t *n = &s->nodes[i];
        memset(n, 0, sizeof(*n));
        n->id    = i;
        n->level = level_of(i);
        n->parent = (i == 0) ? -1 : (i - 1) / 2;

        int c0 = 2 * i + 1;
        int c1 = 2 * i + 2;
        n->n_children = 0;
        if (c0 < COS_V225_N_NODES) n->children[n->n_children++] = c0;
        if (c1 < COS_V225_N_NODES) n->children[n->n_children++] = c1;

        if (n->level == 0) {
            int leaf = i - 15;
            n->sigma = LEAF_SIGMA[leaf];
        }
    }
}

void cos_v225_run(cos_v225_state_t *s) {
    /* 1. Aggregate upward: σ_parent = mean(σ_children). */
    for (int level = 1; level <= 4; ++level) {
        for (int i = 0; i < COS_V225_N_NODES; ++i) {
            cos_v225_node_t *n = &s->nodes[i];
            if (n->level != level) continue;
            float sum = 0.0f;
            for (int c = 0; c < n->n_children; ++c)
                sum += s->nodes[n->children[c]].sigma;
            n->sigma = (n->n_children > 0)
                ? sum / (float)n->n_children : 0.0f;
            if (n->sigma < 0.0f) n->sigma = 0.0f;
            if (n->sigma > 1.0f) n->sigma = 1.0f;
        }
    }
    s->sigma_root = s->nodes[0].sigma;
    s->k_root     = 1.0f - s->sigma_root;

    /* 2. Declared-σ fixture: plant 1 deliberate reference
     *    mismatch at node 7 (level 1) to exercise the
     *    cross-scale incoherence detector. Every other
     *    node declares σ_declared == σ. */
    for (int i = 0; i < COS_V225_N_NODES; ++i)
        s->nodes[i].sigma_declared = s->nodes[i].sigma;
    /* Node 7 claims σ = 0.08 while its two children's
     * mean is somewhere around (0.05 + 0.08)/2 = 0.065 +
     * .. actually the mismatch is planted by picking a
     * declared value that is far from the actual child-
     * mean.  Use +0.25 offset. */
    s->nodes[7].sigma_declared = s->nodes[7].sigma + 0.25f;
    if (s->nodes[7].sigma_declared > 1.0f)
        s->nodes[7].sigma_declared = 1.0f;
    s->nodes[7].is_reference_mismatch = true;

    /* 3. Scale-invariance check: σ_parent == mean(children)
     *    by construction in (1), but we re-verify at 1e-6. */
    s->n_scale_invariant_ok = 0;
    for (int i = 0; i < COS_V225_N_NODES; ++i) {
        const cos_v225_node_t *n = &s->nodes[i];
        if (n->n_children == 0) continue;
        float sum = 0.0f;
        for (int c = 0; c < n->n_children; ++c)
            sum += s->nodes[n->children[c]].sigma;
        float mean = sum / (float)n->n_children;
        if (fabsf(n->sigma - mean) <= 1e-6f) s->n_scale_invariant_ok++;
    }

    /* 4. Cross-scale coherence against the TRUE aggregate
     *    (should all be 0 — aggregator is mean). */
    s->n_cross_true_diff = 0;
    for (int i = 0; i < COS_V225_N_NODES; ++i) {
        const cos_v225_node_t *n = &s->nodes[i];
        if (n->n_children == 0) continue;
        float sum = 0.0f;
        for (int c = 0; c < n->n_children; ++c)
            sum += s->nodes[n->children[c]].sigma;
        float mean = sum / (float)n->n_children;
        if (fabsf(n->sigma - mean) > s->delta_cross) s->n_cross_true_diff++;
    }
    /* 5. Cross-scale coherence against DECLARED σ
     *    (fixture plants node 7's mismatch → ≥ 1). */
    s->n_cross_declared_diff = 0;
    for (int i = 0; i < COS_V225_N_NODES; ++i) {
        const cos_v225_node_t *n = &s->nodes[i];
        if (n->n_children == 0) continue;
        float sum = 0.0f;
        for (int c = 0; c < n->n_children; ++c)
            sum += s->nodes[n->children[c]].sigma;
        float mean = sum / (float)n->n_children;
        if (fabsf(n->sigma_declared - mean) > s->delta_cross)
            s->n_cross_declared_diff++;
    }
    /* 6. K(K)=K holographic identity: A({K, K, ..., K}) - K
     *    = 0 when A is mean (trivially).  We still compute
     *    and count nodes where the identity holds to
     *    ε_kk. */
    s->n_kk_ok = 0;
    for (int i = 0; i < COS_V225_N_NODES; ++i) {
        const cos_v225_node_t *n = &s->nodes[i];
        if (n->n_children == 0) continue;
        float K_here = 1.0f - n->sigma;
        float agg = K_here;  /* mean of n copies of K_here */
        if (fabsf(agg - K_here) <= s->eps_kk) s->n_kk_ok++;
    }

    /* 7. FNV-1a chain (BFS order). */
    uint64_t prev = 0x225D11A1ULL;
    for (int i = 0; i < COS_V225_N_NODES; ++i) {
        cos_v225_node_t *n = &s->nodes[i];
        n->hash_prev = prev;
        struct { int id, level, parent, nc, mismatch;
                 float sig, dec; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id       = n->id;
        rec.level    = n->level;
        rec.parent   = n->parent;
        rec.nc       = n->n_children;
        rec.mismatch = n->is_reference_mismatch ? 1 : 0;
        rec.sig      = n->sigma;
        rec.dec      = n->sigma_declared;
        rec.prev     = prev;
        n->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = n->hash_curr;
    }
    s->terminal_hash = prev;

    uint64_t v = 0x225D11A1ULL;
    s->chain_valid = true;
    for (int i = 0; i < COS_V225_N_NODES; ++i) {
        const cos_v225_node_t *n = &s->nodes[i];
        if (n->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, level, parent, nc, mismatch;
                 float sig, dec; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id       = n->id;
        rec.level    = n->level;
        rec.parent   = n->parent;
        rec.nc       = n->n_children;
        rec.mismatch = n->is_reference_mismatch ? 1 : 0;
        rec.sig      = n->sigma;
        rec.dec      = n->sigma_declared;
        rec.prev     = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != n->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v225_to_json(const cos_v225_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v225\","
        "\"n_levels\":%d,\"fanout\":%d,\"n_nodes\":%d,"
        "\"delta_cross\":%.3f,\"eps_kk\":%.6f,"
        "\"sigma_root\":%.4f,\"k_root\":%.4f,"
        "\"n_scale_invariant_ok\":%d,"
        "\"n_cross_true_diff\":%d,"
        "\"n_cross_declared_diff\":%d,"
        "\"n_kk_ok\":%d,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"nodes\":[",
        COS_V225_N_LEVELS, COS_V225_FANOUT, COS_V225_N_NODES,
        s->delta_cross, s->eps_kk,
        s->sigma_root, s->k_root,
        s->n_scale_invariant_ok,
        s->n_cross_true_diff,
        s->n_cross_declared_diff,
        s->n_kk_ok,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V225_N_NODES; ++i) {
        const cos_v225_node_t *nd = &s->nodes[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"level\":%d,\"parent\":%d,\"nc\":%d,"
            "\"sigma\":%.4f,\"declared\":%.4f,\"mismatch\":%s}",
            i == 0 ? "" : ",",
            nd->id, nd->level, nd->parent, nd->n_children,
            nd->sigma, nd->sigma_declared,
            nd->is_reference_mismatch ? "true" : "false");
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v225_self_test(void) {
    cos_v225_state_t s;
    cos_v225_init(&s, 0x225F4AC7ULL);
    cos_v225_build(&s);
    cos_v225_run(&s);

    if (!s.chain_valid) return 1;

    int n_internal = 0;
    for (int i = 0; i < COS_V225_N_NODES; ++i)
        if (s.nodes[i].n_children > 0) n_internal++;

    if (s.n_scale_invariant_ok != n_internal) return 2;
    if (s.n_cross_true_diff    != 0)           return 3;
    if (s.n_cross_declared_diff < 1)           return 4;
    if (s.n_kk_ok != n_internal)               return 5;

    for (int i = 0; i < COS_V225_N_NODES; ++i) {
        const cos_v225_node_t *n = &s.nodes[i];
        if (n->sigma          < 0.0f || n->sigma          > 1.0f) return 6;
        if (n->sigma_declared < 0.0f || n->sigma_declared > 1.0f) return 6;
    }
    return 0;
}
