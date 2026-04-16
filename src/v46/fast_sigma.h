/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v46 lab: σ from an existing logits row — no extra model forward pass (only O(V) scans).
 *
 * Input is **pre-softmax logits** (same contract as `sigma_decompose_dirichlet_evidence`).
 * `total` may be replaced by a v46 **composite** scalar for dashboards; `epistemic` / `aleatoric`
 * remain Dirichlet-evidence decomposition.
 */
#ifndef CREATION_OS_V46_FAST_SIGMA_H
#define CREATION_OS_V46_FAST_SIGMA_H

#include "../sigma/decompose.h"

typedef struct {
    float token_latency_ms;
    float sigma_overhead_ms;
    float sigma_overhead_pct;
} v46_latency_profile_t;

/** Dirichlet epistemic/aleatoric + softmax entropy + top-margin blend into `out->total` (see .c). */
void v46_fast_sigma_from_logits(const float *logits, int vocab_size, sigma_decomposed_t *out);

float v46_softmax_entropy_from_logits(const float *logits, int n);
float v46_top1_minus_top2_margin(const float *logits, int n);

void v46_latency_profile_reset(v46_latency_profile_t *p);
void v46_latency_profile_add(v46_latency_profile_t *p, float token_latency_ms, float sigma_overhead_ms);

#endif /* CREATION_OS_V46_FAST_SIGMA_H */
