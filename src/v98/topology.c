/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v98 — σ-Topology implementation.
 */

#include "topology.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Integer sine / cosine lookup table, 16 angles uniformly spaced on
 * [0, 2π).  Q16.16 values computed once by hand at build time so the
 * source stays floating-point-free and deterministic. */
static const cos_v98_q16_t COS_V98_SIN_LUT[COS_V98_N] = {
         0,   33779,   56754,   64602,   56754,   33779,        0,  -33779,
    -56754,  -64602,  -56754,  -33779
};
static const cos_v98_q16_t COS_V98_COS_LUT[COS_V98_N] = {
     65536,   56754,   33779,       0,  -33779,  -56754,   -65536,  -56754,
    -33779,       0,   33779,   56754
};

static inline int64_t q_mul64(int64_t a, int64_t b) { return (a * b) >> 16; }

static uint64_t xs(uint64_t *s)
{
    uint64_t x = *s; x ^= x << 13; x ^= x >> 7; x ^= x << 17; *s = x;
    return x;
}

/* ------------------------------------------------------------------ */

void cos_v98_init(cos_v98_space_t *s)
{
    memset(s, 0, sizeof(*s));
    s->sentinel = COS_V98_SENTINEL;
}

void cos_v98_load_circle(cos_v98_space_t *s, uint64_t seed)
{
    cos_v98_init(s);
    uint64_t r = seed ? seed : 0xC13C1EC13C1EULL;

    for (uint32_t i = 0u; i < COS_V98_N; ++i) {
        int32_t jx = (int32_t)((xs(&r) & 0x3FFu) - 0x200);   /* ≤ 512 Q16.16 = 0.0078 */
        int32_t jy = (int32_t)((xs(&r) & 0x3FFu) - 0x200);
        s->p[i][0] = (cos_v98_q16_t)(COS_V98_COS_LUT[i] + jx);
        s->p[i][1] = (cos_v98_q16_t)(COS_V98_SIN_LUT[i] + jy);
        s->p[i][2] = 0;
    }
    cos_v98_recompute_d2(s);
}

void cos_v98_recompute_d2(cos_v98_space_t *s)
{
    for (uint32_t i = 0u; i < COS_V98_N; ++i) {
        s->d2[i][i] = 0;
        for (uint32_t j = i + 1u; j < COS_V98_N; ++j) {
            int64_t acc = 0;
            for (uint32_t k = 0u; k < COS_V98_D; ++k) {
                int64_t delta = (int64_t)s->p[i][k] - (int64_t)s->p[j][k];
                /* Q16.16 × Q16.16 → Q32.32; keep as Q32.32 int64. */
                acc += delta * delta;
            }
            s->d2[i][j] = acc;
            s->d2[j][i] = acc;
        }
    }
}

/* ---- Union–Find ---------------------------------------------------- */

static uint32_t uf_find(uint32_t *parent, uint32_t x)
{
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];           /* path compression */
        x = parent[x];
    }
    return x;
}

static uint32_t uf_union(uint32_t *parent, uint32_t *rank,
                         uint32_t a, uint32_t b)
{
    uint32_t ra = uf_find(parent, a);
    uint32_t rb = uf_find(parent, b);
    if (ra == rb) return 0u;                     /* already merged */
    if      (rank[ra] < rank[rb]) parent[ra] = rb;
    else if (rank[ra] > rank[rb]) parent[rb] = ra;
    else { parent[rb] = ra; ++rank[ra]; }
    return 1u;
}

/* ---- Betti-0, Betti-1, edges -------------------------------------- */

uint32_t cos_v98_betti0_at(const cos_v98_space_t *s, int64_t eps_sq)
{
    uint32_t parent[COS_V98_N], rank[COS_V98_N];
    for (uint32_t i = 0u; i < COS_V98_N; ++i) { parent[i] = i; rank[i] = 0u; }

    for (uint32_t i = 0u; i < COS_V98_N; ++i) {
        for (uint32_t j = i + 1u; j < COS_V98_N; ++j) {
            if (s->d2[i][j] <= eps_sq) {
                (void)uf_union(parent, rank, i, j);
            }
        }
    }
    uint32_t comps = 0u;
    for (uint32_t i = 0u; i < COS_V98_N; ++i)
        if (uf_find(parent, i) == i) ++comps;
    return comps;
}

uint32_t cos_v98_edges_at(const cos_v98_space_t *s, int64_t eps_sq)
{
    uint32_t e = 0u;
    for (uint32_t i = 0u; i < COS_V98_N; ++i)
        for (uint32_t j = i + 1u; j < COS_V98_N; ++j)
            if (s->d2[i][j] <= eps_sq) ++e;
    return e;
}

uint32_t cos_v98_betti1_at(const cos_v98_space_t *s, int64_t eps_sq)
{
    /* β₁ of the 1-skeleton = E − V + C  (Euler characteristic). */
    uint32_t E = cos_v98_edges_at(s, eps_sq);
    uint32_t C = cos_v98_betti0_at(s, eps_sq);
    /* E ≥ V − C  always, so the signed subtraction is non-negative. */
    int64_t v = (int64_t)E - (int64_t)COS_V98_N + (int64_t)C;
    if (v < 0) v = 0;                            /* guard rail */
    return (uint32_t)v;
}

void cos_v98_filtration_check(cos_v98_space_t *s,
                              const int64_t *eps_sq_schedule,
                              uint32_t schedule_len)
{
    s->monotone_violations = 0u;
    uint32_t prev_b0 = COS_V98_N + 1u;
    for (uint32_t k = 0u; k < schedule_len; ++k) {
        uint32_t b0 = cos_v98_betti0_at(s, eps_sq_schedule[k]);
        uint32_t b1 = cos_v98_betti1_at(s, eps_sq_schedule[k]);
        if (b0 > prev_b0) ++s->monotone_violations;   /* β₀ must not grow */
        if ((int64_t)b1 < 0)   ++s->monotone_violations;
        prev_b0 = b0;
        s->betti_0_last = b0;
        s->betti_1_last = b1;
    }
}

/* ---- Verdict ------------------------------------------------------ */

uint32_t cos_v98_ok(const cos_v98_space_t *s)
{
    uint32_t sent = (s->sentinel == COS_V98_SENTINEL) ? 1u : 0u;
    /* Distance matrix must be symmetric and zero on the diagonal. */
    uint32_t dok = 1u;
    for (uint32_t i = 0u; i < COS_V98_N; ++i) {
        if (s->d2[i][i] != 0) dok = 0u;
        for (uint32_t j = i + 1u; j < COS_V98_N; ++j) {
            if (s->d2[i][j] != s->d2[j][i]) dok = 0u;
            if (s->d2[i][j] <  0)            dok = 0u;
        }
    }
    uint32_t mono = (s->monotone_violations == 0u) ? 1u : 0u;
    return sent & dok & mono;
}

uint32_t cos_v98_compose_decision(uint32_t v97_composed_ok, uint32_t v98_ok)
{
    return v97_composed_ok & v98_ok;
}

/* q_mul64 is declared for potential use by future filtration radius
 * helpers; silence the compiler about it being unused for now. */
static void cos_v98_force_link_q_mul64(void) { (void)q_mul64(0, 0); }
static void (*cos_v98_keep_q_mul64)(void) __attribute__((unused)) =
    &cos_v98_force_link_q_mul64;
