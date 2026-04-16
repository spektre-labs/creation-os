/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v43 lab: σ-weighted knowledge distillation (token-level weight on KL).
 *
 * Evidence class: portable math + policy hooks — not a trained KD loop in-tree.
 */
#ifndef CREATION_OS_V43_SIGMA_DISTILL_H
#define CREATION_OS_V43_SIGMA_DISTILL_H

#include "../sigma/decompose.h"

/** Per-token σ policy: skip, learn hard, learn soft, or anti-transfer (negative). */
float v43_sigma_weight(const sigma_decomposed_t *teacher_sigma);

/**
 * KL(p || q) with p = softmax(teacher_logits / T), q = softmax(student_logits / T).
 * Numerically stable (max-subtraction). Undefined if n < 1.
 */
float v43_kl_forward_pt_qs(const float *teacher_logits, const float *student_logits, int n, float temperature);

/** Reverse direction KL(q || p) for “push student away from teacher” experiments. */
float v43_kl_reverse_qt_ps(const float *teacher_logits, const float *student_logits, int n, float temperature);

/**
 * σ-weighted distillation contribution for one token position.
 * w == 0 → 0 (skip). w < 0 → w * reverse_KL (anti-hallucination / repulsion term).
 */
float v43_sigma_distillation_loss(const float *student_logits, const float *teacher_logits,
                                  const sigma_decomposed_t *teacher_sigma, int vocab_size, float temperature);

#endif /* CREATION_OS_V43_SIGMA_DISTILL_H */
