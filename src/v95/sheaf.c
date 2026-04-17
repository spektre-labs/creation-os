/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v95 — σ-Sheaf implementation.
 */

#include "sheaf.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint64_t xs(uint64_t *s)
{
    uint64_t x = *s; x ^= x << 13; x ^= x >> 7; x ^= x << 17; *s = x;
    return x;
}

static inline cos_v95_q16_t q_abs(cos_v95_q16_t x)
{
    uint32_t ux = (uint32_t)x;
    uint32_t m  = (uint32_t)(x >> 31);
    return (cos_v95_q16_t)((ux ^ m) - m);
}

void cos_v95_sheaf_init(cos_v95_sheaf_t *s, uint64_t seed)
{
    memset(s, 0, sizeof(*s));
    s->sentinel = COS_V95_SENTINEL;

    /* Ring graph: edge i connects vertex i and (i+1) mod N. */
    for (uint32_t i = 0; i < COS_V95_EDGES; ++i) {
        s->e_u[i] = (uint8_t)i;
        s->e_v[i] = (uint8_t)((i + 1u) % COS_V95_NODES);
    }

    /* Signed-identity restriction maps: each diagonal entry ∈ {−1,+1}.
     * Random choice seeded from the parameter; resulting maps are
     * orthogonal so F^⊤ F = I. */
    uint64_t r = seed ? seed : 0x95C0FAA595C0FAA5ULL;
    for (uint32_t e = 0; e < COS_V95_EDGES; ++e) {
        for (uint32_t d = 0; d < COS_V95_DIM; ++d) {
            s->r_u[e][d] = (int8_t)(((xs(&r) & 1u) ? 1 : -1));
            s->r_v[e][d] = (int8_t)(((xs(&r) & 1u) ? 1 : -1));
        }
    }

    s->last_energy = INT64_MAX;
}

void cos_v95_set_signal(cos_v95_sheaf_t *s,
                        const cos_v95_q16_t x[COS_V95_NODES][COS_V95_DIM])
{
    memcpy(s->x, x, sizeof(s->x));
    s->last_energy = cos_v95_energy(s);
}

/* Compute Δ_F x in integer arithmetic.  For each edge e = (u, v) with
 * restriction diagonals d_u, d_v ∈ {−1,+1}^D, the edge error in stalk
 * F(e) is
 *     ε = d_u · x_u − d_v · x_v       (per-dimension product)
 *
 * Its back-projection is
 *     (Δ_F x)_u += d_u · ε
 *     (Δ_F x)_v += −d_v · ε
 *
 * Since d_u·d_u = 1 this matches the unnormalised sheaf Laplacian. */
void cos_v95_laplacian_apply(const cos_v95_sheaf_t *s,
                             cos_v95_q16_t y[COS_V95_NODES][COS_V95_DIM])
{
    for (uint32_t v = 0; v < COS_V95_NODES; ++v)
        for (uint32_t d = 0; d < COS_V95_DIM; ++d)
            y[v][d] = 0;

    for (uint32_t e = 0; e < COS_V95_EDGES; ++e) {
        uint32_t u = s->e_u[e];
        uint32_t v = s->e_v[e];
        for (uint32_t d = 0; d < COS_V95_DIM; ++d) {
            int32_t xu = (s->r_u[e][d] > 0) ?  (int32_t)s->x[u][d]
                                            : -(int32_t)s->x[u][d];
            int32_t xv = (s->r_v[e][d] > 0) ?  (int32_t)s->x[v][d]
                                            : -(int32_t)s->x[v][d];
            int32_t eps = xu - xv;
            int32_t back_u = (s->r_u[e][d] > 0) ?  eps : -eps;
            int32_t back_v = (s->r_v[e][d] > 0) ? -eps :  eps;
            y[u][d] = (cos_v95_q16_t)((int32_t)y[u][d] + back_u);
            y[v][d] = (cos_v95_q16_t)((int32_t)y[v][d] + back_v);
        }
    }
}

/* λ = 1/8 → right-shift by 3. This step size is known-stable on the
 * ring graph whose sheaf-Laplacian spectral radius is ≤ 4. */
void cos_v95_diffuse_step(cos_v95_sheaf_t *s)
{
    cos_v95_q16_t y[COS_V95_NODES][COS_V95_DIM];
    cos_v95_laplacian_apply(s, y);
    for (uint32_t v = 0; v < COS_V95_NODES; ++v) {
        for (uint32_t d = 0; d < COS_V95_DIM; ++d) {
            int32_t step = y[v][d] / 8;
            s->x[v][d] = (cos_v95_q16_t)((int32_t)s->x[v][d] - step);
        }
    }
    int64_t new_energy = cos_v95_energy(s);
    if (s->last_energy != INT64_MAX && new_energy > s->last_energy) {
        ++s->energy_violations;
    }
    s->last_energy = new_energy;
    ++s->steps;
}

int64_t cos_v95_energy(const cos_v95_sheaf_t *s)
{
    cos_v95_q16_t y[COS_V95_NODES][COS_V95_DIM];
    cos_v95_laplacian_apply(s, y);
    int64_t e = 0;
    for (uint32_t v = 0; v < COS_V95_NODES; ++v)
        for (uint32_t d = 0; d < COS_V95_DIM; ++d)
            e += (int64_t)q_abs(y[v][d]);
    return e;
}

uint32_t cos_v95_ok(const cos_v95_sheaf_t *s)
{
    uint32_t sent = (s->sentinel == COS_V95_SENTINEL) ? 1u : 0u;
    uint32_t nonviol = (s->energy_violations == 0u) ? 1u : 0u;
    uint32_t maps_ok = 1u;
    for (uint32_t e = 0; e < COS_V95_EDGES; ++e) {
        for (uint32_t d = 0; d < COS_V95_DIM; ++d) {
            int v_u = s->r_u[e][d];
            int v_v = s->r_v[e][d];
            if (v_u != 1 && v_u != -1) maps_ok = 0u;
            if (v_v != 1 && v_v != -1) maps_ok = 0u;
        }
    }
    return sent & nonviol & maps_ok;
}

uint32_t cos_v95_compose_decision(uint32_t v94_composed_ok, uint32_t v95_ok)
{
    return v94_composed_ok & v95_ok;
}
