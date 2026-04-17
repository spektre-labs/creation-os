/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v90 — σ-Hierarchical implementation.
 * Three-level predictive-coding stack with SHAKE-256 receipts.
 */

#include "hierarchical.h"
#include "lattice.h"        /* v81 SHAKE-256 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 *  Helpers                                                           *
 * ------------------------------------------------------------------ */

static inline cos_v90_q16_t q_abs(cos_v90_q16_t x)
{
    uint32_t ux = (uint32_t)x;
    uint32_t m  = (uint32_t)(x >> 31);
    return (cos_v90_q16_t)((ux ^ m) - m);
}

static inline cos_v90_q16_t q_mul(cos_v90_q16_t a, cos_v90_q16_t b)
{
    int64_t p = (int64_t)a * (int64_t)b;
    return (cos_v90_q16_t)(p >> 16);
}

/* Free-energy contribution at one level: F = π · Σ |ε_i|.
 * We use L1 instead of L2 to stay in bounded integer range. */
static cos_v90_q16_t level_free_energy(cos_v90_q16_t pi,
                                       const cos_v90_q16_t err[COS_V90_DIM])
{
    uint32_t acc = 0;
    for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
        acc += (uint32_t)q_abs(err[d]);
    }
    cos_v90_q16_t sum = (cos_v90_q16_t)(acc / COS_V90_DIM);
    return q_mul(pi, sum);
}

/* ------------------------------------------------------------------ *
 *  Init                                                              *
 * ------------------------------------------------------------------ */

void cos_v90_hi_init(cos_v90_hi_t *h, uint64_t seed)
{
    memset(h, 0, sizeof(*h));
    h->sentinel = COS_V90_SENTINEL;
    /* Precisions: L0 is most precise (fast), L2 least (slow). */
    h->pi[0] = (cos_v90_q16_t)(1 << 16);     /* 1.0 */
    h->pi[1] = (cos_v90_q16_t)(1 << 15);     /* 0.5 */
    h->pi[2] = (cos_v90_q16_t)(1 << 14);     /* 0.25 */

    h->free_energy_ceiling = (uint32_t)(8 << 16);   /* 8.0 */

    /* Seed the chain hash. */
    cos_v81_xof_t x;
    uint8_t seed_bytes[8];
    for (uint32_t i = 0; i < 8u; ++i)
        seed_bytes[i] = (uint8_t)(seed >> (8u * i));
    cos_v81_shake256_init(&x);
    cos_v81_shake_absorb(&x, seed_bytes, sizeof(seed_bytes));
    cos_v81_shake_finalize(&x);
    cos_v81_shake_squeeze(&x, h->chain_hash, sizeof(h->chain_hash));
}

/* ------------------------------------------------------------------ *
 *  Step                                                              *
 * ------------------------------------------------------------------ */

cos_v90_q16_t cos_v90_step(cos_v90_hi_t *h,
                           const cos_v90_q16_t o[COS_V90_DIM],
                           const cos_v90_q16_t p[COS_V90_DIM])
{
    if (h->sentinel != COS_V90_SENTINEL) {
        ++h->invariant_violations;
        return INT32_MAX;
    }

    /* 1. Slow prior p drives the top-level posterior. */
    for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
        h->mu[2][d] = p[d];
    }

    /* 2. Top-down cascade: μ_pr_l = μ_l+1 (simple passthrough;
     * a more elaborate RGM would use a level-specific linear map). */
    for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
        h->mu_pr[1][d] = h->mu[2][d];
        h->mu_pr[0][d] = h->mu[1][d];
    }

    /* 3. Observation hits L0: posterior = (precision-weighted) mix of
     *    prior and observation: μ_0 = (π_0·o + o_pr) / (π_0 + 1).
     * For integer arithmetic, we use a right-shift of 1 (average). */
    for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
        int32_t s = (int32_t)(((uint32_t)o[d]) + ((uint32_t)h->mu_pr[0][d]));
        h->mu[0][d] = (cos_v90_q16_t)(s / 2);
        h->err[0][d] = (cos_v90_q16_t)((uint32_t)o[d] - (uint32_t)h->mu_pr[0][d]);
    }

    /* 4. Bottom-up: L1 posterior is averaged with L0 residual.
     *    μ_1 += ε_0 / 2 (arithmetic shift preserves sign). */
    for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
        int32_t update = h->err[0][d] / 2;       /* signed divide */
        h->mu[1][d] = (cos_v90_q16_t)((uint32_t)h->mu_pr[1][d] + (uint32_t)update);
        h->err[1][d] = (cos_v90_q16_t)((uint32_t)h->mu[1][d] - (uint32_t)h->mu_pr[1][d]);
    }

    /* 5. Top level: track residual. */
    for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
        h->err[2][d] = (cos_v90_q16_t)((uint32_t)h->mu[1][d] - (uint32_t)h->mu[2][d]);
    }

    /* 6. Free-energy accumulator. */
    cos_v90_q16_t F = 0;
    for (uint32_t l = 0; l < COS_V90_LEVELS; ++l) {
        F = (cos_v90_q16_t)((uint32_t)F +
                            (uint32_t)level_free_energy(h->pi[l], h->err[l]));
    }
    h->total_free_energy = (cos_v90_q16_t)((uint32_t)h->total_free_energy +
                                           (uint32_t)F);
    h->last_free_energy = F;

    /* 7. SHAKE-256 receipt chain extension. */
    uint8_t obs_bytes[COS_V90_DIM * 4 + 32];
    for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
        int32_t x = o[d];
        obs_bytes[4 * d + 0] = (uint8_t)(x >> 0);
        obs_bytes[4 * d + 1] = (uint8_t)(x >> 8);
        obs_bytes[4 * d + 2] = (uint8_t)(x >> 16);
        obs_bytes[4 * d + 3] = (uint8_t)(x >> 24);
    }
    memcpy(&obs_bytes[COS_V90_DIM * 4], h->chain_hash, 32);
    cos_v81_xof_t x;
    cos_v81_shake256_init(&x);
    cos_v81_shake_absorb(&x, obs_bytes, sizeof(obs_bytes));
    cos_v81_shake_finalize(&x);
    cos_v81_shake_squeeze(&x, h->chain_hash, sizeof(h->chain_hash));

    ++h->steps;
    return F;
}

/* ------------------------------------------------------------------ *
 *  Readouts                                                          *
 * ------------------------------------------------------------------ */

cos_v90_q16_t cos_v90_total_free_energy(const cos_v90_hi_t *h)
{
    return h->total_free_energy;
}

void cos_v90_receipt(const cos_v90_hi_t *h, uint8_t out[32])
{
    memcpy(out, h->chain_hash, 32);
}

/* ------------------------------------------------------------------ *
 *  Schema consolidation                                              *
 * ------------------------------------------------------------------ */

void cos_v90_update_schema(cos_v90_hi_t *h)
{
    /* EMA μ_2 ← μ_2 + (μ_1 - μ_2) >> 3 (τ = 1/8). */
    for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
        int32_t diff = (int32_t)((uint32_t)h->mu[1][d] - (uint32_t)h->mu[2][d]);
        int32_t step = diff / 8;
        h->mu[2][d] = (cos_v90_q16_t)((uint32_t)h->mu[2][d] + (uint32_t)step);
    }
}

void cos_v90_reset_schema(cos_v90_hi_t *h)
{
    for (uint32_t d = 0; d < COS_V90_DIM; ++d) h->mu[2][d] = 0;
    h->total_free_energy = 0;
}

/* ------------------------------------------------------------------ *
 *  Gate + compose                                                    *
 * ------------------------------------------------------------------ */

uint32_t cos_v90_ok(const cos_v90_hi_t *h)
{
    uint32_t sent   = (h->sentinel == COS_V90_SENTINEL)    ? 1u : 0u;
    uint32_t noviol = (h->invariant_violations == 0u)      ? 1u : 0u;
    /* Gate is based on the last step's free energy, not the
     * cumulative sum — the cumulative sum naturally grows with time
     * under any non-zero observation and is tracked for audit only. */
    uint32_t fe_ok  = ((uint32_t)h->last_free_energy <= h->free_energy_ceiling)
                        ? 1u : 0u;
    return sent & noviol & fe_ok;
}

uint32_t cos_v90_compose_decision(uint32_t v89_composed_ok, uint32_t v90_ok)
{
    return v89_composed_ok & v90_ok;
}
