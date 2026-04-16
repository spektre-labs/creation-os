/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v55 EARS — Efficient Adaptive Rejection Sampling for speculative
 * decoding (Sun, Mao, Xu, Chen — arXiv:2512.13194, Dec 2025).
 *
 * Standard speculative-decoding rejection sampling accepts a draft
 * token x with probability min(1, P_target(x) / P_draft(x)).  In
 * high-uncertainty regimes the "random rejection" problem hurts
 * throughput: plausible candidates are rejected by bad luck.
 *
 * EARS relaxes the acceptance criterion by a tolerance proportional
 * to the target's own uncertainty (1 − max P_target):
 *
 *     accept  iff  r < min(1, P_target(x) / P_draft(x)
 *                            + α · σ_knowledge)
 *
 * Reported in the paper: up to +18.12 % throughput, −0.84 %
 * accuracy (GSM8K).  Branchless, scaffold-tier C, SIMD batch helper.
 *
 * This module does **no** draft / target inference — the caller
 * brings the two probability scalars and σ_knowledge (from
 * v55_sigma3_t.sigma_knowledge or any equivalent measurement).
 */
#ifndef CREATION_OS_V55_EARS_H
#define CREATION_OS_V55_EARS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float alpha;            /* tolerance scale, e.g. 0.25 (α=0 ⇒ exact spec-decode) */
    float max_threshold;    /* typically 1.0 (spec-decode invariant cap) */
} v55_ears_params_t;

/* Scalar acceptance.  Branchless-friendly: no `if` on the hot path,
 * only FP compares + min.  Returns 1 if accept, 0 if reject. */
int v55_ears_accept(const v55_ears_params_t *p,
                    float p_target, float p_draft,
                    float rnd, float sigma_knowledge);

/* Compute the relaxed threshold (exposed for calibration / logging). */
float v55_ears_threshold(const v55_ears_params_t *p,
                         float p_target, float p_draft,
                         float sigma_knowledge);

/* Batch helper.  Writes 0/1 into accept_mask.  Scalar loop — compilers
 * auto-vectorize the body on aarch64/x86 (all ops branchless). */
void v55_ears_accept_batch(const v55_ears_params_t *p,
                           const float *p_target,
                           const float *p_draft,
                           const float *rnd,
                           const float *sigma_knowledge,
                           int *accept_mask,
                           int n);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V55_EARS_H */
