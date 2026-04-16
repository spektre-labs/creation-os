/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v43 lab: σ-weighted multi-teacher logit ensemble (same vocab geometry).
 */
#ifndef CREATION_OS_V43_MULTI_TEACHER_H
#define CREATION_OS_V43_MULTI_TEACHER_H

#include "../sigma/decompose.h"

typedef struct {
    const float *logits;
    sigma_decomposed_t sigma;
    const char *name;
} v43_teacher_output_t;

/**
 * Weight teacher t by 1 / (σ_epistemic + 0.1), normalize, write weighted mean into out.
 * Returns 0 on success; nonzero if n_teachers < 1, vocab < 1, null pointers, or all weights ~0.
 * Caller supplies out[vocab_size] (e.g. aligned_alloc(64, ...)).
 */
int v43_ensemble_teacher_logits(const v43_teacher_output_t *teachers, int n_teachers, int vocab_size, float *out);

#endif /* CREATION_OS_V43_MULTI_TEACHER_H */
