/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v227 σ-Entropy — information-theoretic unification:
 *   entropy decomposition, σ_product vs σ_H, KL-
 *   divergence, mutual information, free-energy.
 *
 *   Shannon entropy H = −Σ p log p is the classic
 *   uncertainty measure.  v104 already showed that
 *   σ_product (v29 geometric mean over 8 channels)
 *   correlates more tightly with empirical error than
 *   entropy alone.  v227 formalises why: σ_product
 *   decomposes H into orthogonal components and
 *   reweights them.
 *
 *   Fixture (v0):
 *     N = 8 token-level distributions p_i ∈ R^K
 *     (K = 5), each with a planted 'true σ' ∈ [0, 1]
 *     used purely as a reference for MSE comparison.
 *     The distributions are parametrised by two
 *     latent sharpness / tail parameters (α, β) so
 *     the four decomposed channels really do pick up
 *     different aspects of the shape.
 *
 *   Per-distribution σ-channels:
 *     σ_H        = H / log K                    — entropy
 *     σ_nEff     = 1 − n_eff / K                 — effective support
 *     σ_tail     = tail mass on k ≥ K/2          — tail mass
 *     σ_top1     = 1 − p_max                     — top-1 margin
 *
 *   σ_product = geometric mean of the four.
 *
 *   Information-theoretic checks:
 *     KL(p || uniform)           = log K − H
 *     σ_free   = KL(p || uniform) / log K        — free energy
 *                = 1 − σ_H, by algebra.
 *     I(X; C)                    = H(X) − H(X|C)
 *     σ_mutual = 1 − I(X; C) / H(X) ∈ [0, 1]
 *
 *   MSE contract:
 *     mse_H       = mean_i (σ_H_i         − σ_true_i)^2
 *     mse_product = mean_i (σ_product_i   − σ_true_i)^2
 *     contract:   mse_product < mse_H
 *     (σ_product actually separates signal from H).
 *
 *   Contracts (v0):
 *     1. N = 8, K = 5.
 *     2. Every σ ∈ [0, 1].
 *     3. Every p_i sums to 1 ± 1e-5 and has p_i[k] ≥ 0.
 *     4. σ_free_i + σ_H_i ≈ 1  (within 1e-4).
 *     5. I(X; C) / H(X) ∈ [0, 1], and σ_mutual ∈ [0,1].
 *     6. mse_product < mse_H  (strict; σ_product wins).
 *     7. FNV-1a chain over p, σ, KL, MI, σ_product, MSE
 *        replays byte-identically.
 *
 *   v227.1 (named): active inference loop (policy that
 *     minimises free energy over time), KL bounds for
 *     model calibration, empirical σ_H vs σ_product
 *     learning-rate schedule, plug-in for v224 tensor
 *     channels.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V227_ENTROPY_H
#define COS_V227_ENTROPY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V227_N          8
#define COS_V227_K          5

typedef struct {
    int   id;
    float p[COS_V227_K];
    float sigma_true;       /* reference target */

    float sigma_H;
    float sigma_nEff;
    float sigma_tail;
    float sigma_top1;
    float sigma_product;    /* geom mean of the 4 */

    float kl_uniform;
    float sigma_free;       /* KL / log K */
    float mi;               /* I(X;C)      */
    float sigma_mutual;     /* 1 − MI/H    */
} cos_v227_item_t;

typedef struct {
    cos_v227_item_t items[COS_V227_N];

    float mse_H;
    float mse_product;

    bool  chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v227_state_t;

void   cos_v227_init(cos_v227_state_t *s, uint64_t seed);
void   cos_v227_build(cos_v227_state_t *s);
void   cos_v227_run(cos_v227_state_t *s);

size_t cos_v227_to_json(const cos_v227_state_t *s,
                         char *buf, size_t cap);

int    cos_v227_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V227_ENTROPY_H */
