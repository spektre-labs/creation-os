/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v100 — σ-Hyena (sub-quadratic long-range operator: gated
 * exponentially-decayed causal convolution, Hyena/Monarch family).
 *
 * ------------------------------------------------------------------
 *  What v100 is
 * ------------------------------------------------------------------
 *
 * The Hyena operator (Poli et al., 2023; Monarch-Mixer, 2023–26)
 * replaces attention's O(L²) similarity matrix with an O(L log L)
 * data-controlled *long convolution with multiplicative gating*:
 *
 *     y_t = g_t · (h * x)_t        (element-wise gate · causal conv)
 *
 * Here h is an implicit filter parameterised by an exponentially
 * decaying envelope
 *
 *     h_k = sign_k · γ^k ,   k = 0 … L-1
 *
 * with sign_k ∈ {−1, +1} from the initialisation seed, and γ < 1
 * chosen in Q16.16.  g_t is a per-position gate in [0, Q1] learned in
 * parallel; here it is generated deterministically from the seed so
 * the kernel stays RNG-free.  The filter decays exponentially so the
 * operator has effective receptive field ≈ −log(Q1/ε) / log γ.
 *
 * v100 exposes the operator directly as a causal convolution
 *
 *     y_t = g_t · Σ_{k=0}^{t} h_k · x_{t-k}
 *
 * and verifies the three canonical Hyena properties:
 *
 *   1. Causality    y_t depends only on x_{0..t}
 *   2. Linearity    y(α·x + β·x') = α·y(x) + β·y(x')  (pre-gate)
 *   3. Shift-covariance of the filter (stationary kernel)
 *
 * Invariants checked at runtime:
 *   • filter decay monotone: |h_k+1| ≤ |h_k|
 *   • gate bounded: 0 ≤ g_t ≤ Q1
 *   • causality verified by zeroing x_{≥t0} and checking y_{<t0}
 *   • linearity within saturating clamp tolerance
 *   • sentinel intact
 *
 * ------------------------------------------------------------------
 *  Composed 40-bit branchless decision (extends v99)
 * ------------------------------------------------------------------
 *
 *     cos_v100_compose_decision(v99_composed_ok, v100_ok)
 *         = v99_composed_ok & v100_ok
 *
 * Reaching 40 composed kernels is the σ-Hyena landmark — each
 * extension of the AGI plane is still exactly one single AND gate.
 */

#ifndef COS_V100_HYENA_H
#define COS_V100_HYENA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V100_SENTINEL    0xA0C0FEE0u            /* "100-Hyena" */
#define COS_V100_L           32u                    /* sequence length */
#define COS_V100_Q1          65536                  /* 1.0 Q16.16 */
#define COS_V100_GAMMA       62259                  /* γ = 0.95 Q16.16 */
#define COS_V100_CLAMP       (128 * 65536)          /* |y| ≤ 128.0 */

typedef int32_t cos_v100_q16_t;

typedef struct {
    cos_v100_q16_t h[COS_V100_L];                   /* filter, Q16.16 */
    cos_v100_q16_t g[COS_V100_L];                   /* gate, ∈ [0, Q1] */
    uint32_t       clamp_violations;
    uint32_t       sentinel;
} cos_v100_hyena_t;

void cos_v100_init(cos_v100_hyena_t *h, uint64_t seed);

/* Causal gated long conv: y_t = g_t · Σ_{k=0}^{t} h_k · x_{t-k}.     */
void cos_v100_apply(cos_v100_hyena_t *h,
                    const cos_v100_q16_t x[COS_V100_L],
                    cos_v100_q16_t       y[COS_V100_L]);

/* Pre-gate convolution: y_t = Σ_{k=0}^{t} h_k · x_{t-k}.  Exposed for
 * testing the linearity property independently of the gate. */
void cos_v100_conv_only(const cos_v100_hyena_t *h,
                        const cos_v100_q16_t x[COS_V100_L],
                        cos_v100_q16_t       y[COS_V100_L]);

uint32_t cos_v100_ok(const cos_v100_hyena_t *h);
uint32_t cos_v100_compose_decision(uint32_t v99_composed_ok,
                                   uint32_t v100_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V100_HYENA_H */
