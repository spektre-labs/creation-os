/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v87 — σ-SAE (mechanistic-interpretability plane).
 *
 * ------------------------------------------------------------------
 *  What v87 is
 * ------------------------------------------------------------------
 *
 * v87 implements a Top-K Sparse Autoencoder (Makhzani 2014; Bricken
 * et al 2023; Anthropic circuit-tracing 2024-2026).  Given a
 * transformer residual-stream activation h ∈ Q16.16^D, the SAE
 * decomposes it into a D_feat-wide feature dictionary by:
 *
 *     a   = ReLU(W_enc · h + b_enc)
 *     a_k = TopK(a)            (keep the k largest, zero the rest)
 *     ĥ   = W_dec · a_k + b_dec
 *
 * Interpretability signals:
 *
 *   - L0   = popcount(a_k != 0) — exact by construction (= k)
 *   - L2   = L1(ĥ - h)          — reconstruction quality
 *   - Attribution: cos_v87_attribute returns the top-K feature-ids
 *     that carry a given direction, enabling *feature-level circuit
 *     tracing* (Marks et al 2024; Anthropic circuit-tracer 2025).
 *
 * Six primitives:
 *
 *   1. cos_v87_sae_init        — load a deterministic reference SAE.
 *   2. cos_v87_encode          — h → sparse activations a_k.
 *   3. cos_v87_decode          — a_k → reconstructed ĥ.
 *   4. cos_v87_recon_error     — L1(ĥ, h).
 *   5. cos_v87_attribute       — return the k feature-ids ranked by
 *                                |a_i| for a given activation.
 *   6. cos_v87_ablate          — zero a named feature-id and
 *                                re-decode; used for causal tracing
 *                                (ablate & measure downstream delta).
 *
 * ------------------------------------------------------------------
 *  Composed 27-bit branchless decision (extends v86)
 * ------------------------------------------------------------------
 *
 *     cos_v87_compose_decision(v86_composed_ok, v87_ok)
 *         = v86_composed_ok & v87_ok
 *
 * `v87_ok = 1` iff the sentinel is intact, the L0 sparsity equals
 * the declared K (i.e. the Top-K rule is exact), and the last
 * reconstruction error is within the declared budget.
 */

#ifndef COS_V87_SAE_H
#define COS_V87_SAE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V87_SENTINEL   0x5A45FEA7u
#define COS_V87_DIM         16u      /* residual-stream width */
#define COS_V87_FEAT        64u      /* feature-dictionary size */
#define COS_V87_TOPK        4u       /* exact K for Top-K SAE */

typedef int32_t cos_v87_q16_t;

typedef struct {
    cos_v87_q16_t W_enc[COS_V87_FEAT][COS_V87_DIM];
    cos_v87_q16_t b_enc[COS_V87_FEAT];
    cos_v87_q16_t W_dec[COS_V87_DIM][COS_V87_FEAT];
    cos_v87_q16_t b_dec[COS_V87_DIM];

    uint32_t      recon_budget;       /* Q16.16 */
    cos_v87_q16_t last_recon_err;
    uint32_t      last_l0;
    uint32_t      invariant_violations;
    uint32_t      sentinel;
} cos_v87_sae_t;

/* Init: deterministic SAE from seed. */
void cos_v87_sae_init(cos_v87_sae_t *s, uint64_t seed);

/* Forward: h ∈ R^D -> sparse activations a_k ∈ R^F (non-top-K zeroed)
 * and the ids / magnitudes of the K retained features (sorted by
 * decreasing |a_i|). */
void cos_v87_encode(cos_v87_sae_t *s,
                    const cos_v87_q16_t h[COS_V87_DIM],
                    cos_v87_q16_t        a[COS_V87_FEAT],
                    uint32_t             topk_ids[COS_V87_TOPK]);

/* Reconstruction: a -> ĥ. */
void cos_v87_decode(const cos_v87_sae_t *s,
                    const cos_v87_q16_t a  [COS_V87_FEAT],
                    cos_v87_q16_t        hh[COS_V87_DIM]);

/* L1 reconstruction error per-dim average. */
cos_v87_q16_t cos_v87_recon_error(const cos_v87_q16_t hh[COS_V87_DIM],
                                  const cos_v87_q16_t h [COS_V87_DIM]);

/* Feature attribution: returns the top-K feature ids for h. */
void cos_v87_attribute(cos_v87_sae_t *s,
                       const cos_v87_q16_t h[COS_V87_DIM],
                       uint32_t            topk_ids[COS_V87_TOPK]);

/* Ablate feature `fid`: set its activation to 0, re-decode. Returns
 * the L1 delta between ablated and full reconstruction. */
cos_v87_q16_t cos_v87_ablate(cos_v87_sae_t *s,
                             const cos_v87_q16_t h[COS_V87_DIM],
                             uint32_t fid);

/* Gate + compose. */
uint32_t cos_v87_ok(const cos_v87_sae_t *s);
uint32_t cos_v87_compose_decision(uint32_t v86_composed_ok,
                                  uint32_t v87_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V87_SAE_H */
