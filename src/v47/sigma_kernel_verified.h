/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v47: ACSL-documented σ-kernel surface (Frama-C/WP target).
 *
 * Semantics match the shipped Dirichlet-evidence path in `src/sigma/decompose.c`
 * (max-subtracted exp accumulation in float; logits, not probabilities).
 */
#ifndef CREATION_OS_V47_SIGMA_KERNEL_VERIFIED_H
#define CREATION_OS_V47_SIGMA_KERNEL_VERIFIED_H

#include "../sigma/decompose.h"

/** Maximum vocabulary slice size this lab kernel is parameterized for (tier / tooling bound). */
#define V47_VERIFIED_MAX_N 131072

float v47_verified_max_logit(const float *logits, int n);

float v47_verified_softmax_entropy(const float *logits, int n);

/** Normalized top-1 minus top-2 margin on logits, clamped to [0,1] (display / contract helper). */
float v47_verified_top_margin01(const float *logits, int n);

void v47_verified_dirichlet_decompose(const float *logits, int n, sigma_decomposed_t *out);

#endif /* CREATION_OS_V47_SIGMA_KERNEL_VERIFIED_H */
