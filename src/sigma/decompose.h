/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v34: Dirichlet / evidence-style decomposition of logit uncertainty (lab).
 *
 * Shipped math: treat unnormalized exp(logits) as Dirichlet evidence mass so
 * Σα responds to sharpness (unlike softmax·N with fixed mass). Literature ties
 * and limitations are documented in docs/SIGMA_VS_FIELD.md (I-tier).
 */
#ifndef CREATION_OS_SIGMA_DECOMPOSE_H
#define CREATION_OS_SIGMA_DECOMPOSE_H

typedef struct {
    float total;
    float aleatoric;
    float epistemic;
} sigma_decomposed_t;

/**
 * Dirichlet-style decomposition with α_i ∝ exp(logits[i] − max).
 * K = vocab size n, S = Σα.
 * aleatoric = K/S, epistemic = K(K−1)/(S(S+1)) (same functional shape as LogTokU-style ratios).
 */
void sigma_decompose_dirichlet_evidence(const float *logits, int n, sigma_decomposed_t *out);

/** Reference variant: α_i = softmax_i · mass, S = mass (constant if mass fixed). */
void sigma_decompose_softmax_mass(const float *logits, int n, float mass, sigma_decomposed_t *out);

#endif /* CREATION_OS_SIGMA_DECOMPOSE_H */
